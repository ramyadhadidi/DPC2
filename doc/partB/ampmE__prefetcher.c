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
#include <stdlib.h>
#include "../inc/prefetcher.h"

#define AMPM_PAGE_COUNT 64
#define AMPM_PREFETCH_DEGREE 2

typedef struct cl
{
  unsigned long long addr;

  int pf;

  int valid;
} cl_t;

typedef struct pollution
{
  unsigned long long addr;

  int valid;
} pollution_t;

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

cl_t cache[4096];
int cache_size = 0;

int miss_tot = 0;
int acc_tot = 0;


pollution_t poll[100000];
int poll_size = 0;
int poll_tot = 0;

int prefetch_tot_num = 0;
int prefetch_use_num = 0;

int ampm_page_miss = 0;
int ampm_page_hit = 0;
int ampm_imp = 0;

void cache_insert(unsigned long long addr, int pf) {
  int i;
  for (i=0; i<cache_size; i++)
    if (cache[i].valid == 0) {
      cache[i].valid = 1;
      cache[i].addr = addr;
      cache[i].pf = pf;
      return;
    }
  cache[cache_size].valid = 1;
  cache[cache_size].addr = addr;
  cache[cache_size].pf = pf;
  cache_size++;
}

void cache_remove(unsigned long long addr) {
  int i;
  for (i=0; i<cache_size; i++)
    if (cache[i].addr == addr) {
      cache[i].valid = 0;
    }
}

int cache_search(unsigned long long addr) {
  int i;
  for (i=0; i<cache_size; i++)
    if (cache[i].addr == addr) 
      return i;
  return -1;
}

void poll_insert(unsigned long long addr) {
  int i;
  for (i=0; i<poll_size; i++)
    if (poll[i].valid == 0) {
      poll[i].valid = 1;
      poll[i].addr = addr;
      return;
    }
  poll[poll_size].valid = 1;
  poll[poll_size].addr = addr;
  poll_size++;
}

void poll_remove(unsigned long long addr) {
  int i;
  for (i=0; i<poll_size; i++)
    if (poll[i].addr == addr) {
      poll[i].valid = 0;
    }
}

int poll_search(unsigned long long addr) {
  int i;
  for (i=0; i<poll_size; i++)
    if (poll[i].addr == addr) 
      return i;
  return -1;
}

void l2_prefetcher_initialize(int cpu_num)
{
  printf("AMPM Lite Prefetcher\n");
  // you can inspect these knob values from your code to see which configuration you're runnig in
  printf("Knobs visible from prefetcher: %d %d %d\n", knob_scramble_loads, knob_small_llc, knob_low_bandwidth);

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

void l2_prefetcher_operate_ampm(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit)
{
  // uncomment this line to see all the information available to make prefetch decisions
  //printf("(0x%llx 0x%llx %d %d %d) ", addr, ip, cache_hit, get_l2_read_queue_occupancy(0), get_l2_mshr_occupancy(0));

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
    ampm_page_miss++;
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
    else {
      ampm_page_hit++;
    }

  // update LRU
  ampm_pages[page_index].lru = get_current_cycle(0);

  // mark the access map
  ampm_pages[page_index].access_map[page_offset] = 1;

  //** Prefetching
  // positive prefetching
  int count_prefetches = 0;
  for(i=1; i<=16; i++) {
    unsigned long long int page_pf = page;
    unsigned long long int addr_base_pf = addr;
    int check_index1 = page_offset - i;
    int check_index2 = page_offset - 2*i;
    int pf_index = page_offset + i;
    
    if(check_index2 < 0)
      break;

    if(pf_index > 63) {
      break;
    }

    if(count_prefetches >= AMPM_PREFETCH_DEGREE)
      break;

    if(ampm_pages[page_index].access_map[pf_index] == 1)
      continue;

    if(ampm_pages[page_index].pf_map[pf_index] == 1)
      continue;

    if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1)) {
      // we found the stride repeated twice, so issue a prefetch
      unsigned long long int pf_address = (page_pf<<12)+(pf_index<<6);

      //if(get_l2_mshr_occupancy(0) < 8)
        //l2_prefetch_line(0, addr_base_pf, pf_address, FILL_L2);
      //else
        l2_prefetch_line(0, addr_base_pf, pf_address, FILL_LLC);       

      // mark the prefetched line so we don't prefetch it again
      ampm_pages[page_index].pf_map[pf_index] = 1;
      count_prefetches++;
    }
  }

  // negative prefetching
  count_prefetches = 0;
  for(i=1; i<=16; i++) {
    unsigned long long int page_pf = page;
    unsigned long long int addr_base_pf = addr;
    int check_index1 = page_offset + i;
    int check_index2 = page_offset + 2*i;
    int pf_index = page_offset - i;

    if(check_index2 > 63)
      break;

    if(pf_index < 0) {
      break;
    }

    if(count_prefetches >= AMPM_PREFETCH_DEGREE)
      break;

    if(ampm_pages[page_index].access_map[pf_index] == 1)
      continue;

    if(ampm_pages[page_index].pf_map[pf_index] == 1)
      continue;

    if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1)) {
      unsigned long long int pf_address = (page_pf<<12)+(pf_index<<6);

      //if(get_l2_mshr_occupancy(0) < 12)
        //l2_prefetch_line(0, addr_base_pf, pf_address, FILL_L2);
      //else
        l2_prefetch_line(0, addr_base_pf, pf_address, FILL_LLC);

      // mark the prefetched line so we don't prefetch it again
      ampm_pages[page_index].pf_map[pf_index] = 1;
      count_prefetches++;
    }
  }


  /*
  int j;
  // ME +
  count_prefetches = 0;
  for(i=1; i<=16; i++) {
    for (j=1;j<16; j++) {

      unsigned long long int page_pf = page;
      unsigned long long int addr_base_pf = addr;
      int index1 = i;
      int index2 = j;
      int index3 = i;
      int index4 = j;

      int check_index1 = page_offset - index1;
      int check_index2 = check_index1 - index2;
      int check_index3 = check_index2 - index3;
      int pf_index = check_index3 + index4;

      if(check_index3 < 0)
        break;

      if(pf_index > 63) {
        break;
      }

      if(count_prefetches >= AMPM_PREFETCH_DEGREE)
        break;

      if(ampm_pages[page_index].access_map[pf_index] == 1)
        continue;

      if(ampm_pages[page_index].pf_map[pf_index] == 1)
        continue;

      if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1) && (ampm_pages[page_index].access_map[check_index3]==1)) {
        unsigned long long int pf_address = (page_pf<<12)+(pf_index<<6);
        ampm_imp++;

        //if(get_l2_mshr_occupancy(0) < 12)
          //l2_prefetch_line(0, addr_base_pf, pf_address, FILL_L2);
        //else
          l2_prefetch_line(0, addr_base_pf, pf_address, FILL_LLC);

        // mark the prefetched line so we don't prefetch it again
        ampm_pages[page_index].pf_map[pf_index] = 1;
        count_prefetches++;
      }
    }
  }
  // ME -
  count_prefetches = 0;
  for(i=1; i<=16; i++) {
    for (j=1;j<16; j++) {

      unsigned long long int page_pf = page;
      unsigned long long int addr_base_pf = addr;
      int index1 = i;
      int index2 = j;
      int index3 = i;
      int index4 = j;

      int check_index1 = page_offset + index1;
      int check_index2 = check_index1 + index2;
      int check_index3 = check_index2 + index3;
      int pf_index = check_index3 - index4;

      if(check_index3 > 63)
        break;

      if(pf_index < 0) {
        break;
      }

      if(count_prefetches >= AMPM_PREFETCH_DEGREE)
        break;

      if(ampm_pages[page_index].access_map[pf_index] == 1)
        continue;

      if(ampm_pages[page_index].pf_map[pf_index] == 1)
        continue;

      if((ampm_pages[page_index].access_map[check_index1]==1) && (ampm_pages[page_index].access_map[check_index2]==1) && (ampm_pages[page_index].access_map[check_index3]==1)) {
        unsigned long long int pf_address = (page_pf<<12)+(pf_index<<6);
        ampm_imp++;

        //if(get_l2_mshr_occupancy(0) < 12)
          //l2_prefetch_line(0, addr_base_pf, pf_address, FILL_L2);
        //else
          l2_prefetch_line(0, addr_base_pf, pf_address, FILL_LLC);

        // mark the prefetched line so we don't prefetch it again
        ampm_pages[page_index].pf_map[pf_index] = 1;
        count_prefetches++;
      }
    }
  }
  */

}

void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit) 
{
  l2_prefetcher_operate_ampm(cpu_num, addr, ip, cache_hit);


  acc_tot++;
  if (!cache_hit) {
    miss_tot++;

    if ( poll_search(addr) != -1 )
      poll_tot++;
  }

  int found = cache_search(addr);
  if (found != -1)
    if (cache[found].pf == 1) {
      prefetch_use_num++;
      cache[found].pf = 0;
    }
  

  //printf("acc:%d miss:%d pre:%d use_pre:%d poll:%d ___ page miss:%d hit:%d imp:%d\n", acc_tot, miss_tot ,prefetch_tot_num, prefetch_use_num, poll_tot, ampm_page_miss, ampm_page_hit, ampm_imp);
}

void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr)
{
  // uncomment this line to see the information available to you when there is a cache fill event
  //printf("0x%llx %d %d %d 0x%llx\n", addr, set, way, prefetch, evicted_addr);

  cache_insert(addr, prefetch);
  if (prefetch==1) {
    prefetch_tot_num++;


    // Insert Evicted address into pollution filter
    // only if it is not prefetch
    int found = cache_search(evicted_addr);
    if (found == -1) {
      if (evicted_addr != 0) {
      printf("ERROR: evicted address not in the cache - %llx \n", evicted_addr);
      exit(1);
      }
    }
    else 
      if (cache[found].pf != 1)
        poll_insert(evicted_addr);

    poll_remove(addr);
    
  }

  cache_remove(evicted_addr);
  
}


