#pragma once

#include <cstdint>
#include <string>

namespace bpt {

struct Record {
    std::string key;
    std::string payload;
};

struct TreeStats {
    std::uint64_t records = 0;
    std::uint64_t nodes_or_pages = 0;
    std::uint64_t page_reads = 0;
    std::uint64_t page_writes = 0;
    std::uint64_t payload_reads = 0;
    std::uint64_t payload_writes = 0;
    std::uint64_t cache_hits = 0;
    std::uint64_t cache_misses = 0;
    std::uint64_t disk_bytes = 0;
    std::size_t height = 0;
    std::size_t internal_capacity = 0;
    std::size_t leaf_capacity = 0;
};

} // namespace bpt
