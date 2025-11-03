#ifndef FDIP_H
#define FDIP_H

#include <cstdint>
#include <optional>
#include <deque>
#include <vector>
#include "address.h"
#include "champsim.h"
#include "modules.h"
#include "msl/lru_table.h"

struct fdip : public champsim::modules::prefetcher {
    
    // FTQ Entry - stores predicted fetch blocks
    struct ftq_entry {
        champsim::address fetch_addr{};     // Fetch block address
        champsim::address ip{};             // Instruction pointer that led here
        bool is_branch{false};              // Is this a branch target?
        bool prefetch_candidate{false};     // Should we prefetch this?
        bool enqueued{false};               // Already in prefetch queue?
        uint64_t confidence{0};             // Confidence counter
        
        auto index() const {
            using namespace champsim::data::data_literals;
            return fetch_addr.slice_upper<6_b>();
        }
        
        auto tag() const {
            using namespace champsim::data::data_literals;
            return fetch_addr.slice_lower<10_b>();
        }
    };
    
    // Cache Miss Tracker - tracks which cache sets miss frequently
    struct miss_tracker_entry {
        uint32_t set_index{0};
        uint8_t miss_counter{0};  // 2-bit saturating counter
        
        auto index() const { return set_index; }
        auto tag() const { return set_index; }
    };
    
    // Eviction Tracker - marks evicted blocks in BTB-like structure
    struct eviction_entry {
        champsim::address addr{};
        bool evicted{false};
        uint64_t last_access{0};
        
        auto index() const {
            using namespace champsim::data::data_literals;
            return addr.slice_upper<8_b>();
        }
        
        auto tag() const {
            using namespace champsim::data::data_literals;
            return addr.slice_lower<12_b>();
        }
    };
    
    // Prefetch Buffer Entry
    struct prefetch_buffer_entry {
        champsim::address addr{};
        bool valid{false};
        uint64_t fill_cycle{0};
    };
    
    // Configuration parameters
    static constexpr std::size_t FTQ_SIZE = 32;
    static constexpr std::size_t PREFETCH_QUEUE_SIZE = 16;
    static constexpr std::size_t PREFETCH_BUFFER_SIZE = 8;
    static constexpr std::size_t MISS_TRACKER_SETS = 64;
    static constexpr std::size_t EVICTION_TRACKER_SETS = 256;
    static constexpr std::size_t EVICTION_TRACKER_WAYS = 4;
    static constexpr int PREFETCH_DEGREE = 3;
    static constexpr int FTQ_LOOKAHEAD_START = 2;  // Start from 2nd entry
    static constexpr int FTQ_LOOKAHEAD_END = 10;   // Up to 10th entry
    static constexpr uint64_t MISS_COUNTER_RESET_INTERVAL = 1000000;
    
    // Data structures
    std::deque<ftq_entry> ftq;
    std::deque<champsim::address> prefetch_queue;
    std::vector<prefetch_buffer_entry> prefetch_buffer;
    std::vector<miss_tracker_entry> miss_tracker;
    champsim::msl::lru_table<eviction_entry> eviction_tracker{EVICTION_TRACKER_SETS, EVICTION_TRACKER_WAYS};
    
    // Counters and state
    uint64_t cycle_count{0};
    uint64_t last_miss_reset{0};
    uint32_t ftq_head{0};  // Current FTQ head for instruction fetch
    uint32_t prefetch_scan_ptr{0};  // Pointer for scanning FTQ for prefetches
    
    // Statistics
    uint64_t total_prefetches{0};
    uint64_t useful_prefetches{0};
    uint64_t filtered_prefetches{0};
    
public:
    using champsim::modules::prefetcher::prefetcher;
    
    void prefetcher_initialize() ;
    
    uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, 
                                     uint8_t cache_hit, bool useful_prefetch, 
                                     access_type type, uint32_t metadata_in) ;
    
    void prefetcher_cycle_operate() ;
    
    uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, 
                                   uint8_t prefetch, champsim::address evicted_addr, 
                                   uint32_t metadata_in) ;
    
    void prefetcher_final_stats() ;
    
private:
    // Helper functions
    void add_to_ftq(champsim::address addr, champsim::address ip, bool is_branch);
    void scan_ftq_for_prefetches();
    bool should_prefetch(const ftq_entry& entry);
    bool cache_probe_filter(champsim::address addr);
    void enqueue_prefetch(champsim::address addr);
    void issue_prefetches();
    void update_miss_tracker(uint32_t set_index, bool is_miss);
    bool is_high_miss_set(champsim::address addr);
    void mark_evicted(champsim::address addr);
    bool is_marked_evicted(champsim::address addr);
    void advance_ftq();
};

#endif // FDIP_H