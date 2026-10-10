#include "memory_bptree.hpp"
#include "paged_bptree.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string key_for(int i) {
    std::ostringstream oss;
    oss << 'K' << std::setw(8) << std::setfill('0') << i;
    return oss.str();
}

std::vector<std::pair<std::string, std::string>> make_records(int n) {
    std::vector<std::pair<std::string, std::string>> rows;
    rows.reserve(n);
    for (int i = 0; i < n; ++i) {
        rows.emplace_back(key_for(i), "payload_" + std::to_string(i) + "_abcdefghijklmnopqrstuvwxyz");
    }
    std::mt19937 rng(12345);
    std::shuffle(rows.begin(), rows.end(), rng);
    return rows;
}

void require(bool cond, const std::string& message) {
    if (!cond) throw std::runtime_error("TEST FAILURE: " + message);
}

void test_memory(const std::vector<std::pair<std::string, std::string>>& rows) {
    bpt::MemoryBPlusTree tree(7); // deliberately tiny to force many splits
    for (const auto& [k, v] : rows) tree.insert(k, v);

    for (const auto& [k, v] : rows) {
        std::string got;
        require(tree.search(k, got), "memory search missed " + k);
        require(got == v, "memory payload mismatch for " + k);
    }
    std::string got;
    require(!tree.search("K99999999", got), "memory tree found a key that was never inserted");
    const auto s = tree.stats();
    require(s.records == rows.size(), "memory record count mismatch");
    require(s.height >= 2, "memory tree did not split as expected");
}

void test_paged(const std::vector<std::pair<std::string, std::string>>& rows,
                const std::filesystem::path& dir,
                std::size_t cache_pages) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    bpt::PagedBPlusTree tree(dir, cache_pages);
    for (const auto& [k, v] : rows) tree.insert(k, v);

    for (const auto& [k, v] : rows) {
        std::string got;
        require(tree.search(k, got), "paged search missed " + k);
        require(got == v, "paged payload mismatch for " + k);
    }
    std::string got;
    require(!tree.search("K99999999", got), "paged tree found a key that was never inserted");
    const auto s = tree.stats();
    require(s.records == rows.size(), "paged record count mismatch");
    require(s.height >= 2, "paged tree did not split as expected");
    require(s.disk_bytes > 0, "paged tree reported no disk usage");
    if (cache_pages > 0) require(s.cache_hits > 0, "hybrid cache never produced a hit");
}

} // namespace

int main() {
    try {
        const auto rows = make_records(5000);
        test_memory(rows);
        test_paged(rows, std::filesystem::temp_directory_path() / "bptree_test_disk", 0);
        test_paged(rows, std::filesystem::temp_directory_path() / "bptree_test_hybrid", 64);
        std::cout << "All B+ tree tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
