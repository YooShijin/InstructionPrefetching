#ifndef FDIP_H
#define FDIP_H

#include <cstdint>
#include <optional>

#include "address.h"
#include "champsim.h"
#include "modules.h"
#include "msl/lru_table.h"

struct fdip : public champsim::modules::prefetcher {
  struct tracker_entry {
    champsim::address ip{};                                // IP tracked
    champsim::block_number last_cl_addr{};                 // last cache-line address seen
    champsim::block_number::difference_type last_stride{}; // last stride observed

    auto index() const
    {
      using namespace champsim::data::data_literals;
      return ip.slice_upper<2_b>();
    }
    auto tag() const
    {
      using namespace champsim::data::data_literals;
      return ip.slice_upper<2_b>();
    }
  };

  struct lookahead_entry {
    champsim::address address{};                 // current prefetch base (cache-line address)
    champsim::address::difference_type stride{}; // stride to apply each step (for FDIP it's +1)
    int degree = 0;                              // remaining degree
  };

  constexpr static std::size_t TRACKER_SETS = 256;
  constexpr static std::size_t TRACKER_WAYS = 4;
  constexpr static int PREFETCH_DEGREE = 3; // number of sequential lines to prefetch

  std::optional<lookahead_entry> active_lookahead;
  champsim::msl::lru_table<tracker_entry> table{TRACKER_SETS, TRACKER_WAYS};

public:
  using champsim::modules::prefetcher::prefetcher;

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);

  void prefetcher_cycle_operate();

  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
};

#endif // FDIP_H




















