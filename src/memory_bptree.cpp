#include "memory_bptree.hpp"

#include <algorithm>
#include <stdexcept>

namespace bpt {

MemoryBPlusTree::MemoryBPlusTree(std::size_t max_keys) : max_keys_(max_keys) {
    if (max_keys_ < 3) {
        throw std::invalid_argument("Memory B+ tree max_keys must be at least 3");
    }
    root_ = make_node(true);
}

MemoryBPlusTree::~MemoryBPlusTree() {
    destroy(root_);
}

MemoryBPlusTree::Node* MemoryBPlusTree::make_node(bool leaf) {
    ++node_count_;
    return new Node(leaf);
}

void MemoryBPlusTree::destroy(Node* node) {
    if (!node) return;
    if (!node->leaf) {
        for (Node* child : node->children) {
            destroy(child);
        }
    }
    delete node;
}

bool MemoryBPlusTree::search(const std::string& key, std::string& payload_out) const {
    const Node* node = root_;
    while (!node->leaf) {
        const auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key);
        const std::size_t idx = static_cast<std::size_t>(it - node->keys.begin());
        node = node->children[idx];
    }

    const auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    if (it == node->keys.end() || *it != key) return false;
    const std::size_t idx = static_cast<std::size_t>(it - node->keys.begin());
    payload_out = node->values[idx];
    return true;
}

void MemoryBPlusTree::insert(const std::string& key, const std::string& payload) {
    Node* node = root_;
    std::vector<Node*> path;

    while (!node->leaf) {
        path.push_back(node);
        const auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key);
        const std::size_t idx = static_cast<std::size_t>(it - node->keys.begin());
        node = node->children[idx];
    }

    const auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    const std::size_t pos = static_cast<std::size_t>(it - node->keys.begin());
    if (it != node->keys.end() && *it == key) {
        throw std::runtime_error("Duplicate key encountered in dataset: " + key);
    }

    node->keys.insert(node->keys.begin() + static_cast<std::ptrdiff_t>(pos), key);
    node->values.insert(node->values.begin() + static_cast<std::ptrdiff_t>(pos), payload);
    ++records_;

    if (node->keys.size() <= max_keys_) return;

    Node* right = make_node(true);
    const std::size_t split = node->keys.size() / 2;

    right->keys.assign(std::make_move_iterator(node->keys.begin() + static_cast<std::ptrdiff_t>(split)),
                       std::make_move_iterator(node->keys.end()));
    right->values.assign(std::make_move_iterator(node->values.begin() + static_cast<std::ptrdiff_t>(split)),
                         std::make_move_iterator(node->values.end()));
    node->keys.resize(split);
    node->values.resize(split);

    right->next = node->next;
    node->next = right;

    const std::string separator = right->keys.front();
    insert_into_parent(path, node, separator, right);
}

void MemoryBPlusTree::insert_into_parent(std::vector<Node*>& path, Node* left,
                                         const std::string& separator, Node* right) {
    if (path.empty()) {
        Node* new_root = make_node(false);
        new_root->keys.push_back(separator);
        new_root->children.push_back(left);
        new_root->children.push_back(right);
        root_ = new_root;
        ++height_;
        return;
    }

    Node* parent = path.back();
    path.pop_back();

    const auto child_it = std::find(parent->children.begin(), parent->children.end(), left);
    if (child_it == parent->children.end()) {
        throw std::runtime_error("Internal error: parent/child relationship lost");
    }
    const std::size_t child_pos = static_cast<std::size_t>(child_it - parent->children.begin());

    parent->keys.insert(parent->keys.begin() + static_cast<std::ptrdiff_t>(child_pos), separator);
    parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(child_pos + 1), right);

    if (parent->keys.size() <= max_keys_) return;

    Node* new_right = make_node(false);
    const std::size_t mid = parent->keys.size() / 2;
    const std::string promote = parent->keys[mid];

    new_right->keys.assign(
        std::make_move_iterator(parent->keys.begin() + static_cast<std::ptrdiff_t>(mid + 1)),
        std::make_move_iterator(parent->keys.end()));
    new_right->children.assign(
        parent->children.begin() + static_cast<std::ptrdiff_t>(mid + 1),
        parent->children.end());

    parent->keys.resize(mid);
    parent->children.resize(mid + 1);

    insert_into_parent(path, parent, promote, new_right);
}

TreeStats MemoryBPlusTree::stats() const {
    TreeStats s;
    s.records = records_;
    s.nodes_or_pages = node_count_;
    s.height = height_;
    s.internal_capacity = max_keys_;
    s.leaf_capacity = max_keys_;
    return s;
}

} // namespace bpt
