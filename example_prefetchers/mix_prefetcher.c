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
#define TOTAL_SANDBOX	2
#define FALSE_POSITIVE 10 			// from 1000-11==1.1%
#define SANDBOX_PERIOD 256			// Period in L2 Accesses
#define SANDBOX_SIZE_EACH 256

// IP stride
#define IP_TRACKER_COUNT 1024
#define IP_PREFETCH_DEGREE 3


//**********************************************************************
// Structs & Their utulities
//**********************************************************************

// Sandbox
typedef struct sandbox
{
	// Data
	unsigned long long int data[SANDBOX_SIZE_EACH];

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

//**********************************************************************
// Global Data
//**********************************************************************

// sandboxes
sandbox_t sandboxes[TOTAL_SANDBOX];
int sandbox_scores[TOTAL_SANDBOX];
int sandbox_period_count;
int active_pref_num;

// IP stride
ip_tracker_t trackers_ip[IP_TRACKER_COUNT];

//**********************************************************************
// Instantiates
//**********************************************************************
void l2_prefetcher_next_line(unsigned long long int addr, sandbox_t *sandbox, int evaluation);
void l2_prefetcher_initialize_ip_stride();
void l2_prefetcher_ip_stride(unsigned long long int addr, unsigned long long int ip, sandbox_t* sandbox, int evaluation);

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
  	switch (i){
  		// Next Line
  		case 0:	  	
		  	sandboxes[i].max_size = SANDBOX_SIZE_EACH + 1;
		  	break;

		  // IP Stride
		  case 1:
		  	sandboxes[i].max_size = SANDBOX_SIZE_EACH * IP_PREFETCH_DEGREE + 1;
		  	break;
  	}
  }


  sandbox_period_count = 0;

  // Choose the first prefetcher as active one
  active_pref_num = 0;

  //** IP stride
	l2_prefetcher_initialize_ip_stride();
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
			l2_prefetcher_next_line(addr, &sandboxes[0], 0);
			break;

		// IP Stride
		case 1:
			l2_prefetcher_initialize_ip_stride(addr, &sandboxes[1], 0);
			break;

		default:
			printf("Error Active Prefetcher Number is not listed\n");
			exit(1);
	}

	//** Increase Evaluation Period
	// If period is done decide next active prefetcher - reset scores & sandboxes - reset period
	sandbox_period_count++;
	if (sandbox_period_count == SANDBOX_PERIOD) {
		// decide next active prefetcher
		int i;
		int max_score = -1;
		int index_max = -1;
		for (i=0; i<TOTAL_SANDBOX; i++) {
			printf("\tscore %d = %d\n", i, sandbox_scores[i]);
			if (sandbox_scores[i] > max_score) {
				index_max = i;
				max_score = sandbox_scores[i];
			}
		}

		active_pref_num = index_max;

		printf("\tactive prefetcher = %d\n", active_pref_num);

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
		

		// reset scores & sandbox
		for (i=0; i<TOTAL_SANDBOX; i++) {
	  	sandboxes[i].size = 0;
	  	sandboxes[i].false_positive = FALSE_POSITIVE;

	  	sandbox_scores[i] = 0;
	  	// Each sandbox for a prefetcher max_size is different
	  	// based on their degree
	  	// +1 because of last cycle
	  	switch (i){
	  		// Next Line
	  		case 0:	  	
			  	sandboxes[i].max_size = SANDBOX_SIZE_EACH + 1;
			  	break;

			  // IP Stride
			  case 1:
			  	sandboxes[i].max_size = SANDBOX_SIZE_EACH * IP_PREFETCH_DEGREE + 1;
			  	break;
	  	}
  	}

  	// reset period counter
  	sandbox_period_count = 0;
	}

	//** Check for Hits in Sandboxes
	int i;
	for (i=0; i<TOTAL_SANDBOX; i++) {
		if (sandbox_test(&sandboxes[i], addr))
			sandbox_scores[i]++;
	}

	//** Operate Sandboxes Prefetchers
  l2_prefetcher_next_line(addr, &sandboxes[0], 1);
  l2_prefetcher_ip_stride(addr, ip, &sandboxes[1], 1);

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
void l2_prefetcher_next_line(unsigned long long int addr, sandbox_t *sandbox, int evaluation)
{
	// next line prefetcher
	// since addr is a byte address, we >>6 to get the cache line address, +1, and then <<6 it back to a byte address
	// l2_prefetch_line is expecting byte addresses
	unsigned long long int pref_addr = ((addr>>6)+1)<<6;
	if (evaluation)
		if (!sandbox_insert (sandbox, pref_addr)) {
			printf("Error Next Line Insert - Sandbox Full\n");
			exit(1);
		}
	if (!evaluation)
		l2_prefetch_line(0, addr, pref_addr, FILL_L2);

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
void l2_prefetcher_ip_stride(unsigned long long int addr, unsigned long long int ip, sandbox_t *sandbox, int evaluation)
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
		  if (evaluation) {
			  if (!sandbox_insert (sandbox, pf_address)) {
					printf("Error IP Stride Insert - Sandbox Full\n");
					exit(1);
				}
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