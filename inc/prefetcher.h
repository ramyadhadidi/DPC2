//
// Data Prefetching Championship 2 Simulator
// Seth Pugsley, seth.h.pugsley@intel.com
//

// CHAMPIONSHIP PARTICIPANTS SHOULD NOT EDIT THIS FILE

// 64 bytes
#define CACHE_LINE_SIZE 64
// 4 KB
#define PAGE_SIZE 4096

/*
 Use one of these for the fill_level argument of l2_prefetch_line().
 IMPORTANT: There are trade-offs between prefetching into the L2 and LLC (see below).
 You should experiment using both fill levels in different situations.
*/

// FILL_L2 causes the prefetched cache line to be brought into the Mid Level Cache (L2).  
// L2 prefetches DO consume an L2 MSHR, but bring the prefetched data closer to the core.
// Use this option if L2 MSHR resources are plentiful.
#define FILL_L2 (1<<2)
// FILL_LLC causes the prefetched cache line to be brought into the Last Level Cache (LLC).
// LLC prefetches DO NOT consume an L2 MSHR, but accessing the prefetched data in the LLC will be slower.
// Use this option if L2 MSHR resources are scarce.
#define FILL_LLC (1<<3)

/*
 You may examine these knob variables to see which simulation configuration you are operating in.
*/

extern int knob_low_bandwidth;
extern int knob_small_llc;
extern int knob_scramble_loads;

/*
  These functions are provided for you.
  cpu_num is a vestige of a previous version of the simulator.
  Always pass 0 for cpu_num into these functions for the championship.
*/

#define L2_MSHR_COUNT 16
#define L2_READ_QUEUE_SIZE 32

// returns the CPU core's cycle count since simulation began
unsigned long long int get_current_cycle(int cpu_num);

// Returns the current number of occupied L2 MSHRs (out of the maximum 16).
int get_l2_mshr_occupancy(int cpu_num);

// Returns the current length of the L2 read queue (out of the maximum 32).
int get_l2_read_queue_occupancy(int cpu_num);

// Prefetches cache line with address pf_addr, into the cache level specified by fill_level (see FILL_L2 and FILL_LLC above).
// base_addr should be the same address that was passed into l2_prefetcher_operate() as addr by the simulator.
// NOTE: base_addr and pf_addr MUST be in the same 4 KB page, otherwise the prefetch will fail
// Returns 1 if the prefetch was successfully added to the L2 read queue, and 0 if the prefetch was not.
// This function can also fail if the L2 read queue is full, or if all L2 MSHRs are occupied at the time the prefetch is issued.
int l2_prefetch_line(int cpu_num, unsigned long long int base_addr, unsigned long long int pf_addr, int fill_level);

#define L2_SET_COUNT 256
#define L2_ASSOCIATIVITY 8

// Returns which set in the L2 this line is found
int l2_get_set(unsigned long long int addr);

// Returns which way in its set (see l2_get_set()) this line is found.
// Returns -1 if the line is not found in that set
int l2_get_way(int cpu_num, unsigned long long int addr, int set);

/*
  These functions are to be implemented by the championship participant.
  cpu_num is a vestige of a previous version of the simulator, and will always be 0 in the current simulator.
*/

// This function is called once by the simulator on startup
void l2_prefetcher_initialize(int cpu_num);

// This function is called once for each Mid Level Cache read, and is the entry point for participants' prefetching algorithms.
// addr - the byte address of the current cache read
// ip - the instruction pointer (program counter) of the instruction that caused the current cache read
// cache_hit - 1 for an L2 cache hit, 0 for an L2 cache miss
void l2_prefetcher_operate(int cpu_num, unsigned long long int addr, unsigned long long int ip, int cache_hit);

// This function is called when a cache block is filled into the L2, and lets you konw which set and way of the cache the block occupies.
// You can use this function to know when prefetched lines arrive in the L2, and along with l2_get_set() and l2_get_way() you can
// reconstruct a view of the contents of the L2 cache.
// Using this function is optional.
void l2_cache_fill(int cpu_num, unsigned long long int addr, int set, int way, int prefetch, unsigned long long int evicted_addr);
