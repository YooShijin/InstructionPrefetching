#ifndef BRANCH_FDIP_H
#define BRANCH_FDIP_H

#include <array>
#include <algorithm>
#include "address.h"
#include "modules.h"

// Minimal FDIP: Fetch-directed Indirect Predictor
class fdip : public champsim::modules::branch_predictor
{
private:
    static constexpr size_t TABLE_SIZE = 1024;
    static constexpr size_t TARGETS_PER_ENTRY = 4;
    static constexpr size_t HISTORY_LENGTH = 16;  // 16-bit global history

    // Branch type constants
    static constexpr uint8_t BRANCH_DIRECT_JUMP = 1;
    static constexpr uint8_t BRANCH_INDIRECT = 2;
    static constexpr uint8_t BRANCH_CONDITIONAL = 3;
    static constexpr uint8_t BRANCH_DIRECT_CALL = 4;
    static constexpr uint8_t BRANCH_INDIRECT_CALL = 5;
    static constexpr uint8_t BRANCH_RETURN = 6;

    struct target_entry {
        uint64_t target;
        uint16_t history_pattern;
        uint8_t confidence;
        target_entry() : target(0), history_pattern(0), confidence(0) {}
    };

    struct entry {
        uint64_t tag;
        std::array<target_entry, TARGETS_PER_ENTRY> targets;
        bool valid;
        entry() : tag(0), valid(false) {}
    };

    std::array<entry, TABLE_SIZE> table;
    uint16_t global_history = 0;

    [[nodiscard]] static size_t hash(champsim::address ip) {
        return ip.to<uint64_t>() % TABLE_SIZE;
    }

    target_entry* find_best_target(entry& e);

public:
    using branch_predictor::branch_predictor;

    bool predict_branch(champsim::address ip);
    void last_branch_result(champsim::address ip,
                            champsim::address branch_target,
                            bool taken,
                            uint8_t branch_type);
};

#endif
