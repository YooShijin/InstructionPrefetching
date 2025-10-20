#include "bipt.h"

bipt::target_entry* bipt::find_target(bipt_entry& entry, uint64_t target)
{
  for (auto& t : entry.targets)
    if (t.target == target)
      return &t;
  return nullptr;
}

bipt::target_entry* bipt::find_replacement_slot(bipt_entry& entry)
{
  // Replace lowest-confidence entry
  return &*std::min_element(entry.targets.begin(), entry.targets.end(),
                            [](const target_entry& a, const target_entry& b) { return a.confidence < b.confidence; });
}

bool bipt::predict_branch(champsim::address ip)
{
  auto idx = get_index(ip);
  auto tag = get_tag(ip);
  auto& entry = table[idx];

  if (entry.valid && entry.tag == tag) {
    // Return true if we have a confident target prediction
    for (auto& t : entry.targets)
      if (t.confidence > 2)
        return true;
  }

  return false;
}

void bipt::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  auto idx = get_index(ip);
  auto tag = get_tag(ip);
  auto& entry = table[idx];

  if (!entry.valid || entry.tag != tag) {
    entry.valid = true;
    entry.tag = tag;
    entry.targets.fill(target_entry());
  }

  auto* t = find_target(entry, branch_target.to<uint64_t>());
  if (!t) {
    t = find_replacement_slot(entry);
    t->target = branch_target.to<uint64_t>();
    t->confidence = 0;
  }

  if (taken)
    t->confidence = std::min<uint8_t>(t->confidence + 1, 7);
  else
    t->confidence = t->confidence > 0 ? t->confidence - 1 : 0;
}
