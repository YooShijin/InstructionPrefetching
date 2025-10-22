// fdip.cpp
#include "fdip.h"
#include "cache.h"



uint32_t fdip::prefetcher_cache_operate(champsim::address addr, champsim::address ip,
                                        uint8_t cache_hit, bool useful_prefetch,
                                        access_type type, uint32_t metadata_in)
{
  // Convert to cache-line address
  champsim::block_number cl_addr{addr};
  champsim::block_number::difference_type stride = 0;

  // Check tracker table for this IP
  auto found = table.check_hit({ip, cl_addr, stride});

  if (found.has_value()) {
    // Compute stride between last seen cache line for this IP and current
    stride = champsim::offset(found->last_cl_addr, cl_addr);

    // Trigger FDIP lookahead only for forward sequential stride == +1
    // and only if this repeated stride matches last_stride (confidence)
    if (stride == 1 && stride == found->last_stride) {
      // Start lookahead from the current cache line address
      active_lookahead = {champsim::address{cl_addr}, /*stride*/ 1, PREFETCH_DEGREE};
    }
  }

  // Update tracker entry (fill or update LRU table)
  table.fill({ip, cl_addr, stride});

  return metadata_in;
}

void fdip::prefetcher_cycle_operate()
{
  if (!active_lookahead.has_value())
    return;

  auto [old_pf_address, stride, degree] = active_lookahead.value();
  assert(degree > 0);

  // next sequential block
  champsim::address pf_address{champsim::block_number{old_pf_address} + stride};

  // stop if crossing page boundary unless virtual_prefetch is enabled
  if (intern_->virtual_prefetch || champsim::page_number{pf_address} == champsim::page_number{old_pf_address}) {
    // check MSHR occupancy to decide whether to prefetch into this level
    const bool mshr_under_light_load = intern_->get_mshr_occupancy_ratio() < 0.5;
    const bool success = prefetch_line(pf_address, mshr_under_light_load, 0);
    if (success) {
      // update lookahead to next address and reduce degree
      active_lookahead = {pf_address, stride, degree - 1};
    } else {
      // if prefetch failed to issue, we'll try again next cycle (do not decrement degree)
      active_lookahead = {old_pf_address, stride, degree};
    }

    // if we've finished degree, reset
    if (active_lookahead.has_value() && active_lookahead->degree == 0)
      active_lookahead.reset();
  } else {
    // ran off page, stop
    active_lookahead.reset();
  }
}

uint32_t fdip::prefetcher_cache_fill(champsim::address addr, long set, long way,
                                     uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
