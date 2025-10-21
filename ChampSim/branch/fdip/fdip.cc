#include "fdip.h"

// Pick the target entry with highest confidence
fdip::target_entry* fdip::find_best_target(entry& e)
{
    target_entry* best = nullptr;
    for (auto& t : e.targets) {
        if (t.target != 0) {
            if (!best || t.confidence > best->confidence)
                best = &t;
        }
    }
    return best;
}

bool fdip::predict_branch(champsim::address ip)
{
    size_t idx = hash(ip);
    auto& e = table[idx];

    if (!e.valid)
        return false;

    auto* t = find_best_target(e);
    return t != nullptr;
}

void fdip::last_branch_result(champsim::address ip,
                              champsim::address branch_target,
                              bool taken,
                              uint8_t branch_type)
{
    // Only track indirect branches
    if (!(branch_type == BRANCH_INDIRECT || branch_type == BRANCH_INDIRECT_CALL))
        return;

    size_t idx = hash(ip);
    auto& e = table[idx];

    if (!e.valid) {
        e.tag = ip.to<uint64_t>();
        e.valid = true;
    }

    // Update global history
    global_history = ((global_history << 1) | (taken ? 1 : 0)) & ((1 << HISTORY_LENGTH) - 1);

    // Find existing target or empty slot
    for (auto& t : e.targets) {
        if (t.target == 0 || t.target == branch_target.to<uint64_t>()) {
            t.target = branch_target.to<uint64_t>();
            t.history_pattern = global_history;
            t.confidence = std::min(255, t.confidence + 1);
            return;
        }
    }

    // Replace the least confident if all slots are full
    auto* least = &e.targets[0];
    for (auto& t : e.targets)
        if (t.confidence < least->confidence)
            least = &t;

    least->target = branch_target.to<uint64_t>();
    least->history_pattern = global_history;
    least->confidence = 1;
}
