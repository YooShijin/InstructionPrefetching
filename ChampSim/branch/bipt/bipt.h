#ifndef BRANCH_BIPT_H
#define BRANCH_BIPT_H

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include "address.h"
#include "modules.h"
#include "msl/lru_table.h"

class bipt : public champsim::modules::branch_predictor
{
private:
  static constexpr std::size_t TABLE_SIZE = 4096;
  static constexpr std::size_t TARGET_CACHE_SIZE = 4;
  static constexpr std::size_t TAG_BITS = 12;

  struct target_entry {
    uint64_t target = 0;
    uint8_t confidence = 0;
  };

  struct bipt_entry {
    uint64_t tag = 0;
    bool valid = false;
    std::array<target_entry, TARGET_CACHE_SIZE> targets;
  };

  std::array<bipt_entry, TABLE_SIZE> table;

  [[nodiscard]] static constexpr std::size_t get_index(champsim::address ip) { return (ip.to<uint64_t>() >> 2) % TABLE_SIZE; }

  [[nodiscard]] static constexpr uint64_t get_tag(champsim::address ip) { return (ip.to<uint64_t>() >> 2) & ((1ULL << TAG_BITS) - 1); }

  target_entry* find_target(bipt_entry& entry, uint64_t target);
  target_entry* find_replacement_slot(bipt_entry& entry);

public:
  using branch_predictor::branch_predictor;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

#endif // BRANCH_BIPT_H
