//
// Data Prefetching Championship Simulator 2
// Seth Pugsley, seth.h.pugsley@intel.com
//

/*

  This file describes an Instruction Pointer-based (Program Counter-based) stride prefetcher.  
  The prefetcher detects stride patterns coming from the same IP, and then 
  prefetches additional cache lines.

  Prefetches are issued into the L2 or LLC depending on L2 MSHR occupancy.

 */

#include <stdio.h>
#include "../inc/prefetcher.h"

#define IP_TRACKER_COUNT 1024
#define PREFETCH_DEGREE 3

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

ip_tracker_t trackers[IP_TRACKER_COUNT];

void l2_prefetcher_initialize(int cpu_num)
{
  printf("IP-based Stride Prefetcher\n");
  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);

  int i;
  for(i=0; i<IP_TRACKER_COUNT; i++)
    {
      trackers[i].ip = 0;
      trackers[i].last_addr = 0;
      trackers[i].last_stride = 0;
      trackers[i].lru_cycle = 0;
    }
}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  // uncomment this line to see all the information available to make prefetch decisions
  //printf("(%lld 0x%llx 0x%llx %d %d %d) ", get_current_cycle(0), addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));

  // check for a tracker hit
  int tracker_index = -1;

  int i;
  for(i=0; i<IP_TRACKER_COUNT; i++)
    {
      if(trackers[i].ip == ip)
	{
	  trackers[i].lru_cycle = get_current_cycle(0);
	  tracker_index = i;
	  break;
	}
    }

  if(tracker_index == -1)
    {
      // this is a new IP that doesn't have a tracker yet, so allocate one
      int lru_index=0;
      unsigned long long int lru_cycle = trackers[lru_index].lru_cycle;
      int i;
      for(i=0; i<IP_TRACKER_COUNT; i++)
	{
	  if(trackers[i].lru_cycle < lru_cycle)
	    {
	      lru_index = i;
	      lru_cycle = trackers[lru_index].lru_cycle;
	    }
	}

      tracker_index = lru_index;

      // reset the old tracker
      trackers[tracker_index].ip = ip;
      trackers[tracker_index].last_addr = addr;
      trackers[tracker_index].last_stride = 0;
      trackers[tracker_index].lru_cycle = get_current_cycle(0);

      return;
    }

  // calculate the stride between the current address and the last address
  // this bit appears overly complicated because we're calculating
  // differences between unsigned address variables
  long long int stride = 0;
  if(addr > trackers[tracker_index].last_addr)
    {
      stride = addr - trackers[tracker_index].last_addr;
    }
  else
    {
      stride = trackers[tracker_index].last_addr - addr;
      stride *= -1;
    }

  // don't do anything if we somehow saw the same address twice in a row
  if(stride == 0)
    {
      return;
    }

  // only do any prefetching if there's a pattern of seeing the same
  // stride more than once
  if(stride == trackers[tracker_index].last_stride)
    {
      // do some prefetching
      int i;
      for(i=0; i<PREFETCH_DEGREE; i++)
	{
	  unsigned long long int pf_address = addr + (stride*(i+1));

	  // only issue a prefetch if the prefetch address is in the same 4 KB page 
	  // as the current demand access address
	  if((pf_address>>12) != (addr>>12))
	    {
	      break;
	    }

	  // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
	  if(get_l2_mshr_occupancy(0) < 8)
	    {
	      l2_prefetch_line(0, addr, pf_address, FILL_L2);
	    }
	  else
	    {
	      l2_prefetch_line(0, addr, pf_address, FILL_LLC);
	    }
	  
	}
    }

  trackers[tracker_index].last_addr = addr;
  trackers[tracker_index].last_stride = stride;
}
