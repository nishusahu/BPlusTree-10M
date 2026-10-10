#include "paged_bptree.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace bpt {
namespace {
constexpr std::uint64_t META_MAGIC = 0x4250545245453031ULL; // "BPTREE01"
constexpr std::uint32_t META_VERSION = 1;
}

PagedBPlusTree::PagedBPlusTree(const std::filesystem::path& directory, std::size_t cache_pages)
    : directory_(directory),
      index_path_(directory / "index.dat"),
      payload_path_(directory / "payload.dat"),
      cache_capacity_(cache_pages) {

    std::filesystem::create_directories(directory_);

    // Page layout:
    // leaf     = 32-byte header + N * (32-byte key + 8-byte payload offset)
    // internal = 32-byte header + 8-byte first child + N * (32-byte key + 8-byte right child)
    leaf_capacity_ = (PAGE_SIZE - HEADER_SIZE) / (KEY_BYTES + sizeof(std::uint64_t));
    internal_capacity_ = (PAGE_SIZE - HEADER_SIZE - sizeof(std::uint64_t)) /
                         (KEY_BYTES + sizeof(std::uint64_t));

    if (leaf_capacity_ < 3 || internal_capacity_ < 3) {
        throw std::runtime_error("Page layout produced an invalid B+ tree capacity");
    }

    initialize_new_files();

    root_page_ = allocate_page(true);
    Node root;
    root.id = root_page_;
    root.leaf = true;
    write_node(root);
    write_metadata();
}

PagedBPlusTree::~PagedBPlusTree() {
    try {
        flush();
    } catch (...) {
        // Destructors must not throw.
    }
}

void PagedBPlusTree::validate_key(const std::string& key) {
    if (key.empty()) {
        throw std::runtime_error("Empty keys are not supported");
    }
    if (key.size() > KEY_BYTES) {
        throw std::runtime_error(
            "Key '" + key + "' is longer than the configured maximum of " +
            std::to_string(KEY_BYTES) + " bytes");
    }
    if (key.find('\0') != std::string::npos) {
        throw std::runtime_error("Keys containing NUL bytes are not supported");
    }
}

void PagedBPlusTree::write_u16(char* p, std::uint16_t v) { std::memcpy(p, &v, sizeof(v)); }
void PagedBPlusTree::write_u32(char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof(v)); }
void PagedBPlusTree::write_u64(char* p, std::uint64_t v) { std::memcpy(p, &v, sizeof(v)); }
std::uint16_t PagedBPlusTree::read_u16(const char* p) { std::uint16_t v; std::memcpy(&v, p, sizeof(v)); return v; }
std::uint32_t PagedBPlusTree::read_u32(const char* p) { std::uint32_t v; std::memcpy(&v, p, sizeof(v)); return v; }
std::uint64_t PagedBPlusTree::read_u64(const char* p) { std::uint64_t v; std::memcpy(&v, p, sizeof(v)); return v; }

void PagedBPlusTree::write_fixed_key(char* p, const std::string& key) {
    std::memset(p, 0, KEY_BYTES);
    std::memcpy(p, key.data(), key.size());
}

std::string PagedBPlusTree::read_fixed_key(const char* p) {
    std::size_t len = 0;
    while (len < KEY_BYTES && p[len] != '\0') ++len;
    return std::string(p, len);
}

void PagedBPlusTree::initialize_new_files() {
    index_file_.open(index_path_, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!index_file_) throw std::runtime_error("Could not create " + index_path_.string());

    payload_file_.open(payload_path_, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!payload_file_) throw std::runtime_error("Could not create " + payload_path_.string());

    // Reserve page 0 for metadata.
    std::array<char, PAGE_SIZE> zero{};
    index_file_.write(zero.data(), static_cast<std::streamsize>(zero.size()));
    if (!index_file_) throw std::runtime_error("Failed to initialize metadata page");
    index_file_.flush();
}

void PagedBPlusTree::write_metadata() {
    std::array<char, PAGE_SIZE> page{};
    write_u64(page.data() + 0, META_MAGIC);
    write_u32(page.data() + 8, META_VERSION);
    write_u32(page.data() + 12, static_cast<std::uint32_t>(PAGE_SIZE));
    write_u64(page.data() + 16, root_page_);
    write_u64(page.data() + 24, next_page_id_);
    write_u64(page.data() + 32, static_cast<std::uint64_t>(height_));
    write_u64(page.data() + 40, records_);
    write_u32(page.data() + 48, static_cast<std::uint32_t>(KEY_BYTES));
    write_u32(page.data() + 52, static_cast<std::uint32_t>(leaf_capacity_));
    write_u32(page.data() + 56, static_cast<std::uint32_t>(internal_capacity_));

    index_file_.clear();
    index_file_.seekp(0, std::ios::beg);
    index_file_.write(page.data(), static_cast<std::streamsize>(page.size()));
    if (!index_file_) throw std::runtime_error("Failed to write B+ tree metadata");
}

std::uint64_t PagedBPlusTree::allocate_page(bool /*leaf*/) {
    const std::uint64_t id = next_page_id_++;
    // Do not physically extend here; write_node() writes the page.
    // Metadata is checkpointed when the root changes and on flush().
    return id;
}

std::optional<PagedBPlusTree::Node> PagedBPlusTree::cache_get(std::uint64_t page_id) {
    if (cache_capacity_ == 0) return std::nullopt;
    auto it = cache_.find(page_id);
    if (it == cache_.end()) return std::nullopt;

    lru_.erase(it->second.lru_it);
    lru_.push_front(page_id);
    it->second.lru_it = lru_.begin();
    return it->second.node;
}

void PagedBPlusTree::cache_put(const Node& node) {
    if (cache_capacity_ == 0) return;

    auto it = cache_.find(node.id);
    if (it != cache_.end()) {
        it->second.node = node;
        lru_.erase(it->second.lru_it);
        lru_.push_front(node.id);
        it->second.lru_it = lru_.begin();
        return;
    }

    if (cache_.size() >= cache_capacity_) {
        const std::uint64_t victim = lru_.back();
        lru_.pop_back();
        cache_.erase(victim);
    }

    lru_.push_front(node.id);
    cache_.emplace(node.id, CacheEntry{node, lru_.begin()});
}

PagedBPlusTree::Node PagedBPlusTree::read_node(std::uint64_t page_id) {
    if (cache_capacity_ > 0) {
        auto cached = cache_get(page_id);
        if (cached) {
            ++cache_hits_;
            return *cached;
        }
        ++cache_misses_;
    }

    Node node = read_node_from_disk(page_id);
    cache_put(node);
    return node;
}

PagedBPlusTree::Node PagedBPlusTree::read_node_from_disk(std::uint64_t page_id) {
    if (page_id == 0 || page_id >= next_page_id_) {
        throw std::runtime_error("Attempt to read invalid page ID " + std::to_string(page_id));
    }

    std::array<char, PAGE_SIZE> page{};
    const std::uint64_t offset = page_id * PAGE_SIZE;
    index_file_.clear();
    index_file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    index_file_.read(page.data(), static_cast<std::streamsize>(page.size()));
    if (index_file_.gcount() != static_cast<std::streamsize>(page.size())) {
        throw std::runtime_error("Short read while reading page " + std::to_string(page_id));
    }
    ++page_reads_;

    Node node;
    node.id = page_id;
    const std::uint8_t type = static_cast<std::uint8_t>(page[0]);
    if (type != 1 && type != 2) {
        throw std::runtime_error("Corrupt page type at page " + std::to_string(page_id));
    }
    node.leaf = (type == 1);
    const std::uint16_t count = read_u16(page.data() + 2);
    node.next_leaf = read_u64(page.data() + 8);

    if (node.leaf) {
        if (count > leaf_capacity_) throw std::runtime_error("Corrupt leaf key count");
        node.keys.reserve(count);
        node.ptrs.reserve(count);
        std::size_t pos = HEADER_SIZE;
        for (std::size_t i = 0; i < count; ++i) {
            node.keys.push_back(read_fixed_key(page.data() + pos));
            pos += KEY_BYTES;
            node.ptrs.push_back(read_u64(page.data() + pos));
            pos += sizeof(std::uint64_t);
        }
    } else {
        if (count > internal_capacity_) throw std::runtime_error("Corrupt internal key count");
        node.keys.reserve(count);
        node.ptrs.reserve(static_cast<std::size_t>(count) + 1);
        std::size_t pos = HEADER_SIZE;
        node.ptrs.push_back(read_u64(page.data() + pos));
        pos += sizeof(std::uint64_t);
        for (std::size_t i = 0; i < count; ++i) {
            node.keys.push_back(read_fixed_key(page.data() + pos));
            pos += KEY_BYTES;
            node.ptrs.push_back(read_u64(page.data() + pos));
            pos += sizeof(std::uint64_t);
        }
    }

    return node;
}

void PagedBPlusTree::write_node(const Node& node) {
    write_node_to_disk(node); // write-through cache: durable/simple and easy to reason about
    cache_put(node);
}

void PagedBPlusTree::write_node_to_disk(const Node& node) {
    std::array<char, PAGE_SIZE> page{};
    page[0] = static_cast<char>(node.leaf ? 1 : 2);
    const std::size_t count = node.keys.size();
    if (count > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("Too many keys for serialized page");
    }
    write_u16(page.data() + 2, static_cast<std::uint16_t>(count));
    write_u64(page.data() + 8, node.next_leaf);

    std::size_t pos = HEADER_SIZE;
    if (node.leaf) {
        if (count > leaf_capacity_ || node.ptrs.size() != count) {
            throw std::runtime_error("Invalid leaf node shape before serialization");
        }
        for (std::size_t i = 0; i < count; ++i) {
            write_fixed_key(page.data() + pos, node.keys[i]);
            pos += KEY_BYTES;
            write_u64(page.data() + pos, node.ptrs[i]);
            pos += sizeof(std::uint64_t);
        }
    } else {
        if (count > internal_capacity_ || node.ptrs.size() != count + 1) {
            throw std::runtime_error("Invalid internal node shape before serialization");
        }
        write_u64(page.data() + pos, node.ptrs[0]);
        pos += sizeof(std::uint64_t);
        for (std::size_t i = 0; i < count; ++i) {
            write_fixed_key(page.data() + pos, node.keys[i]);
            pos += KEY_BYTES;
            write_u64(page.data() + pos, node.ptrs[i + 1]);
            pos += sizeof(std::uint64_t);
        }
    }

    const std::uint64_t offset = node.id * PAGE_SIZE;
    index_file_.clear();
    index_file_.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    index_file_.write(page.data(), static_cast<std::streamsize>(page.size()));
    if (!index_file_) throw std::runtime_error("Failed to write page " + std::to_string(node.id));
    ++page_writes_;
}

std::uint64_t PagedBPlusTree::append_payload(const std::string& payload) {
    if (payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Payload too large");
    }

    payload_file_.clear();
    payload_file_.seekp(0, std::ios::end);
    const auto pos = payload_file_.tellp();
    if (pos < 0) throw std::runtime_error("Failed to determine payload file position");
    const std::uint64_t offset = static_cast<std::uint64_t>(pos);
    const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
    payload_file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
    payload_file_.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!payload_file_) throw std::runtime_error("Failed to append payload");
    ++payload_writes_;
    return offset;
}

std::string PagedBPlusTree::read_payload(std::uint64_t offset) {
    payload_file_.clear();
    payload_file_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    std::uint32_t len = 0;
    payload_file_.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!payload_file_) throw std::runtime_error("Failed to read payload length");
    std::string payload(len, '\0');
    if (len > 0) payload_file_.read(payload.data(), static_cast<std::streamsize>(len));
    if (!payload_file_) throw std::runtime_error("Failed to read payload data");
    ++payload_reads_;
    return payload;
}

bool PagedBPlusTree::search(const std::string& key, std::string& payload_out) {
    validate_key(key);
    Node node = read_node(root_page_);
    while (!node.leaf) {
        const auto it = std::upper_bound(node.keys.begin(), node.keys.end(), key);
        const std::size_t idx = static_cast<std::size_t>(it - node.keys.begin());
        node = read_node(node.ptrs[idx]);
    }

    const auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key);
    if (it == node.keys.end() || *it != key) return false;
    const std::size_t idx = static_cast<std::size_t>(it - node.keys.begin());
    payload_out = read_payload(node.ptrs[idx]);
    return true;
}

void PagedBPlusTree::insert(const std::string& key, const std::string& payload) {
    validate_key(key);

    Node node = read_node(root_page_);
    std::vector<std::uint64_t> path;
    while (!node.leaf) {
        path.push_back(node.id);
        const auto it = std::upper_bound(node.keys.begin(), node.keys.end(), key);
        const std::size_t idx = static_cast<std::size_t>(it - node.keys.begin());
        node = read_node(node.ptrs[idx]);
    }

    const auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key);
    const std::size_t pos = static_cast<std::size_t>(it - node.keys.begin());
    if (it != node.keys.end() && *it == key) {
        throw std::runtime_error("Duplicate key encountered in dataset: " + key);
    }

    const std::uint64_t payload_offset = append_payload(payload);
    node.keys.insert(node.keys.begin() + static_cast<std::ptrdiff_t>(pos), key);
    node.ptrs.insert(node.ptrs.begin() + static_cast<std::ptrdiff_t>(pos), payload_offset);
    ++records_;

    if (node.keys.size() <= leaf_capacity_) {
        write_node(node);
        return;
    }

    Node right;
    right.id = allocate_page(true);
    right.leaf = true;
    const std::size_t split = node.keys.size() / 2;

    right.keys.assign(std::make_move_iterator(node.keys.begin() + static_cast<std::ptrdiff_t>(split)),
                      std::make_move_iterator(node.keys.end()));
    right.ptrs.assign(node.ptrs.begin() + static_cast<std::ptrdiff_t>(split), node.ptrs.end());
    node.keys.resize(split);
    node.ptrs.resize(split);

    right.next_leaf = node.next_leaf;
    node.next_leaf = right.id;

    write_node(node);
    write_node(right);

    const std::string separator = right.keys.front();
    insert_into_parent(path, node.id, separator, right.id);
}

void PagedBPlusTree::insert_into_parent(std::vector<std::uint64_t>& path,
                                        std::uint64_t left_id,
                                        const std::string& separator,
                                        std::uint64_t right_id) {
    if (path.empty()) {
        Node new_root;
        new_root.id = allocate_page(false);
        new_root.leaf = false;
        new_root.keys.push_back(separator);
        new_root.ptrs.push_back(left_id);
        new_root.ptrs.push_back(right_id);
        write_node(new_root);
        root_page_ = new_root.id;
        ++height_;
        write_metadata();
        return;
    }

    const std::uint64_t parent_id = path.back();
    path.pop_back();
    Node parent = read_node(parent_id);
    if (parent.leaf) throw std::runtime_error("Internal error: expected parent page");

    const auto child_it = std::find(parent.ptrs.begin(), parent.ptrs.end(), left_id);
    if (child_it == parent.ptrs.end()) {
        throw std::runtime_error("Internal error: parent does not reference split child");
    }
    const std::size_t child_pos = static_cast<std::size_t>(child_it - parent.ptrs.begin());

    parent.keys.insert(parent.keys.begin() + static_cast<std::ptrdiff_t>(child_pos), separator);
    parent.ptrs.insert(parent.ptrs.begin() + static_cast<std::ptrdiff_t>(child_pos + 1), right_id);

    if (parent.keys.size() <= internal_capacity_) {
        write_node(parent);
        return;
    }

    Node new_right;
    new_right.id = allocate_page(false);
    new_right.leaf = false;

    const std::size_t mid = parent.keys.size() / 2;
    const std::string promote = parent.keys[mid];

    new_right.keys.assign(
        std::make_move_iterator(parent.keys.begin() + static_cast<std::ptrdiff_t>(mid + 1)),
        std::make_move_iterator(parent.keys.end()));
    new_right.ptrs.assign(parent.ptrs.begin() + static_cast<std::ptrdiff_t>(mid + 1),
                          parent.ptrs.end());

    parent.keys.resize(mid);
    parent.ptrs.resize(mid + 1);

    write_node(parent);
    write_node(new_right);

    insert_into_parent(path, parent.id, promote, new_right.id);
}

void PagedBPlusTree::flush() {
    write_metadata();
    index_file_.flush();
    payload_file_.flush();
}

TreeStats PagedBPlusTree::stats() {
    flush();
    TreeStats s;
    s.records = records_;
    s.nodes_or_pages = (next_page_id_ > 0) ? (next_page_id_ - 1) : 0;
    s.page_reads = page_reads_;
    s.page_writes = page_writes_;
    s.payload_reads = payload_reads_;
    s.payload_writes = payload_writes_;
    s.cache_hits = cache_hits_;
    s.cache_misses = cache_misses_;
    s.height = height_;
    s.internal_capacity = internal_capacity_;
    s.leaf_capacity = leaf_capacity_;
    std::error_code ec;
    const auto a = std::filesystem::file_size(index_path_, ec);
    if (!ec) s.disk_bytes += a;
    ec.clear();
    const auto b = std::filesystem::file_size(payload_path_, ec);
    if (!ec) s.disk_bytes += b;
    return s;
}

} // namespace bpt
