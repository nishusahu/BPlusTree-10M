#pragma once

#include "common.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace bpt {

class MemoryBPlusTree {
public:
    explicit MemoryBPlusTree(std::size_t max_keys = 100);
    ~MemoryBPlusTree();

    MemoryBPlusTree(const MemoryBPlusTree&) = delete;
    MemoryBPlusTree& operator=(const MemoryBPlusTree&) = delete;

    void insert(const std::string& key, const std::string& payload);
    bool search(const std::string& key, std::string& payload_out) const;
    TreeStats stats() const;

private:
    struct Node {
        bool leaf;
        std::vector<std::string> keys;
        std::vector<Node*> children;       // internal nodes only
        std::vector<std::string> values;   // leaf nodes only
        Node* next = nullptr;              // leaf chain

        explicit Node(bool is_leaf) : leaf(is_leaf) {}
    };

    Node* root_ = nullptr;
    std::size_t max_keys_ = 100;
    std::size_t height_ = 1;
    std::uint64_t records_ = 0;
    std::uint64_t node_count_ = 0;

    Node* make_node(bool leaf);
    static void destroy(Node* node);
    void insert_into_parent(std::vector<Node*>& path, Node* left,
                            const std::string& separator, Node* right);
};

} // namespace bpt
