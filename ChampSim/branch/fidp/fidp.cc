#include "fidp.h"

#include <algorithm>

void fidp::initialize_branch_predictor()
{
  global_history = 0;

  // Initialize all table entries
  for (auto& entry : table) {
    entry.valid = false;
    entry.tag = 0;
    for (auto& target : entry.targets) {
      target.target = 0;
      target.history_pattern = 0;
      target.confidence = 0;
    }
  }
}

fidp::target_entry* fidp::find_best_target(fidp_entry& entry, uint16_t history)
{
  target_entry* best_match = nullptr;
  int best_score = -1;

  for (auto& target : entry.targets) {
    if (target.confidence == 0)
      continue;

    // Calculate Hamming distance (number of matching bits)
    uint16_t xor_result = target.history_pattern ^ history;
    int matching_bits = HISTORY_LENGTH - __builtin_popcount(xor_result);

    // Prefer entries with higher confidence and better pattern match
    int score = (target.confidence * HISTORY_LENGTH) + matching_bits;

    if (score > best_score) {
      best_score = score;
      best_match = &target;
    }
  }

  // Only return if confidence is sufficient
  if (best_match && best_match->confidence >= 2) {
    return best_match;
  }

  return nullptr;
}

fidp::target_entry* fidp::find_replacement_slot(fidp_entry& entry)
{
  // First, try to find an empty slot
  for (auto& target : entry.targets) {
    if (target.confidence == 0) {
      return &target;
    }
  }

  // Find slot with minimum confidence
  target_entry* min_conf_slot = &entry.targets[0];
  for (auto& target : entry.targets) {
    if (target.confidence < min_conf_slot->confidence) {
      min_conf_slot = &target;
    }
  }

  return min_conf_slot;
}

bool fidp::predict_branch(champsim::address ip)
{
  // Only predict for indirect branches
  // For other branches, default prediction is handled by base predictor

  std::size_t index = get_index(ip, global_history);
  uint64_t tag = get_tag(ip);

  fidp_entry& entry = table[index];

  // Check if entry is valid and tag matches
  if (!entry.valid || entry.tag != tag) {
    return true; // Default: predict taken
  }

  // Find best matching target based on history
  target_entry* best = find_best_target(entry, global_history);

  if (best != nullptr) {
    return true; // Predict taken if we found a confident target
  }

  return true; // Default: predict taken
}

void fidp::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // Update global history for all branches (shift in the taken direction)
  global_history = ((global_history << 1) | (taken ? 1 : 0)) & ((1 << HISTORY_LENGTH) - 1);

  // Only update predictor table for indirect branches
  if (!is_indirect_branch(branch_type)) {
    return;
  }

  std::size_t index = get_index(ip, global_history);
  uint64_t tag = get_tag(ip);

  fidp_entry& entry = table[index];

  // Allocate new entry if needed
  if (!entry.valid || entry.tag != tag) {
    entry.tag = tag;
    entry.valid = true;
    // Clear all targets
    for (auto& target : entry.targets) {
      target.confidence = 0;
    }
  }

  // Find if this target already exists
  target_entry* existing = nullptr;
  for (auto& target : entry.targets) {
    if (target.target == branch_target.to<uint64_t>() && target.confidence > 0) {
      existing = &target;
      break;
    }
  }

  if (existing != nullptr) {
    // Update existing target
    existing->history_pattern = global_history;
    if (existing->confidence < 3) {
      existing->confidence++;
    }
  } else {
    // Allocate new target
    target_entry* slot = find_replacement_slot(entry);
    slot->target = branch_target.to<uint64_t>();
    slot->history_pattern = global_history;
    slot->confidence = 1;
  }
}