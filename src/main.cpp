#include "benchmark.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_help(const char* exe) {
    std::cout <<
        "B+ Tree benchmark for the 10-million-record assignment\n\n"
        "Usage:\n  " << exe << " --mode <memory|disk|hybrid> --dataset <file.csv> [options]\n\n"
        "Options:\n"
        "  --limit N              Insert only first N records (0/all by default)\n"
        "  --search-count N       Existing keys sampled for search benchmark (default 10000)\n"
        "  --memory-max-keys N    Max keys per memory-tree node (default 100)\n"
        "  --cache-pages N        Hybrid LRU index-page cache size (default 4096 pages)\n"
        "  --seed N               Reservoir-sampling seed (default 42)\n"
        "  --progress-every N     Progress interval (default 500000; 0 disables)\n"
        "  --output FILE          Results CSV (default results/results.csv)\n"
        "  --workdir DIR          Disk-tree files directory (default run_data)\n"
        "  --help                  Show this help\n\n"
        "Examples:\n"
        "  " << exe << " --mode memory --dataset data/dataset.csv --limit 100000\n"
        "  " << exe << " --mode disk --dataset data/dataset.csv --limit 100000\n"
        "  " << exe << " --mode hybrid --dataset data/dataset.csv --limit 100000 --cache-pages 4096\n";
}

std::uint64_t to_u64(const std::string& value, const std::string& option) {
    try {
        std::size_t used = 0;
        const auto n = std::stoull(value, &used);
        if (used != value.size()) throw std::invalid_argument("trailing text");
        return n;
    } catch (...) {
        throw std::runtime_error("Invalid numeric value for " + option + ": " + value);
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        bpt::BenchmarkOptions o;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto need_value = [&](const std::string& option) -> std::string {
                if (i + 1 >= argc) throw std::runtime_error("Missing value after " + option);
                return argv[++i];
            };

            if (arg == "--help" || arg == "-h") {
                print_help(argv[0]);
                return 0;
            } else if (arg == "--mode") {
                o.mode = need_value(arg);
            } else if (arg == "--dataset") {
                o.dataset = need_value(arg);
            } else if (arg == "--limit") {
                o.limit = to_u64(need_value(arg), arg);
            } else if (arg == "--search-count") {
                o.search_count = static_cast<std::size_t>(to_u64(need_value(arg), arg));
            } else if (arg == "--memory-max-keys") {
                o.memory_max_keys = static_cast<std::size_t>(to_u64(need_value(arg), arg));
            } else if (arg == "--cache-pages") {
                o.cache_pages = static_cast<std::size_t>(to_u64(need_value(arg), arg));
            } else if (arg == "--seed") {
                o.seed = to_u64(need_value(arg), arg);
            } else if (arg == "--progress-every") {
                o.progress_every = to_u64(need_value(arg), arg);
            } else if (arg == "--output") {
                o.output_csv = need_value(arg);
            } else if (arg == "--workdir") {
                o.workdir = need_value(arg);
            } else {
                throw std::runtime_error("Unknown option: " + arg + " (use --help)");
            }
        }

        if (o.mode.empty() || o.dataset.empty()) {
            print_help(argv[0]);
            return 1;
        }
        return bpt::run_benchmark(o);
    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << "\n";
        return 1;
    }
}
