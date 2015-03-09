//
// Data Prefetching Championship Simulator 2
// Ramyad Hadidi, rhadidi@gatech.edu
//

/*
  
  This file describes a mechanism to choose between provided prefetchers
  based on their performance. The prefetchers are: next_line, ip_stride,
  stream and ampm. Individual prefetchers work as their main code. However
  for choosing between different prefetcher, a parallel sandbox method is
  implemented.

  All prefetchers will insert their prefetchs in their own private sandbox.
  On an access, each sandbox will be checked and if there was a hit the score
  of that particular prefetcher will be increamented. For the next evaluation
  period we will choose the current best score prefetcher to perform and 
  evaluation continues.

  Although this method requires a lot more storage and logic, no limitation is
  specified on assignment for this online selecting method. So, I have implemented
  the parallel version O(N). It is possible to implement this in serial. So, only
  one sandbox will be needed. However, the logic and storage for each prefetcher
  will remain the same.

 */

#include <stdio.h>
#include <stdlib.h>
#include "../inc/prefetcher.h"

//**********************************************************************
// Defines
//**********************************************************************
// Sandbox
#define TOTAL_SANDBOX	4
#define FALSE_POSITIVE 10 			// from 1000-11==1.1%
#define SANDBOX_PERIOD 256			// Period in L2 Accesses
#define SANDBOX_SIZE_EACH 256

// Next Line
#define NEXT_PREFETCH_DEGREE 1

// IP stride
#define IP_TRACKER_COUNT 1024
#define IP_PREFETCH_DEGREE 2

// Stream
#define STREAM_DETECTOR_COUNT 64
#define STREAM_WINDOW 16
#define STREAM_PREFETCH_DEGREE 2

// AMPM
#define AMPM_PAGE_COUNT 64
#define AMPM_PREFETCH_DEGREE 2


//**********************************************************************
// Structs & Their utulities
//**********************************************************************

// Sandbox
typedef struct sandbox
{
	// Data
	// Max prefetch degree to work: 10
	unsigned long long int data[SANDBOX_SIZE_EACH*10];

	// Size
	int size;
	int max_size;

	// False Positive of sandbox
	int false_positive;

} sandbox_t;

// Insert a data to sandbox | return 1:success, 0:failure
int sandbox_insert (sandbox_t *sandbox, unsigned long long int addr) {
	(*sandbox).data[(*sandbox).size] = addr;

	int size = ++((*sandbox).size);
	if (size == (*sandbox).max_size)
		return 0;

	return 1;
}

// Test if a data is in sandbox or not | return 1:found, 0:not found
// Flase positive is also implemented
int sandbox_test (sandbox_t* sandbox, unsigned long long int addr) {
	int size = (*sandbox).size;
	int index_data = -1;

	int i;
	for (i=0; i<size; i++) {
		if ((*sandbox).data[i] == addr) {
			index_data = i;
			break;
		}
	}

	if (index_data == -1)
		return 0;

	if (rand() % 1000 > (*sandbox).false_positive)
		return 1;
	else
		return 0;
}

// Resets a sandbox
void sandbox_reset (sandbox_t* sandbox) {
	(*sandbox).size = 0;
}

// ---------------------------------------------------------------------
// IP stride
typedef struct ip_tracker
{
  // the IP we're tracking
  unsigned long long int ip;

  // the last address accessed by this IP
  unsigned long long int last_addr;
  // the stride between the last two addresses accessed by this IP
  long long int last_stride;

  // use LRU to evict old IP trackers
  unsigned long long int lru_cycle;
} ip_tracker_t;

// ---------------------------------------------------------------------
// Stream
typedef struct stream_detector
{
  // which 4 KB page this detector is monitoring
  unsigned long long int page;
  
  // + or - direction for the stream
  int direction;

  // this must reach 2 before prefetches can begin
  int confidence;

  // cache line index within the page where prefetches will be issued
  int pf_index;
} stream_detector_t;

// ---------------------------------------------------------------------
// AMPM
typedef struct ampm_page
{
  // page address
  unsigned long long int page;

  // The access map itself.
  // Each element is set when the corresponding cache line is accessed.
  // The whole structure is analyzed to make prefetching decisions.
  // While this is coded as an integer array, it is used conceptually as a single 64-bit vector.
  int access_map[64];

  // This map represents cache lines in this page that have already been prefetched.
  // We will only prefetch lines that haven't already been either demand accessed or prefetched.
  int pf_map[64];

  // used for page replacement
  unsigned long long int lru;
} ampm_page_t;

//**********************************************************************
// Global Data
//**********************************************************************

// sandboxes
sandbox_t sandboxes[TOTAL_SANDBOX];
sandbox_t sandboxes_old[TOTAL_SANDBOX];
int sandbox_scores[TOTAL_SANDBOX];
int sandbox_period_count;
int active_pref_num;

int index_max = 0;
int max_score = 0;

// IP stride
ip_tracker_t trackers_ip[IP_TRACKER_COUNT];

// Stream
stream_detector_t detectors_stream[STREAM_DETECTOR_COUNT];
int stream_replacement_index;

//AMPM
ampm_page_t ampm_pages[AMPM_PAGE_COUNT];

//**********************************************************************
// Instantiates
//**********************************************************************
void l2_prefetcher_next_line(unsigned long long int addr, sandbox_t *sandbox, int evaluation, int cache_hit);
void l2_prefetcher_initialize_ip_stride();
void l2_prefetcher_ip_stride(unsigned long long int addr, unsigned long long int ip, sandbox_t* sandbox, int evaluation, int cache_hit);
void l2_prefetcher_initialize_stream();
void l2_prefetcher_stream(unsigned long long int addr, sandbox_t *sandbox, int evaluation, int cache_hit);
void l2_prefetcher_initialize_ampm();
void l2_prefetcher_ampm(unsigned long long int addr, sandbox_t *sandbox, int evaluation, int cache_hit);

//**********************************************************************
// Main Functions
//**********************************************************************

// This function is called once by the simulator on startup
void l2_prefetcher_initialize(int cpu_num) 
{
  printf("Mix Prefetcher\n");
  
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);

  //** sandboxes
  int i;
  for (i=0; i<TOTAL_SANDBOX; i++) {
  	sandboxes[i].size = 0;
  	sandboxes[i].false_positive = FALSE_POSITIVE;

  	sandbox_scores[i] = 0;
  	// Each sandbox for a prefetcher max_size is different
  	// based on their degree
  	// +1 because of last cycle
  	switch (i) {
  		// Next Line
  		case 0:	  	
		  	sandboxes[i].max_size = SANDBOX_SIZE_EACH + 1;
		  	break;

		  // IP Stride
		  case 1:
		  	sandboxes[i].max_size = SANDBOX_SIZE_EACH * IP_PREFETCH_DEGREE + 1;
		  	break;

		  // Stream
		  case 2:
		  	sandboxes[i].max_size = SANDBOX_SIZE_EACH * STREAM_PREFETCH_DEGREE + 1;
		  	break;

		  // AMPM
		  case 3:
		  	sandboxes[i].max_size = SANDBOX_SIZE_EACH * AMPM_PREFETCH_DEGREE * 2 + 1;
		  	break;

		  default:
		  	printf("Error Active Prefetcher Number is not listed - Initialization\n");
				exit(1);
  	}
  }


  sandbox_period_count = 0;

  // Choose the first prefetcher as active one
  active_pref_num = rand() % TOTAL_SANDBOX;

  //** Next Line

  //** IP stride
	l2_prefetcher_initialize_ip_stride();

	//** Stream
	l2_prefetcher_initialize_stream();

	//** AMPM
	l2_prefetcher_initialize_ampm();
}

// ---------------------------------------------------------------------
// This function is called once for each Mid Level Cache read, 
// and is the entry point for participants' prefetching algorithms
void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
	
	//** Operate Avtive Prefetcher
	switch (active_pref_num) {
		// Next Line
		case 0:
			l2_prefetcher_next_line(addr, &sandboxes[0], 0, cache_hit);
			break;

		// IP Stride
		case 1:
			l2_prefetcher_ip_stride(addr, ip, &sandboxes[1], 0, cache_hit);
			break;

		// Stream
		case 2:
			l2_prefetcher_stream(addr, &sandboxes[2], 0, cache_hit);
			break;

		case 3:
			l2_prefetcher_ampm(addr, &sandboxes[3], 0, cache_hit);
			break;

		default:
			printf("Error Active Prefetcher Number is not listed\n");
			exit(1);
	}

	//** Check for Hits in Sandboxnboxes
	int i;
	for (i=0; i<TOTAL_SANDBOX; i++) {
		if (sandbox_test(&sandboxes_old[i], addr))
				sandbox_scores[i]++;
	}

	//** Operate Sandboxes Prefetchers
	// since we had executed the active one we don't need to
	// execute it again
  switch (active_pref_num) {
		// Next Line is active
		case 0:
		l2_prefetcher_ip_stride(addr, ip, &sandboxes[1], 1, cache_hit);
		l2_prefetcher_stream(addr, &sandboxes[2], 1, cache_hit);
		l2_prefetcher_ampm(addr, &sandboxes[3], 1, cache_hit);
		break;

		// IP Stride is active
		case 1:
		l2_prefetcher_next_line(addr, &sandboxes[0], 1, cache_hit);
		l2_prefetcher_stream(addr, &sandboxes[2], 1, cache_hit);
		l2_prefetcher_ampm(addr, &sandboxes[3], 1, cache_hit);
		break;

		// Stream is active
		case 2:
		l2_prefetcher_next_line(addr, &sandboxes[0], 1, cache_hit);
		l2_prefetcher_ip_stride(addr, ip, &sandboxes[1], 1, cache_hit);
		l2_prefetcher_ampm(addr, &sandboxes[3], 1, cache_hit);
		break;

		// AMPM is active
		case 3:
		l2_prefetcher_next_line(addr, &sandboxes[0], 1, cache_hit);
		l2_prefetcher_ip_stride(addr, ip, &sandboxes[1], 1, cache_hit);
		l2_prefetcher_stream(addr, &sandboxes[2], 1, cache_hit);
		break;

		default:
		printf("Error Active Prefetcher Number is not listed - Sandbox\n");
		exit(1);
	}

	//** Increase Evaluation Period
	// If period is done decide next active prefetcher - reset scores & sandboxes - reset period
	sandbox_period_count++;
	if (sandbox_period_count == SANDBOX_PERIOD) {
		// decide next active prefetcher
		int i;
		
		for (i=0; i<TOTAL_SANDBOX; i++) {
			//printf("\tscore %d = %d\n", i, sandbox_scores[i]);
			//printf("\tsize %d = %d\n", i, sandboxes_old[i].size);
			if (sandbox_scores[i] > max_score) {
				index_max = i;
				max_score = sandbox_scores[i];
			}
		}

		active_pref_num = index_max;


		//printf("\tactive prefetcher = %d\n", active_pref_num);

		/*
		for (i=0; i<TOTAL_SANDBOX; i++) {
			printf("\tData %d\n:",i);
			int j;
			for (j=0; j<sandboxes[i].size; j++) 
				printf("\t %d:",(int)sandboxes[i].data[j]);
			printf("\n");
		}		
		printf("\n");
		*/

		// copy sandboxes to sandboxes_old
		for (i=0; i<TOTAL_SANDBOX; i++) {
			sandboxes_old[i].size = sandboxes[i].size;
			sandboxes_old[i].false_positive = sandboxes[i].false_positive;
			sandboxes_old[i].max_size = sandboxes[i].max_size;

			int j;
			for (j=0; j<sandboxes_old[i].size; j++) 
				sandboxes_old[i].data[j] = sandboxes[i].data[j]; 
		}


		// reset scores & sandbox
		for (i=0; i<TOTAL_SANDBOX; i++) {
			sandbox_reset(&sandboxes[i]);
	  	sandbox_scores[i] /= 3;
  	}

  	// reset period counter
  	sandbox_period_count = 0;
	}

}

// This function is called when a cache block is filled into the L2, and lets you konw which set and way of the cache the block occupies.
void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr)
{
	// nothing 
}


//**********************************************************************
// Next Line Prefetcher
//**********************************************************************
/*
	For each input address addr, the next cache line is prefetched, to 
	be filled into the L2.
	sandbox_insert 0 = not in evaluation period (insert line to cache & sandbox)
	sandbox_insert 1 = in evaluation
*/
void l2_prefetcher_next_line(unsigned long long int addr, sandbox_t *sandbox, int evaluation, int cache_hit)
{
	// next line prefetcher
	// since addr is a byte address, we >>6 to get the cache line address, +1, and then <<6 it back to a byte address
	// l2_prefetch_line is expecting byte addresses
	unsigned long long int pf_addr = ((addr>>6)+1)<<6;
  int i;
  for (i=0; i<NEXT_PREFETCH_DEGREE; i++) {
  	//if (!cache_hit) {
			if (!sandbox_insert (sandbox, pf_addr)) {
				printf("Error Next Line Insert - Sandbox Full\n");
				exit(1);
			}
	  	if (!evaluation)
	    	l2_prefetch_line(0, addr, pf_addr, FILL_L2);  	
	    pf_addr = ((pf_addr>>6)+1)<<6;
	  //}
  }

}

//**********************************************************************
// IP Stride
//**********************************************************************
/*
	Instruction Pointer-based (Program Counter-based) stride prefetcher.
	The prefetcher detects stride patterns coming from the same IP, and then 
  prefetches additional cache lines.

  Prefetches are issued into the L2 or LLC depending on L2 MSHR occupancy.
*/

void l2_prefetcher_initialize_ip_stride() 
{
	int i;
	for(i=0; i<IP_TRACKER_COUNT; i++)
	  {
	    trackers_ip[i].ip = 0;
	    trackers_ip[i].last_addr = 0;
	    trackers_ip[i].last_stride = 0;
	    trackers_ip[i].lru_cycle = 0;
	  }
}

/*
	sandbox_insert 0 = not in evaluation period (insert line to cache & sandbox)
	sandbox_insert 1 = in evaluation
*/
void l2_prefetcher_ip_stride(unsigned long long int addr, unsigned long long int ip, sandbox_t *sandbox, int evaluation, int cache_hit)
{
  // check trackers for a hit
  int tracker_index = -1;

  int i;
  for(i=0; i<IP_TRACKER_COUNT; i++) {
  	if(trackers_ip[i].ip == ip) {
	  	trackers_ip[i].lru_cycle = get_current_cycle(0);
	  	tracker_index = i;
	  break;
		}
	}

	// this is a new IP that doesn't have a tracker yet, so allocate one
  if(tracker_index == -1) {
  	// Search for oldest one (LRU)
		int lru_index=0;
		unsigned long long int lru_cycle = trackers_ip[lru_index].lru_cycle;
		int i;
		for(i=0; i<IP_TRACKER_COUNT; i++) {
	  	if(trackers_ip[i].lru_cycle < lru_cycle) {
	      lru_index = i;
	      lru_cycle = trackers_ip[lru_index].lru_cycle;
	    }
		}

    tracker_index = lru_index;

		// reset the old tracker
		trackers_ip[tracker_index].ip = ip;
		trackers_ip[tracker_index].last_addr = addr;
		trackers_ip[tracker_index].last_stride = 0;
		trackers_ip[tracker_index].lru_cycle = get_current_cycle(0);

		return;
	}

	// A hit in Trackers:
  // calculate the stride between the current address and the last address
  // this bit appears overly complicated because we're calculating
  // differences between unsigned address variables
  long long int stride = 0;
  if(addr > trackers_ip[tracker_index].last_addr) {
		stride = addr - trackers_ip[tracker_index].last_addr;
	}
  else {
		stride = trackers_ip[tracker_index].last_addr - addr;
		stride *= -1;
	}

  // don't do anything if we somehow saw the same address twice in a row
  if(stride == 0)
		return;

  // only do any prefetching if there's a pattern of seeing the same
  // stride more than once
  if(stride == trackers_ip[tracker_index].last_stride) {
		int i;
		for(i=0; i<IP_PREFETCH_DEGREE; i++) {
			unsigned long long int pf_address = addr + (stride*(i+1));

		  // only issue a prefetch if the prefetch address is in the same 4 KB page 
		  // as the current demand access address
		  if((pf_address>>12) != (addr>>12))
				break;

		  // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
		  //if (evaluation)
			  if (!sandbox_insert (sandbox, pf_address)) {
					printf("Error IP Stride Insert - Sandbox Full\n");
					exit(1);
				}

		  if (!evaluation) {
			  if(get_l2_mshr_occupancy(0) < 8)
			      l2_prefetch_line(0, addr, pf_address, FILL_L2);
			  else
			      l2_prefetch_line(0, addr, pf_address, FILL_LLC);
			}
		}
	}

	// update tracker
  trackers_ip[tracker_index].last_addr = addr;
  trackers_ip[tracker_index].last_stride = stride;
}

//**********************************************************************
// Stream Prefetcher
//**********************************************************************
/*
	Prefetches are issued after a spatial locality is detected, and a 
	stream direction can be determined.

  Prefetches are issued into the L2 or LLC depending on L2 MSHR occupancy.
*/

void l2_prefetcher_initialize_stream()
{
  int i;
  for(i=0; i<STREAM_DETECTOR_COUNT; i++)
    {
      detectors_stream[i].page = 0;
      detectors_stream[i].direction = 0;
      detectors_stream[i].confidence = 0;
      detectors_stream[i].pf_index = -1;
    }

  stream_replacement_index = 0;
}

/*
	sandbox_insert 0 = not in evaluation period (insert line to cache & sandbox)
	sandbox_insert 1 = in evaluation
*/
void l2_prefetcher_stream(unsigned long long int addr, sandbox_t *sandbox, int evaluation, int cache_hit)
{
  unsigned long long int cl_address = addr>>6;
  unsigned long long int page = cl_address>>6;
  int page_offset = cl_address&63;

  //** Detect
  // check for a detector hit
  int detector_index = -1;

  int i;
  for(i=0; i<STREAM_DETECTOR_COUNT; i++) {
      if(detectors_stream[i].page == page) {
	  		detector_index = i;
	  		break;
		}
	}

	// This is a new page that doesn't have a detector yet, so allocate one
	// allocation is based on Least recently allocated
  if(detector_index == -1) { 
      detector_index = stream_replacement_index;
      stream_replacement_index++;
      if(stream_replacement_index >= STREAM_DETECTOR_COUNT)
	  		stream_replacement_index = 0;

      // reset the oldest page
      detectors_stream[detector_index].page = page;
      detectors_stream[detector_index].direction = 0;
      detectors_stream[detector_index].confidence = 0;
      detectors_stream[detector_index].pf_index = page_offset;
	}

  //** Train
  // upward
  if(page_offset > detectors_stream[detector_index].pf_index) {
  	// accesses outside the STREAM_WINDOW do not train the detector
  	if((page_offset-detectors_stream[detector_index].pf_index) < STREAM_WINDOW) {
	  	if(detectors_stream[detector_index].direction == -1)
	    	// previously-set direction was wrong
	     	detectors_stream[detector_index].confidence = 0;
	  	else
	      detectors_stream[detector_index].confidence++;

	  // set the direction to +1
	  detectors_stream[detector_index].direction = 1;
		}
	}
	// downward
  else if(page_offset < detectors_stream[detector_index].pf_index) {
  	// accesses outside the STREAM_WINDOW do not train the detector
  	if((detectors_stream[detector_index].pf_index-page_offset) < STREAM_WINDOW) {
			if(detectors_stream[detector_index].direction == 1)
	    	// previously-set direction was wrong
	    	detectors_stream[detector_index].confidence = 0;
	    else
	      detectors_stream[detector_index].confidence++;

	  // set the direction to -1
    detectors_stream[detector_index].direction = -1;
		}
	}

	//** Prefetch
  // prefetch if confidence is high enough
  if(detectors_stream[detector_index].confidence >= 2)	{
		int i;
		for(i=0; i<STREAM_PREFETCH_DEGREE; i++) {
	  	detectors_stream[detector_index].pf_index += detectors_stream[detector_index].direction;
	  	// Page boundary check
			if((detectors_stream[detector_index].pf_index < 0) || (detectors_stream[detector_index].pf_index > 63))
	      break;

		  // perform prefetches
		  unsigned long long int pf_address = (page<<12)+((detectors_stream[detector_index].pf_index)<<6);
		  
		  // check MSHR occupancy to decide whether to prefetch into the L2 or LLC
		  //if (evaluation)
			  if (!sandbox_insert (sandbox, pf_address)) {
						printf("Error IP Stride Insert - Sandbox Full\n");
						exit(1);
				}

			if (!evaluation) {
			  if(get_l2_mshr_occupancy(0) > 8)
		      l2_prefetch_line(0, addr, pf_address, FILL_LLC);
			  else
		      l2_prefetch_line(0, addr, pf_address, FILL_L2);
		  }
		}
  }

}


//**********************************************************************
// AMPM Prefetcher
//**********************************************************************

/*
  This file describes a prefetcher that resembles a simplified version of the
  Access Map Pattern Matching (AMPM) prefetcher, which won the first 
  Data Prefetching Championship.  The original AMPM prefetcher tracked large
  regions of virtual address space to make prefetching decisions, but this 
  version works only on smaller 4 KB physical pages.
*/

void l2_prefetcher_initialize_ampm()
{
  int i;
  for(i=0; i<AMPM_PAGE_COUNT; i++) {
    ampm_pages[i].page = 0;
    ampm_pages[i].lru = 0;

    int j;
    for(j=0; j<64; j++) {
		  ampm_pages[i].access_map[j] = 0;
		  ampm_pages[i].pf_map[j] = 0;
		}
  }
}

/*
	sandbox_insert 0 = not in evaluation period (insert line to cache & sandbox)
	sandbox_insert 1 = in evaluation
*/
void l2_prefetcher_ampm(unsigned long long int addr, sandbox_t *sandbox, int evaluation, int cache_hit) {
  unsigned long long int cl_address = addr>>6;
  unsigned long long int page = cl_address>>6;
  unsigned long long int page_offset = cl_address&63;

  //** Serching for the Page
  // check to see if we have a page hit
  int page_index = -1;
  int i;
  for(i=0; i<AMPM_PAGE_COUNT; i++) {
    if(ampm_pages[i].page == page) {
		  page_index = i;
  		break;
  	}
	}

	// the page was not found, so we must replace an old page with this new page
  if(page_index == -1) {
    // find the oldest page
    int lru_index = 0;
    unsigned long long int lru_cycle = ampm_pages[lru_index].lru;
    int i;
    for(i=0; i<AMPM_PAGE_COUNT; i++) {
	  	if(ampm_pages[i].lru < lru_cycle) {
	      lru_index = i;
	      lru_cycle = ampm_pages[lru_index].lru;
	    }
		}
		page_index = lru_index;

    // reset the oldest page
    ampm_pages[page_index].page = page;
    for(i=0; i<64; i++) {
		  ampm_pages[page_index].access_map[i] = 0;
		  ampm_pages[page_index].pf_map[i] = 0;
		}
	}

  // update LRU
  ampm_pages[page_index].lru = get_current_cycle(0);

  // mark the access map
  ampm_pages[page_index].access_map[page_offset] = 1;

  //** Prefetching
  // positive prefetching
  int count_prefetches = 0;
  for(i=1; i<=16; i++) {
	  int check_index1 = page_offset - i;
	  int check_index2 = page_offset - 2*i;
	  int pf_index = page_offset + i;
		
		if(check_index2 < 0)
	 		break;

    if(pf_index > 63)
	  	break;

		if(count_prefetches >= AMPM_PREFETCH_DEGREE)
	  	break;

    if(ampm_pages[page_index].access_map[pf_index] == 1)
	  	continue;

    if(ampm_pages[page_index].pf_map[pf_index] == 1)
	  	continue;

		if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1)) {
		  // we found the stride repeated twice, so issue a prefetch
		  unsigned long long int pf_address = (page<<12)+(pf_index<<6);

		  //if (evaluation)
				if (!sandbox_insert (sandbox, pf_address)) {
							printf("Error AMPM Insert - Sandbox Full\n");
							exit(1);
				}

			if (!evaluation) {
			  if(get_l2_mshr_occupancy(0) < 8)
			  	l2_prefetch_line(0, addr, pf_address, FILL_L2);
			  else
					l2_prefetch_line(0, addr, pf_address, FILL_LLC);	     
			} 

		  // mark the prefetched line so we don't prefetch it again
		  ampm_pages[page_index].pf_map[pf_index] = 1;
		  count_prefetches++;
		}
	}

  // negative prefetching
  count_prefetches = 0;
  for(i=1; i<=16; i++) {
		int check_index1 = page_offset + i;
		int check_index2 = page_offset + 2*i;
		int pf_index = page_offset - i;

		if(check_index2 > 63)
	  	break;

    if(pf_index < 0)
	  	break;

    if(count_prefetches >= AMPM_PREFETCH_DEGREE)
	  	break;

    if(ampm_pages[page_index].access_map[pf_index] == 1)
	  	continue;

    if(ampm_pages[page_index].pf_map[pf_index] == 1)
	  	continue;

    if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1)) {
	  	unsigned long long int pf_address = (page<<12)+(pf_index<<6);

	  	//if (evaluation)
				if (!sandbox_insert (sandbox, pf_address)) {
					printf("Error AMPM Insert - Sandbox Full\n");
					exit(1);
				}

		  if (!evaluation) {
			  if(get_l2_mshr_occupancy(0) < 12)
					l2_prefetch_line(0, addr, pf_address, FILL_L2);
			  else
					l2_prefetch_line(0, addr, pf_address, FILL_LLC);
			}

		  // mark the prefetched line so we don't prefetch it again
		  ampm_pages[page_index].pf_map[pf_index] = 1;
		  count_prefetches++;
		}
	}

}