#ifndef BRANCH_FIDP_H
#define BRANCH_FIDP_H

#include <array>
#include <cstdint>
#include <vector>

#include "address.h"
#include "modules.h"

// FIDP: Fetch-directed Indirect Predictor
// Uses fetch direction history to predict indirect branches

class fidp : public champsim::modules::branch_predictor
{
private:
  // Configuration
  static constexpr std::size_t TABLE_SIZE = 4096;
  static constexpr std::size_t HISTORY_LENGTH = 16;
  static constexpr std::size_t TARGET_CACHE_SIZE = 4;
  static constexpr std::size_t TAG_BITS = 12;

  // Target entry structure
  struct target_entry {
    uint64_t target;
    uint16_t history_pattern;
    uint8_t confidence;

    target_entry() : target(0), history_pattern(0), confidence(0) {}
  };

  // Main predictor table entry
  struct fidp_entry {
    uint64_t tag;
    std::array<target_entry, TARGET_CACHE_SIZE> targets;
    bool valid;

    fidp_entry() : tag(0), valid(false) {}
  };

  // Predictor state
  std::array<fidp_entry, TABLE_SIZE> table;
  uint16_t global_history;

  // Helper functions
  [[nodiscard]] static constexpr std::size_t get_index(champsim::address ip, uint16_t history) { return ((ip.to<uint64_t>() >> 2) ^ history) % TABLE_SIZE; }

  [[nodiscard]] static constexpr uint64_t get_tag(champsim::address ip) { return (ip.to<uint64_t>() >> 2) & ((1ULL << TAG_BITS) - 1); }

  [[nodiscard]] static constexpr bool is_indirect_branch(uint8_t branch_type)
  {
    return (branch_type == BRANCH_INDIRECT || branch_type == BRANCH_INDIRECT_CALL);
  }

  target_entry* find_best_target(fidp_entry& entry, uint16_t history);
  target_entry* find_replacement_slot(fidp_entry& entry);

public:
  using branch_predictor::branch_predictor;

  void initialize_branch_predictor();
  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

#endif // BRANCH_FIDP_H