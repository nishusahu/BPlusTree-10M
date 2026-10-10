#pragma once

#include "common.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bpt {

class PagedBPlusTree {
public:
    static constexpr std::size_t PAGE_SIZE = 4096;
    static constexpr std::size_t HEADER_SIZE = 32;
    static constexpr std::size_t KEY_BYTES = 32;

    PagedBPlusTree(const std::filesystem::path& directory, std::size_t cache_pages);
    ~PagedBPlusTree();

    PagedBPlusTree(const PagedBPlusTree&) = delete;
    PagedBPlusTree& operator=(const PagedBPlusTree&) = delete;

    void insert(const std::string& key, const std::string& payload);
    bool search(const std::string& key, std::string& payload_out);
    TreeStats stats();
    void flush();

    std::size_t cache_pages() const { return cache_capacity_; }
    std::filesystem::path index_path() const { return index_path_; }
    std::filesystem::path payload_path() const { return payload_path_; }

private:
    struct Node {
        std::uint64_t id = 0;
        bool leaf = true;
        std::vector<std::string> keys;
        std::vector<std::uint64_t> ptrs; // leaf: payload offsets; internal: child page IDs
        std::uint64_t next_leaf = 0;
    };

    struct CacheEntry {
        Node node;
        std::list<std::uint64_t>::iterator lru_it;
    };

    std::filesystem::path directory_;
    std::filesystem::path index_path_;
    std::filesystem::path payload_path_;
    std::fstream index_file_;
    std::fstream payload_file_;

    std::uint64_t root_page_ = 0;
    std::uint64_t next_page_id_ = 1;
    std::size_t height_ = 1;
    std::uint64_t records_ = 0;

    std::size_t internal_capacity_ = 0;
    std::size_t leaf_capacity_ = 0;

    std::size_t cache_capacity_ = 0;
    std::unordered_map<std::uint64_t, CacheEntry> cache_;
    std::list<std::uint64_t> lru_;

    std::uint64_t page_reads_ = 0;
    std::uint64_t page_writes_ = 0;
    std::uint64_t payload_reads_ = 0;
    std::uint64_t payload_writes_ = 0;
    std::uint64_t cache_hits_ = 0;
    std::uint64_t cache_misses_ = 0;

    static void validate_key(const std::string& key);
    static void write_u16(char* p, std::uint16_t v);
    static void write_u32(char* p, std::uint32_t v);
    static void write_u64(char* p, std::uint64_t v);
    static std::uint16_t read_u16(const char* p);
    static std::uint32_t read_u32(const char* p);
    static std::uint64_t read_u64(const char* p);
    static void write_fixed_key(char* p, const std::string& key);
    static std::string read_fixed_key(const char* p);

    void initialize_new_files();
    void write_metadata();
    std::uint64_t allocate_page(bool leaf);

    Node read_node(std::uint64_t page_id);
    Node read_node_from_disk(std::uint64_t page_id);
    void write_node(const Node& node);
    void write_node_to_disk(const Node& node);

    void cache_put(const Node& node);
    std::optional<Node> cache_get(std::uint64_t page_id);

    std::uint64_t append_payload(const std::string& payload);
    std::string read_payload(std::uint64_t offset);

    void insert_into_parent(std::vector<std::uint64_t>& path,
                            std::uint64_t left_id,
                            const std::string& separator,
                            std::uint64_t right_id);
};

} // namespace bpt
