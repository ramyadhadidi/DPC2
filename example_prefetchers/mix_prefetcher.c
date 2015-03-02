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
#include "../inc/prefetcher.h"

//**********************************************************************
// Defines
//**********************************************************************

// IP stride
#define IP_TRACKER_COUNT 1024
#define IP_PREFETCH_DEGREE 3

//**********************************************************************
// Instantiates
//**********************************************************************
void l2_prefetcher_next_line(unsigned long long int addr);

//**********************************************************************
// Structs
//**********************************************************************

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
// Main Functions
//**********************************************************************

// This function is called once by the simulator on startup
void l2_prefetcher_initialize(int cpu_num) 
{
  printf("Mix Prefetcher\n");
  
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);
}

// This function is called once for each Mid Level Cache read, and is the entry point for participants' prefetching algorithms
void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  
}

// This function is called when a cache block is filled into the L2, and lets you konw which set and way of the cache the block occupies.
void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr)
{
  
}


//**********************************************************************
// Next Line Prefetcher
//**********************************************************************
/*
	For each input address addr, the next cache line is prefetched, to 
	be filled into the L2.
*/
void l2_prefetcher_next_line(unsigned long long int addr)
{
	// next line prefetcher
	// since addr is a byte address, we >>6 to get the cache line address, +1, and then <<6 it back to a byte address
	// l2_prefetch_line is expecting byte addresses
	l2_prefetch_line(0, addr, ((addr>>6)+1)<<6, FILL_L2);
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