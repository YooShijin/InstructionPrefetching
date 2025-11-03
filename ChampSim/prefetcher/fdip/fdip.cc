#include "fdip.h"

#include <algorithm>
#include <iostream>

void fdip::prefetcher_initialize()
{
  // Initialize prefetch buffer
  prefetch_buffer.resize(PREFETCH_BUFFER_SIZE);
  for (auto& entry : prefetch_buffer) {
    entry.valid = false;
  }

  // Initialize miss tracker
  miss_tracker.resize(MISS_TRACKER_SETS);
  for (uint32_t i = 0; i < MISS_TRACKER_SETS; i++) {
    miss_tracker[i].set_index = i;
    miss_tracker[i].miss_counter = 0;
  }

  ftq_head = 0;
  prefetch_scan_ptr = 0;
  cycle_count = 0;
  last_miss_reset = 0;
}

uint32_t fdip::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                        uint32_t metadata_in)
{

  champsim::block_number cl_addr{addr};

  // Add to FTQ - this simulates the branch predictor feeding the FTQ
  // In a real implementation, this would come from the BP unit
  add_to_ftq(addr, ip, false);

  // Track cache misses for filtering
  if (!cache_hit) {
    // Get the cache set index - convert block_number to uint64_t
    uint64_t addr_val = cl_addr.to<uint64_t>();
    uint32_t set_idx = static_cast<uint32_t>(addr_val % MISS_TRACKER_SETS);
    update_miss_tracker(set_idx, true);
  }

  // Track useful prefetches
  if (cache_hit && useful_prefetch) {
    useful_prefetches++;
  }

  return metadata_in;
}

void fdip::prefetcher_cycle_operate()
{
  cycle_count++;

  // Periodic reset of miss counters (as per paper)
  if (cycle_count - last_miss_reset > MISS_COUNTER_RESET_INTERVAL) {
    for (auto& entry : miss_tracker) {
      entry.miss_counter = 0;
    }
    last_miss_reset = cycle_count;
  }

  // Scan FTQ for prefetch candidates
  scan_ftq_for_prefetches();

  // Issue prefetches from the prefetch queue
  issue_prefetches();

  // Advance FTQ head (simulating instruction fetch consuming entries)
  advance_ftq();
}

uint32_t fdip::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{

  // Track evictions for eviction-based prefetching
  if (evicted_addr != champsim::address{0}) {
    mark_evicted(evicted_addr);
  }

  // Remove from prefetch buffer if present
  for (auto& entry : prefetch_buffer) {
    if (entry.valid && champsim::block_number{entry.addr} == champsim::block_number{addr}) {
      entry.valid = false;
      break;
    }
  }

  return metadata_in;
}

void fdip::prefetcher_final_stats()
{
  std::cout << "FDIP Statistics:" << std::endl;
  std::cout << "Total Prefetches: " << total_prefetches << std::endl;
  std::cout << "Useful Prefetches: " << useful_prefetches << std::endl;
  std::cout << "Filtered Prefetches: " << filtered_prefetches << std::endl;
  if (total_prefetches > 0) {
    std::cout << "Prefetch Accuracy: " << (100.0 * useful_prefetches / total_prefetches) << "%" << std::endl;
  }
}

// Private helper functions

void fdip::add_to_ftq(champsim::address addr, champsim::address ip, bool is_branch)
{
  // Check if FTQ is full
  if (ftq.size() >= FTQ_SIZE) {
    return;
  }

  champsim::block_number cl_addr{addr};

  // Check if already in FTQ
  auto it = std::find_if(ftq.begin(), ftq.end(), [cl_addr](const ftq_entry& e) { return champsim::block_number{e.fetch_addr} == cl_addr; });

  if (it != ftq.end()) {
    return; // Already in FTQ
  }

  ftq_entry entry;
  entry.fetch_addr = addr;
  entry.ip = ip;
  entry.is_branch = is_branch;
  entry.prefetch_candidate = false;
  entry.enqueued = false;
  entry.confidence = 0;

  ftq.push_back(entry);
}

void fdip::scan_ftq_for_prefetches()
{
  // Scan FTQ entries from lookahead start to end positions
  size_t scan_start = std::min(static_cast<size_t>(FTQ_LOOKAHEAD_START), ftq.size());
  size_t scan_end = std::min(static_cast<size_t>(FTQ_LOOKAHEAD_END), ftq.size());

  for (size_t i = scan_start; i < scan_end; i++) {
    auto& entry = ftq[i];

    // Skip if already enqueued
    if (entry.enqueued) {
      continue;
    }

    // Determine if this should be prefetched
    if (should_prefetch(entry)) {
      entry.prefetch_candidate = true;
      enqueue_prefetch(entry.fetch_addr);
      entry.enqueued = true;
    }
  }
}

bool fdip::should_prefetch(const ftq_entry& entry)
{
  champsim::address addr = entry.fetch_addr;

  // Filter 1: Check if marked as evicted (high priority)
  if (is_marked_evicted(addr)) {
    return true;
  }

  // Filter 2: Check if maps to high-miss cache set
  if (is_high_miss_set(addr)) {
    return true;
  }

  // Filter 3: Cache probe filtering (if we can check cache)
  // This would check if block is already in cache
  // For now, we'll use a conservative approach
  if (cache_probe_filter(addr)) {
    filtered_prefetches++;
    return false; // Already in cache
  }

  // Default: prefetch if not filtered out
  return true;
}

bool fdip::cache_probe_filter(champsim::address addr)
{
  // Check if address is in prefetch buffer (simple filter)
  champsim::block_number cl_addr{addr};

  for (const auto& entry : prefetch_buffer) {
    if (entry.valid && champsim::block_number{entry.addr} == cl_addr) {
      return true; // Already prefetched
    }
  }

  // In a full implementation, this would check the actual cache tags
  // using an idle cache port (as described in the paper)
  return false;
}

void fdip::enqueue_prefetch(champsim::address addr)
{
  // Check if already in queue
  champsim::block_number cl_addr{addr};
  auto it = std::find_if(prefetch_queue.begin(), prefetch_queue.end(), [cl_addr](const champsim::address& a) { return champsim::block_number{a} == cl_addr; });

  if (it != prefetch_queue.end()) {
    return; // Already queued
  }

  // Add to prefetch queue
  if (prefetch_queue.size() < PREFETCH_QUEUE_SIZE) {
    prefetch_queue.push_back(addr);
  }
}

void fdip::issue_prefetches()
{
  // Issue prefetches from the queue
  // In latest ChampSim, we don't directly check MSHR/PQ occupancy
  // The prefetch_line function will handle queueing internally

  // Try to issue up to PREFETCH_DEGREE prefetches
  int issued = 0;
  while (!prefetch_queue.empty() && issued < PREFETCH_DEGREE) {
    champsim::address pf_addr = prefetch_queue.front();

    // Try to issue prefetch
    // prefetch_line returns true if successful
    bool success = prefetch_line(pf_addr, true, 0);

    if (success) {
      // Add to prefetch buffer
      for (auto& entry : prefetch_buffer) {
        if (!entry.valid) {
          entry.addr = pf_addr;
          entry.valid = true;
          entry.fill_cycle = cycle_count;
          break;
        }
      }

      total_prefetches++;
      prefetch_queue.pop_front();
      issued++;
    } else {
      // Failed to issue, stop trying this cycle
      break;
    }
  }
}

void fdip::update_miss_tracker(uint32_t set_index, bool is_miss)
{
  if (set_index >= MISS_TRACKER_SETS) {
    return;
  }

  auto& entry = miss_tracker[set_index];

  if (is_miss) {
    // Increment saturating counter (max 3)
    if (entry.miss_counter < 3) {
      entry.miss_counter++;
    }
  }
  // Note: We don't decrement on hits, only reset periodically
}

bool fdip::is_high_miss_set(champsim::address addr)
{
  champsim::block_number cl_addr{addr};
  uint64_t addr_val = cl_addr.to<uint64_t>();
  uint32_t set_idx = static_cast<uint32_t>(addr_val % MISS_TRACKER_SETS);

  if (set_idx >= MISS_TRACKER_SETS) {
    return false;
  }

  // Consider it high-miss if counter is saturated (value 3)
  return miss_tracker[set_idx].miss_counter >= 3;
}

void fdip::mark_evicted(champsim::address addr)
{
  eviction_entry entry;
  entry.addr = addr;
  entry.evicted = true;
  entry.last_access = cycle_count;

  eviction_tracker.fill(entry);
}

bool fdip::is_marked_evicted(champsim::address addr)
{
  auto found = eviction_tracker.check_hit(eviction_entry{addr, false, 0});

  if (found.has_value() && found->evicted) {
    // Clear the evicted bit after using it
    eviction_entry updated = *found;
    updated.evicted = false;
    eviction_tracker.fill(updated);
    return true;
  }

  return false;
}

void fdip::advance_ftq()
{
  // Remove oldest entries that have been "consumed" by instruction fetch
  // In a real implementation, this would be driven by actual fetch
  if (!ftq.empty() && ftq.size() > FTQ_LOOKAHEAD_END) {
    ftq.pop_front();
  }
}