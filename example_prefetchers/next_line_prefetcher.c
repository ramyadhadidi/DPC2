//
// Data Prefetching Championship Simulator 2
// Seth Pugsley, seth.h.pugsley@intel.com
//

/*
  
  This file describes a simple next-line prefetcher.  For each input address addr,
  the next cache line is prefetched, to be filled into the L2.

 */

#include <stdio.h>
#include "../inc/prefetcher.h"

void l2_prefetcher_initialize(int cpu_num)
{
  printf("Next-Line Prefetcher\n");
  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);
}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  // uncomment this line to see all the information available to make prefetch decisions
  //printf("(0x%llx 0x%llx %d %d %d) ", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));

  // next line prefetcher
  // since addr is a byte address, we >>6 to get the cache line address, +1, and then <<6 it back to a byte address
  // l2_prefetch_line is expecting byte addresses
  l2_prefetch_line(0, addr, ((addr>>6)+1)<<6, FILL_L2);
}
