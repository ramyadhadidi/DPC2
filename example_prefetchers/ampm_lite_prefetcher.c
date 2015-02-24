//
// Data Prefetching Championship Simulator 2
// Seth Pugsley, seth.h.pugsley@intel.com
//

/*

  This file describes a prefetcher that resembles a simplified version of the
  Access Map Pattern Matching (AMPM) prefetcher, which won the first 
  Data Prefetching Championship.  The original AMPM prefetcher tracked large
  regions of virtual address space to make prefetching decisions, but this 
  version works only on smaller 4 KB physical pages.

 */

#include <stdio.h>
#include "../inc/prefetcher.h"

#define AMPM_PAGE_COUNT 64
#define PREFETCH_DEGREE 3

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

ampm_page_t ampm_pages[AMPM_PAGE_COUNT];

void l2_prefetcher_initialize(int cpu_num)
{
  printf("AMPM Lite Prefetcher\n");
  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);

  int i;
  for(i=0; i<AMPM_PAGE_COUNT; i++)
    {
      ampm_pages[i].page = 0;
      ampm_pages[i].lru = 0;

      int j;
      for(j=0; j<64; j++)
	{
	  ampm_pages[i].access_map[j] = 0;
	  ampm_pages[i].pf_map[j] = 0;
	}
    }
}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  // uncomment this line to see all the information available to make prefetch decisions
  //printf("(0x%llx 0x%llx %d %d %d) ", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));

  unsigned long long int cl_address = addr>>6;
  unsigned long long int page = cl_address>>6;
  unsigned long long int page_offset = cl_address&63;

  // check to see if we have a page hit
  int page_index = -1;
  int i;
  for(i=0; i<AMPM_PAGE_COUNT; i++)
    {
      if(ampm_pages[i].page == page)
	{
	  page_index = i;
	  break;
	}
    }

  if(page_index == -1)
    {
      // the page was not found, so we must replace an old page with this new page

      // find the oldest page
      int lru_index = 0;
      unsigned long long int lru_cycle = ampm_pages[lru_index].lru;
      int i;
      for(i=0; i<AMPM_PAGE_COUNT; i++)
	{
	  if(ampm_pages[i].lru < lru_cycle)
	    {
	      lru_index = i;
	      lru_cycle = ampm_pages[lru_index].lru;
	    }
	}
      page_index = lru_index;

      // reset the oldest page
      ampm_pages[page_index].page = page;
      for(i=0; i<64; i++)
	{
	  ampm_pages[page_index].access_map[i] = 0;
	  ampm_pages[page_index].pf_map[i] = 0;
	}
    }

  // update LRU
  ampm_pages[page_index].lru = get_current_cycle(0);

  // mark the access map
  ampm_pages[page_index].access_map[page_offset] = 1;

  // positive prefetching
  int count_prefetches = 0;
  for(i=1; i<=16; i++)
    {
      int check_index1 = page_offset - i;
      int check_index2 = page_offset - 2*i;
      int pf_index = page_offset + i;

      if(check_index2 < 0)
	{
	  break;
	}

      if(pf_index > 63)
	{
	  break;
	}

      if(count_prefetches >= PREFETCH_DEGREE)
	{
	  break;
	}

      if(ampm_pages[page_index].access_map[pf_index] == 1)
	{
	  // don't prefetch something that's already been demand accessed
	  continue;
	}

      if(ampm_pages[page_index].pf_map[pf_index] == 1)
	{
	  // don't prefetch something that's alrady been prefetched
	  continue;
	}

      if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1))
	{
	  // we found the stride repeated twice, so issue a prefetch

	  unsigned long long int pf_address = (page<<12)+(pf_index<<6);

	  // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
	  if(get_l2_mshr_occupancy(0) < 8)
	    {
	      l2_prefetch_line(0, addr, pf_address, FILL_L2);
	    }
	  else
	    {
	      l2_prefetch_line(0, addr, pf_address, FILL_LLC);	      
	    }

	  // mark the prefetched line so we don't prefetch it again
	  ampm_pages[page_index].pf_map[pf_index] = 1;
	  count_prefetches++;
	}
    }

  // negative prefetching
  count_prefetches = 0;
  for(i=1; i<=16; i++)
    {
      int check_index1 = page_offset + i;
      int check_index2 = page_offset + 2*i;
      int pf_index = page_offset - i;

      if(check_index2 > 63)
	{
	  break;
	}

      if(pf_index < 0)
	{
	  break;
	}

      if(count_prefetches >= PREFETCH_DEGREE)
	{
	  break;
	}

      if(ampm_pages[page_index].access_map[pf_index] == 1)
	{
	  // don't prefetch something that's already been demand accessed
	  continue;
	}

      if(ampm_pages[page_index].pf_map[pf_index] == 1)
	{
	  // don't prefetch something that's alrady been prefetched
	  continue;
	}

      if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1))
	{
	  // we found the stride repeated twice, so issue a prefetch

	  unsigned long long int pf_address = (page<<12)+(pf_index<<6);

	  // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
	  if(get_l2_mshr_occupancy(0) < 12)
	    {
	      l2_prefetch_line(0, addr, pf_address, FILL_L2);
	    }
	  else
	    {
	      l2_prefetch_line(0, addr, pf_address, FILL_LLC);	      
	    }

	  // mark the prefetched line so we don't prefetch it again
	  ampm_pages[page_index].pf_map[pf_index] = 1;
	  count_prefetches++;
	}
    }
}
