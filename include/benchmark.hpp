#pragma once

#include "common.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace bpt {

struct BenchmarkOptions {
    std::string mode;
    std::string dataset;
    std::uint64_t limit = 0; // 0 = all records
    std::size_t search_count = 10000;
    std::size_t memory_max_keys = 100;
    std::size_t cache_pages = 4096;
    std::uint64_t seed = 42;
    std::uint64_t progress_every = 500000;
    std::filesystem::path output_csv = "results/results.csv";
    std::filesystem::path workdir = "run_data";
};

int run_benchmark(const BenchmarkOptions& options);
std::uint64_t peak_rss_kb();

} // namespace bpt
