#include "benchmark.hpp"

#include "csv_reader.hpp"
#include "memory_bptree.hpp"
#include "paged_bptree.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <sys/resource.h>
#include <vector>

namespace bpt {
namespace {

using Clock = std::chrono::steady_clock;

class ReservoirSampler {
public:
    ReservoirSampler(std::size_t capacity, std::uint64_t seed)
        : capacity_(capacity), rng_(seed) {
        sample_.reserve(capacity_);
    }

    void consider(const Record& record) {
        ++seen_;
        if (capacity_ == 0) return;
        if (sample_.size() < capacity_) {
            sample_.push_back(record);
            return;
        }
        std::uniform_int_distribution<std::uint64_t> dist(0, seen_ - 1);
        const std::uint64_t j = dist(rng_);
        if (j < capacity_) sample_[static_cast<std::size_t>(j)] = record;
    }

    const std::vector<Record>& sample() const { return sample_; }

private:
    std::size_t capacity_;
    std::uint64_t seen_ = 0;
    std::mt19937_64 rng_;
    std::vector<Record> sample_;
};

std::string csv_escape(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out += '"';
    return out;
}

void ensure_results_header(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path().empty() ? "." : path.parent_path());
    bool needs_header = true;
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) > 0) {
        needs_header = false;
    }
    if (!needs_header) return;

    std::ofstream out(path, std::ios::app);
    if (!out) throw std::runtime_error("Could not create results file: " + path.string());
    out << "mode,dataset,records,insert_seconds,insert_records_per_sec,search_count,search_seconds,avg_search_us,"
           "successful_searches,verification_failures,peak_rss_mb,disk_bytes,tree_height,nodes_or_pages,"
           "page_reads_insert,page_writes_insert,page_reads_search,page_writes_search,"
           "payload_reads_insert,payload_writes_insert,payload_reads_search,payload_writes_search,"
           "cache_hits_insert,cache_misses_insert,cache_hits_search,cache_misses_search,cache_hit_rate_total,"
           "internal_capacity,leaf_capacity,memory_max_keys,page_size,cache_pages,seed\n";
}

void append_result(const BenchmarkOptions& o,
                   std::uint64_t records,
                   double insert_seconds,
                   double search_seconds,
                   std::uint64_t successes,
                   std::uint64_t failures,
                   const TreeStats& before_search,
                   const TreeStats& final_stats) {
    ensure_results_header(o.output_csv);
    std::ofstream out(o.output_csv, std::ios::app);
    if (!out) throw std::runtime_error("Could not append results file: " + o.output_csv.string());

    const double ips = insert_seconds > 0.0 ? static_cast<double>(records) / insert_seconds : 0.0;
    const std::uint64_t searched = successes + failures;
    const double avg_us = searched > 0 ? search_seconds * 1e6 / static_cast<double>(searched) : 0.0;
    const std::uint64_t total_cache = final_stats.cache_hits + final_stats.cache_misses;
    const double cache_rate = total_cache > 0
        ? 100.0 * static_cast<double>(final_stats.cache_hits) / static_cast<double>(total_cache)
        : 0.0;

    const auto delta = [](std::uint64_t a, std::uint64_t b) -> std::uint64_t {
        return b >= a ? b - a : 0;
    };

    out << csv_escape(o.mode) << ','
        << csv_escape(o.dataset) << ','
        << records << ','
        << std::fixed << std::setprecision(6) << insert_seconds << ','
        << std::fixed << std::setprecision(2) << ips << ','
        << searched << ','
        << std::fixed << std::setprecision(6) << search_seconds << ','
        << std::fixed << std::setprecision(3) << avg_us << ','
        << successes << ','
        << failures << ','
        << std::fixed << std::setprecision(2) << (static_cast<double>(peak_rss_kb()) / 1024.0) << ','
        << final_stats.disk_bytes << ','
        << final_stats.height << ','
        << final_stats.nodes_or_pages << ','
        << before_search.page_reads << ','
        << before_search.page_writes << ','
        << delta(before_search.page_reads, final_stats.page_reads) << ','
        << delta(before_search.page_writes, final_stats.page_writes) << ','
        << before_search.payload_reads << ','
        << before_search.payload_writes << ','
        << delta(before_search.payload_reads, final_stats.payload_reads) << ','
        << delta(before_search.payload_writes, final_stats.payload_writes) << ','
        << before_search.cache_hits << ','
        << before_search.cache_misses << ','
        << delta(before_search.cache_hits, final_stats.cache_hits) << ','
        << delta(before_search.cache_misses, final_stats.cache_misses) << ','
        << std::fixed << std::setprecision(2) << cache_rate << ','
        << final_stats.internal_capacity << ','
        << final_stats.leaf_capacity << ','
        << o.memory_max_keys << ','
        << PagedBPlusTree::PAGE_SIZE << ','
        << (o.mode == "hybrid" ? o.cache_pages : 0) << ','
        << o.seed << '\n';
}

void print_summary(const BenchmarkOptions& o,
                   std::uint64_t records,
                   double insert_seconds,
                   double search_seconds,
                   std::uint64_t successes,
                   std::uint64_t failures,
                   const TreeStats& before_search,
                   const TreeStats& final_stats) {
    const std::uint64_t searched = successes + failures;
    const double avg_us = searched ? search_seconds * 1e6 / static_cast<double>(searched) : 0.0;
    std::cout << "\n============================================================\n";
    std::cout << "BENCHMARK COMPLETE\n";
    std::cout << "============================================================\n";
    std::cout << "Mode                 : " << o.mode << '\n';
    std::cout << "Records inserted     : " << records << '\n';
    std::cout << "Insertion time       : " << std::fixed << std::setprecision(3) << insert_seconds << " s\n";
    std::cout << "Insertion throughput : " << std::fixed << std::setprecision(1)
              << (insert_seconds > 0 ? records / insert_seconds : 0) << " records/s\n";
    std::cout << "Searches             : " << searched << '\n';
    std::cout << "Average search       : " << std::fixed << std::setprecision(3) << avg_us << " us\n";
    std::cout << "Verified hits        : " << successes << '\n';
    std::cout << "Verification failures: " << failures << '\n';
    std::cout << "Peak RSS             : " << std::fixed << std::setprecision(2)
              << static_cast<double>(peak_rss_kb()) / 1024.0 << " MB\n";
    std::cout << "Tree height          : " << final_stats.height << '\n';
    std::cout << "Nodes/pages          : " << final_stats.nodes_or_pages << '\n';
    std::cout << "Internal capacity    : " << final_stats.internal_capacity << " keys/page\n";
    std::cout << "Leaf capacity        : " << final_stats.leaf_capacity << " entries/page\n";

    if (o.mode != "memory") {
        const auto sr = final_stats.page_reads - before_search.page_reads;
        const auto sw = final_stats.page_writes - before_search.page_writes;
        const std::uint64_t total_cache = final_stats.cache_hits + final_stats.cache_misses;
        const double hit_rate = total_cache
            ? 100.0 * static_cast<double>(final_stats.cache_hits) / static_cast<double>(total_cache)
            : 0.0;
        std::cout << "Disk usage           : " << std::fixed << std::setprecision(2)
                  << static_cast<double>(final_stats.disk_bytes) / (1024.0 * 1024.0) << " MB\n";
        std::cout << "Insert page reads    : " << before_search.page_reads << '\n';
        std::cout << "Insert page writes   : " << before_search.page_writes << '\n';
        std::cout << "Search page reads    : " << sr << '\n';
        std::cout << "Search page writes   : " << sw << '\n';
        if (o.mode == "hybrid") {
            std::cout << "Cache pages          : " << o.cache_pages << " (~"
                      << (o.cache_pages * PagedBPlusTree::PAGE_SIZE) / (1024.0 * 1024.0)
                      << " MB nominal pages)\n";
            std::cout << "Cache hit rate       : " << std::fixed << std::setprecision(2) << hit_rate << "%\n";
        }
    }
    std::cout << "Results appended to  : " << o.output_csv << '\n';
    std::cout << "============================================================\n";
}

} // namespace

std::uint64_t peak_rss_kb() {
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss / 1024); // bytes -> KB on macOS
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss); // KB on Linux
#endif
}

int run_benchmark(const BenchmarkOptions& o) {
    if (o.mode != "memory" && o.mode != "disk" && o.mode != "hybrid") {
        throw std::runtime_error("--mode must be one of: memory, disk, hybrid");
    }
    if (o.dataset.empty()) throw std::runtime_error("--dataset is required");

    std::cout << "============================================================\n";
    std::cout << "B+ TREE BENCHMARK\n";
    std::cout << "============================================================\n";
    std::cout << "Mode            : " << o.mode << '\n';
    std::cout << "Dataset         : " << o.dataset << '\n';
    std::cout << "Record limit    : " << (o.limit ? std::to_string(o.limit) : std::string("ALL")) << '\n';
    std::cout << "Search sample   : " << o.search_count << '\n';
    std::cout << "Random seed     : " << o.seed << '\n';
    if (o.mode == "memory") {
        std::cout << "Memory max keys : " << o.memory_max_keys << '\n';
    } else {
        std::cout << "Page size       : " << PagedBPlusTree::PAGE_SIZE << " bytes\n";
        std::cout << "Key bytes       : " << PagedBPlusTree::KEY_BYTES << " bytes max\n";
        if (o.mode == "hybrid") std::cout << "Cache pages     : " << o.cache_pages << '\n';
    }
    std::cout << "============================================================\n\n";

    ReservoirSampler sampler(o.search_count, o.seed);
    CsvReader reader(o.dataset);
    Record record;
    std::uint64_t inserted = 0;

    std::unique_ptr<MemoryBPlusTree> memory_tree;
    std::unique_ptr<PagedBPlusTree> paged_tree;

    if (o.mode == "memory") {
        memory_tree = std::make_unique<MemoryBPlusTree>(o.memory_max_keys);
    } else {
        const auto mode_dir = o.workdir / o.mode;
        std::error_code ec;
        std::filesystem::remove_all(mode_dir, ec);
        std::filesystem::create_directories(mode_dir);
        paged_tree = std::make_unique<PagedBPlusTree>(mode_dir, o.mode == "hybrid" ? o.cache_pages : 0);
        const auto initial = paged_tree->stats();
        std::cout << "Derived internal capacity: " << initial.internal_capacity << " keys/page\n";
        std::cout << "Derived leaf capacity    : " << initial.leaf_capacity << " entries/page\n\n";
    }

    const auto insert_start = Clock::now();
    while ((o.limit == 0 || inserted < o.limit) && reader.next(record)) {
        if (o.mode == "memory") memory_tree->insert(record.key, record.payload);
        else paged_tree->insert(record.key, record.payload);

        sampler.consider(record);
        ++inserted;
        if (o.progress_every > 0 && inserted % o.progress_every == 0) {
            const double elapsed = std::chrono::duration<double>(Clock::now() - insert_start).count();
            const double rate = elapsed > 0 ? inserted / elapsed : 0;
            std::cout << "Inserted " << inserted << " records (" << std::fixed << std::setprecision(1)
                      << rate << " records/s)\n";
        }
    }
    const double insert_seconds = std::chrono::duration<double>(Clock::now() - insert_start).count();

    if (inserted == 0) throw std::runtime_error("No data rows were read from the CSV");
    if (o.limit > 0 && inserted < o.limit) {
        std::cout << "NOTE: dataset ended after " << inserted << " rows, before requested limit " << o.limit << ".\n";
    }

    TreeStats before_search = o.mode == "memory" ? memory_tree->stats() : paged_tree->stats();

    // Search a deterministic reservoir sample of existing records and verify payload equality.
    std::uint64_t successes = 0;
    std::uint64_t failures = 0;
    std::string found_payload;
    const auto search_start = Clock::now();
    for (const auto& expected : sampler.sample()) {
        bool found = false;
        if (o.mode == "memory") found = memory_tree->search(expected.key, found_payload);
        else found = paged_tree->search(expected.key, found_payload);

        if (found && found_payload == expected.payload) ++successes;
        else ++failures;
    }
    const double search_seconds = std::chrono::duration<double>(Clock::now() - search_start).count();

    TreeStats final_stats = o.mode == "memory" ? memory_tree->stats() : paged_tree->stats();
    append_result(o, inserted, insert_seconds, search_seconds, successes, failures, before_search, final_stats);
    print_summary(o, inserted, insert_seconds, search_seconds, successes, failures, before_search, final_stats);

    return failures == 0 ? 0 : 2;
}

} // namespace bpt
