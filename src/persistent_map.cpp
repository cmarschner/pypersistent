#include "persistent_map.hpp"
#include <sstream>
#include <vector>
#include <memory>

// Initialize static sentinel value
py::object PersistentMap::NOT_FOUND = py::object();

//=============================================================================
// BitmapNode Implementation
//=============================================================================

py::object BitmapNode::get(uint32_t shift, uint32_t hash,
                           const py::object& key, const py::object& notFound) const {
    uint32_t bit_pos = 1 << ((hash >> shift) & HASH_MASK);

    // Check if this slot is occupied
    if ((bitmap_ & bit_pos) == 0) {
        return notFound;
    }

    // Calculate array index
    uint32_t idx = popcount(bitmap_ & (bit_pos - 1));

    const auto& elem = array_[idx];

    if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
        // It's a key-value entry
        const auto& entry = std::get<std::shared_ptr<Entry>>(elem);
        if (pmutils::keysEqual(entry->key, key)) {
            return entry->value;
        }
        return notFound;
    } else {
        // It's a child node
        NodeBase* child = std::get<NodeBase*>(elem);
        return child->get(shift + HASH_BITS, hash, key, notFound);
    }
}

NodeBase* BitmapNode::assoc(uint32_t shift, uint32_t hash,
                            const py::object& key, const py::object& val) const {
    uint32_t bit_pos = 1 << ((hash >> shift) & HASH_MASK);
    uint32_t idx = popcount(bitmap_ & (bit_pos - 1));

    if ((bitmap_ & bit_pos) != 0) {
        // Slot is occupied
        const auto& elem = array_[idx];

        if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
            // It's a key-value entry
            const auto& entry = std::get<std::shared_ptr<Entry>>(elem);

            if (pmutils::keysEqual(entry->key, key)) {
                // Same key, update value
                if (entry->value.is(val)) {
                    // Value unchanged, return same node
                    return const_cast<BitmapNode*>(this);
                }

                // Copy-on-write: copy array (cheap shared_ptr copies!) and update one entry
                std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray = array_;
                // Only increment refcount for nodes
                for (auto& e : newArray) {
                    if (std::holds_alternative<NodeBase*>(e)) {
                        std::get<NodeBase*>(e)->addRef();
                    }
                }
                newArray[idx] = std::make_shared<Entry>(key, val);
                return new BitmapNode(bitmap_, std::move(newArray));
            } else {
                // Different key, same hash slot - create a sub-node
                NodeBase* newChild = createNode(shift + HASH_BITS,
                                               entry->key, entry->value,
                                               hash, key, val);

                // Copy array and replace entry with child node
                std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray = array_;
                newChild->addRef();
                for (auto& e : newArray) {
                    if (std::holds_alternative<NodeBase*>(e)) {
                        std::get<NodeBase*>(e)->addRef();
                    }
                }
                newArray[idx] = newChild;
                return new BitmapNode(bitmap_, std::move(newArray));
            }
        } else {
            // It's a child node, recurse
            NodeBase* child = std::get<NodeBase*>(elem);
            NodeBase* newChild = child->assoc(shift + HASH_BITS, hash, key, val);

            if (newChild == child) {
                // No change
                return const_cast<BitmapNode*>(this);
            }

            // Copy array and update child node
            std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray = array_;
            newChild->addRef();
            for (auto& e : newArray) {
                if (std::holds_alternative<NodeBase*>(e)) {
                    std::get<NodeBase*>(e)->addRef();
                }
            }
            newArray[idx] = newChild;
            return new BitmapNode(bitmap_, std::move(newArray));
        }
    } else {
        // Slot is empty, insert new entry
        std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray;
        newArray.reserve(array_.size() + 1);

        // Copy elements before insertion point
        for (size_t i = 0; i < static_cast<size_t>(idx); ++i) {
            const auto& e = array_[i];
            if (std::holds_alternative<NodeBase*>(e)) {
                NodeBase* node = std::get<NodeBase*>(e);
                node->addRef();
            }
            newArray.push_back(e);
        }

        // Insert new entry
        newArray.push_back(std::make_shared<Entry>(key, val));

        // Copy elements after insertion point
        for (size_t i = idx; i < array_.size(); ++i) {
            const auto& e = array_[i];
            if (std::holds_alternative<NodeBase*>(e)) {
                NodeBase* node = std::get<NodeBase*>(e);
                node->addRef();
            }
            newArray.push_back(e);
        }

        return new BitmapNode(bitmap_ | bit_pos, std::move(newArray));
    }
}

NodeBase* BitmapNode::dissoc(uint32_t shift, uint32_t hash,
                             const py::object& key) const {
    uint32_t bit_pos = 1 << ((hash >> shift) & HASH_MASK);

    if ((bitmap_ & bit_pos) == 0) {
        // Key not in this node
        return const_cast<BitmapNode*>(this);
    }

    uint32_t idx = popcount(bitmap_ & (bit_pos - 1));
    const auto& elem = array_[idx];

    if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
        // It's a key-value entry
        const auto& entry = std::get<std::shared_ptr<Entry>>(elem);

        if (!pmutils::keysEqual(entry->key, key)) {
            // Different key, no change
            return const_cast<BitmapNode*>(this);
        }

        // Found it, remove this entry
        if (popcount(bitmap_) == 1) {
            // This was the only entry, return null
            return nullptr;
        }

        // Create new array without this entry
        std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray;
        newArray.reserve(array_.size() - 1);
        for (size_t i = 0; i < array_.size(); ++i) {
            if (i == static_cast<size_t>(idx)) {
                // Skip the removed entry
                continue;
            }
            const auto& e = array_[i];
            if (std::holds_alternative<NodeBase*>(e)) {
                NodeBase* node = std::get<NodeBase*>(e);
                node->addRef();
            }
            newArray.push_back(e);
        }

        return new BitmapNode(bitmap_ & ~bit_pos, std::move(newArray));
    } else {
        // It's a child node
        NodeBase* child = std::get<NodeBase*>(elem);
        NodeBase* newChild = child->dissoc(shift + HASH_BITS, hash, key);

        if (newChild == child) {
            // No change
            return const_cast<BitmapNode*>(this);
        }

        if (newChild == nullptr) {
            // Child is empty, remove this entry
            if (popcount(bitmap_) == 1) {
                // This was the only entry, return null
                return nullptr;
            }

            // Create new array without this entry
            std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray;
            newArray.reserve(array_.size() - 1);
            for (size_t i = 0; i < array_.size(); ++i) {
                if (i == static_cast<size_t>(idx)) {
                    // Skip the removed child
                    continue;
                }
                const auto& e = array_[i];
                if (std::holds_alternative<NodeBase*>(e)) {
                    NodeBase* node = std::get<NodeBase*>(e);
                    node->addRef();
                }
                newArray.push_back(e);
            }

            return new BitmapNode(bitmap_ & ~bit_pos, std::move(newArray));
        } else {
            // Child changed - copy array and update
            std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray;
            newArray.reserve(array_.size());
            for (size_t i = 0; i < array_.size(); ++i) {
                if (i == static_cast<size_t>(idx)) {
                    newChild->addRef();
                    newArray.push_back(newChild);
                } else {
                    const auto& e = array_[i];
                    if (std::holds_alternative<NodeBase*>(e)) {
                        std::get<NodeBase*>(e)->addRef();
                    }
                    newArray.push_back(e);
                }
            }
            return new BitmapNode(bitmap_, std::move(newArray));
        }
    }
}

void BitmapNode::iterate(const std::function<void(const py::object&, const py::object&)>& callback) const {
    for (const auto& elem : array_) {
        if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
            const auto& entry = std::get<std::shared_ptr<Entry>>(elem);
            callback(entry->key, entry->value);
        } else {
            NodeBase* node = std::get<NodeBase*>(elem);
            node->iterate(callback);
        }
    }
}

NodeBase* BitmapNode::createNode(uint32_t shift,
                                const py::object& key1, const py::object& val1,
                                uint32_t hash2, const py::object& key2, const py::object& val2) const {
    uint32_t hash1 = pmutils::hashKey(key1);

    if (shift >= 64) {
        // Too deep, use collision node
        std::vector<Entry*> entries;
        entries.push_back(new Entry(key1, val1));
        entries.push_back(new Entry(key2, val2));
        return new CollisionNode(hash1, std::move(entries));
    }

    uint32_t idx1 = (hash1 >> shift) & HASH_MASK;
    uint32_t idx2 = (hash2 >> shift) & HASH_MASK;

    if (idx1 == idx2) {
        // Same index at this level, recurse deeper
        NodeBase* child = createNode(shift + HASH_BITS, key1, val1, hash2, key2, val2);
        std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> array;
        array.push_back(child);
        child->addRef();
        return new BitmapNode(1 << idx1, std::move(array));
    } else {
        // Different indices, create node with both entries
        uint32_t bitmap = (1 << idx1) | (1 << idx2);
        std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> array;

        if (idx1 < idx2) {
            array.push_back(std::make_shared<Entry>(key1, val1));
            array.push_back(std::make_shared<Entry>(key2, val2));
        } else {
            array.push_back(std::make_shared<Entry>(key2, val2));
            array.push_back(std::make_shared<Entry>(key1, val1));
        }

        return new BitmapNode(bitmap, std::move(array));
    }
}

//=============================================================================
// CollisionNode Implementation
//=============================================================================

py::object CollisionNode::get(uint32_t shift, uint32_t hash,
                              const py::object& key, const py::object& notFound) const {
    for (Entry* entry : *entries_) {
        if (pmutils::keysEqual(entry->key, key)) {
            return entry->value;
        }
    }
    return notFound;
}

NodeBase* CollisionNode::assoc(uint32_t shift, uint32_t hash,
                               const py::object& key, const py::object& val) const {
    // Check if key already exists
    for (size_t i = 0; i < entries_->size(); ++i) {
        if (pmutils::keysEqual((*entries_)[i]->key, key)) {
            // Key exists, update value
            if ((*entries_)[i]->value.is(val)) {
                // Value unchanged
                return const_cast<CollisionNode*>(this);
            }

            // Copy-on-write: only copy the vector, reuse Entry pointers except for the changed one
            auto newEntries = std::make_shared<std::vector<Entry*>>(*entries_);
            delete (*newEntries)[i];  // Delete the old entry
            (*newEntries)[i] = new Entry(key, val);  // Replace with new entry
            return new CollisionNode(hash_, newEntries);
        }
    }

    // Key not found, append
    // Copy-on-write: copy vector and add new entry
    auto newEntries = std::make_shared<std::vector<Entry*>>(*entries_);
    newEntries->push_back(new Entry(key, val));
    return new CollisionNode(hash_, newEntries);
}

NodeBase* CollisionNode::dissoc(uint32_t shift, uint32_t hash,
                                const py::object& key) const {
    for (size_t i = 0; i < entries_->size(); ++i) {
        if (pmutils::keysEqual((*entries_)[i]->key, key)) {
            // Found it
            if (entries_->size() == 1) {
                // Last entry, return null
                return nullptr;
            }

            if (entries_->size() == 2) {
                // Only one entry left, return null (will be handled by parent)
                return nullptr;
            }

            // Create new collision node without this entry
            // Copy-on-write: copy vector and remove entry, but need to duplicate Entry objects
            // since the old node will delete them
            auto newEntries = std::make_shared<std::vector<Entry*>>();
            newEntries->reserve(entries_->size() - 1);
            for (size_t j = 0; j < entries_->size(); ++j) {
                if (j != i) {
                    newEntries->push_back(new Entry((*entries_)[j]->key, (*entries_)[j]->value));
                }
            }
            return new CollisionNode(hash_, newEntries);
        }
    }

    // Key not found
    return const_cast<CollisionNode*>(this);
}

void CollisionNode::iterate(const std::function<void(const py::object&, const py::object&)>& callback) const {
    for (Entry* entry : *entries_) {
        callback(entry->key, entry->value);
    }
}

//=============================================================================
// MapIterator Implementation - O(log n) memory tree traversal
//=============================================================================

MapIterator::MapIterator(const NodeBase* root)
    : root_(root), current_node_(nullptr), current_index_(0), finished_(root == nullptr) {
    if (root_) {
        root_->addRef();  // Keep root alive
        stack_.push_back({root_, 0});
        advance();
    }
}

MapIterator::~MapIterator() {
    if (root_) {
        root_->release();
    }
}

MapIterator::MapIterator(const MapIterator& other)
    : stack_(other.stack_), root_(other.root_),
      current_node_(other.current_node_), current_index_(other.current_index_),
      finished_(other.finished_) {
    if (root_) {
        root_->addRef();
    }
}

MapIterator& MapIterator::operator=(const MapIterator& other) {
    if (this != &other) {
        if (other.root_) other.root_->addRef();
        if (root_) root_->release();

        stack_ = other.stack_;
        root_ = other.root_;
        current_node_ = other.current_node_;
        current_index_ = other.current_index_;
        finished_ = other.finished_;
    }
    return *this;
}

MapIterator::MapIterator(MapIterator&& other) noexcept
    : stack_(std::move(other.stack_)), root_(other.root_),
      current_node_(other.current_node_), current_index_(other.current_index_),
      finished_(other.finished_) {
    other.root_ = nullptr;
    other.current_node_ = nullptr;
    other.finished_ = true;
}

MapIterator& MapIterator::operator=(MapIterator&& other) noexcept {
    if (this != &other) {
        if (root_) root_->release();

        stack_ = std::move(other.stack_);
        root_ = other.root_;
        current_node_ = other.current_node_;
        current_index_ = other.current_index_;
        finished_ = other.finished_;

        other.root_ = nullptr;
        other.current_node_ = nullptr;
        other.finished_ = true;
    }
    return *this;
}

void MapIterator::advance() {
    while (!stack_.empty()) {
        // Get current frame - don't hold references across vector modifications!
        const NodeBase* node = stack_.back().node;
        size_t idx = stack_.back().index;

        if (auto* bitmapNode = dynamic_cast<const BitmapNode*>(node)) {
            const auto& array = bitmapNode->getArray();

            while (idx < array.size()) {
                const auto& elem = array[idx];
                // Increment index in the actual stack frame
                stack_.back().index = ++idx;

                if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
                    // Found an entry!
                    current_node_ = node;
                    current_index_ = idx - 1;
                    return;
                } else {
                    // It's a child node - push onto stack
                    NodeBase* child = std::get<NodeBase*>(elem);
                    stack_.push_back({child, 0});
                    // Break to process child - start outer loop again
                    break;
                }
            }

            // If we've exhausted this node, pop it
            if (stack_.back().index >= array.size()) {
                stack_.pop_back();
            }
        } else if (auto* collisionNode = dynamic_cast<const CollisionNode*>(node)) {
            const auto& entries = collisionNode->getEntries();

            if (idx < entries.size()) {
                // Found an entry in collision node
                current_node_ = node;
                current_index_ = idx;
                stack_.back().index = idx + 1;
                return;
            } else {
                // Finished with collision node
                stack_.pop_back();
            }
        }
    }

    finished_ = true;
}

std::pair<py::object, py::object> MapIterator::next() {
    if (finished_) {
        throw py::stop_iteration();
    }

    py::object key, value;

    if (auto* bitmapNode = dynamic_cast<const BitmapNode*>(current_node_)) {
        const auto& array = bitmapNode->getArray();
        const auto& entry = std::get<std::shared_ptr<Entry>>(array[current_index_]);
        key = entry->key;
        value = entry->value;
    } else if (auto* collisionNode = dynamic_cast<const CollisionNode*>(current_node_)) {
        const auto& entries = collisionNode->getEntries();
        Entry* entry = entries[current_index_];
        key = entry->key;
        value = entry->value;
    }

    advance();
    return {key, value};
}

//=============================================================================
// PersistentMap Implementation
//=============================================================================

PersistentMap PersistentMap::assoc(const py::object& key, const py::object& val) const {
    uint32_t hash = pmutils::hashKey(key);

    if (root_ == nullptr) {
        // Empty map, create first node
        uint32_t bit_pos = 1 << (hash & HASH_MASK);
        std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> array;
        array.push_back(std::make_shared<Entry>(key, val));
        NodeBase* newRoot = new BitmapNode(bit_pos, std::move(array));
        return PersistentMap(newRoot, 1);
    }

    // Check if key already exists
    py::object oldVal = root_->get(0, hash, key, NOT_FOUND);
    NodeBase* newRoot = root_->assoc(0, hash, key, val);

    if (newRoot == root_) {
        // No change
        return *this;
    }

    size_t newCount = oldVal.is(NOT_FOUND) ? count_ + 1 : count_;
    return PersistentMap(newRoot, newCount);
}

PersistentMap PersistentMap::dissoc(const py::object& key) const {
    if (root_ == nullptr) {
        return *this;
    }

    uint32_t hash = pmutils::hashKey(key);
    py::object oldVal = root_->get(0, hash, key, NOT_FOUND);

    if (oldVal.is(NOT_FOUND)) {
        // Key not found
        return *this;
    }

    NodeBase* newRoot = root_->dissoc(0, hash, key);
    return PersistentMap(newRoot, count_ - 1);
}

py::object PersistentMap::get(const py::object& key, const py::object& default_val) const {
    if (root_ == nullptr) {
        return default_val;
    }

    uint32_t hash = pmutils::hashKey(key);
    py::object result = root_->get(0, hash, key, NOT_FOUND);

    return result.is(NOT_FOUND) ? default_val : result;
}

bool PersistentMap::contains(const py::object& key) const {
    if (root_ == nullptr) {
        return false;
    }

    uint32_t hash = pmutils::hashKey(key);
    py::object result = root_->get(0, hash, key, NOT_FOUND);
    return !result.is(NOT_FOUND);
}

KeyIterator PersistentMap::keys() const {
    return KeyIterator(root_);
}

ValueIterator PersistentMap::values() const {
    return ValueIterator(root_);
}

ItemIterator PersistentMap::items() const {
    return ItemIterator(root_);
}

// Fast materialized iteration - returns pre-allocated list
// Pre-allocates list with exact size to avoid repeated reallocation
py::list PersistentMap::itemsList() const {
    if (!root_ || count_ == 0) {
        return py::list();
    }

    // Pre-allocate Python list with exact size (critical for performance!)
    py::list result(count_);
    MapIterator iter(root_);
    size_t idx = 0;

    while (iter.hasNext()) {
        auto pair = iter.next();
        // Direct assignment instead of append (much faster)
        result[idx++] = py::make_tuple(pair.first, pair.second);
    }

    return result;
}

py::list PersistentMap::keysList() const {
    if (!root_ || count_ == 0) {
        return py::list();
    }

    // Pre-allocate list with exact size
    py::list result(count_);
    MapIterator iter(root_);
    size_t idx = 0;

    while (iter.hasNext()) {
        auto pair = iter.next();
        result[idx++] = pair.first;
    }

    return result;
}

py::list PersistentMap::valuesList() const {
    if (!root_ || count_ == 0) {
        return py::list();
    }

    // Pre-allocate list with exact size
    py::list result(count_);
    MapIterator iter(root_);
    size_t idx = 0;

    while (iter.hasNext()) {
        auto pair = iter.next();
        result[idx++] = pair.second;
    }

    return result;
}

bool PersistentMap::operator==(const PersistentMap& other) const {
    if (count_ != other.count_) {
        return false;
    }

    if (root_ == other.root_) {
        return true;
    }

    if (root_ == nullptr || other.root_ == nullptr) {
        return false;
    }

    // Check all entries match
    bool equal = true;
    root_->iterate([&](const py::object& k, const py::object& v) {
        if (!equal) return;

        py::object otherVal = other.get(k, NOT_FOUND);
        if (otherVal.is(NOT_FOUND)) {
            equal = false;
            return;
        }

        try {
            int result = PyObject_RichCompareBool(v.ptr(), otherVal.ptr(), Py_EQ);
            if (result != 1) {
                equal = false;
            }
        } catch (...) {
            equal = false;
        }
    });

    return equal;
}

std::string PersistentMap::repr() const {
    std::ostringstream oss;
    oss << "PersistentMap({";

    if (root_ != nullptr) {
        bool first = true;
        root_->iterate([&](const py::object& k, const py::object& v) {
            if (!first) {
                oss << ", ";
            }
            first = false;

            // Use Python's repr for keys and values
            py::object k_repr = py::repr(k);
            py::object v_repr = py::repr(v);
            oss << py::str(k_repr).cast<std::string>();
            oss << ": ";
            oss << py::str(v_repr).cast<std::string>();
        });
    }

    oss << "})";
    return oss.str();
}

// ============================================================================
// Phase 2: Bottom-Up Tree Construction
// ============================================================================

NodeBase* PersistentMap::buildTreeBulk(std::vector<HashedEntry>& entries,
                                       size_t start, size_t end, uint32_t shift,
                                       BulkOpArena& arena) {
    size_t count = end - start;

    // Base case: no entries
    if (count == 0) {
        return nullptr;
    }

    // Base case: single entry
    if (count == 1) {
        auto& entry = entries[start];
        uint32_t idx = (entry.hash >> shift) & HASH_MASK;
        uint32_t bitmap = 1 << idx;

        std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> array;
        array.push_back(std::make_shared<Entry>(entry.key, entry.value));

        return arena.allocate<BitmapNode>(bitmap, std::move(array));
    }

    // Check if all entries have the same hash (collision case)
    bool all_same_hash = true;
    uint32_t first_hash = entries[start].hash;
    for (size_t i = start + 1; i < end; ++i) {
        if (entries[i].hash != first_hash) {
            all_same_hash = false;
            break;
        }
    }

    if (all_same_hash) {
        // Create CollisionNode
        std::vector<Entry*> collision_entries;
        collision_entries.reserve(count);
        for (size_t i = start; i < end; ++i) {
            collision_entries.push_back(new Entry(entries[i].key, entries[i].value));
        }
        return arena.allocate<CollisionNode>(first_hash, std::move(collision_entries));
    }

    // Group entries by their hash bucket at this level
    // Buckets[i] contains entries whose (hash >> shift) & HASH_MASK == i
    std::vector<std::vector<size_t>> buckets(MAX_BITMAP_SIZE);

    for (size_t i = start; i < end; ++i) {
        uint32_t idx = (entries[i].hash >> shift) & HASH_MASK;
        buckets[idx].push_back(i);
    }

    // Build bitmap and array for this node
    uint32_t bitmap = 0;
    std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> array;

    for (uint32_t idx = 0; idx < MAX_BITMAP_SIZE; ++idx) {
        if (buckets[idx].empty()) {
            continue;
        }

        bitmap |= (1 << idx);

        if (buckets[idx].size() == 1) {
            // Single entry in this bucket - store as Entry
            size_t entry_idx = buckets[idx][0];
            array.push_back(std::make_shared<Entry>(entries[entry_idx].key,
                                                     entries[entry_idx].value));
        } else {
            // Multiple entries - need to recurse deeper or create collision node

            // Check if we've reached max depth (shift >= 30)
            if (shift >= 30) {
                // Max tree depth reached, create collision node
                std::vector<Entry*> collision_entries;
                collision_entries.reserve(buckets[idx].size());
                for (size_t entry_idx : buckets[idx]) {
                    collision_entries.push_back(new Entry(entries[entry_idx].key,
                                                         entries[entry_idx].value));
                }
                NodeBase* collision_node = arena.allocate<CollisionNode>(entries[buckets[idx][0]].hash,
                                                                          std::move(collision_entries));
                array.push_back(collision_node);
                collision_node->addRef();
            } else {
                // Create a contiguous sub-vector for recursion
                std::vector<HashedEntry> sub_entries;
                sub_entries.reserve(buckets[idx].size());
                for (size_t entry_idx : buckets[idx]) {
                    sub_entries.push_back(entries[entry_idx]);
                }

                // Recursively build subtree
                NodeBase* child = buildTreeBulk(sub_entries, 0, sub_entries.size(), shift + HASH_BITS, arena);
                if (child) {
                    child->addRef();  // Add reference for parent node
                    array.push_back(child);
                } else {
                    // Shouldn't happen, but handle gracefully
                    bitmap &= ~(1 << idx);  // Clear bit if no child created
                }
            }
        }
    }

    if (array.empty()) {
        return nullptr;
    }

    return arena.allocate<BitmapNode>(bitmap, std::move(array));
}

PersistentMap PersistentMap::fromDict(const py::dict& d) {
    size_t n = d.size();

    // Empty map
    if (n == 0) {
        return PersistentMap();
    }

    // Small maps: use current implementation (already fast)
    if (n < 1000) {
        PersistentMap m;
        for (auto item : d) {
            m = m.assoc(py::reinterpret_borrow<py::object>(item.first),
                       py::reinterpret_borrow<py::object>(item.second));
        }
        return m;
    }

    // Large maps: use bottom-up tree construction (Phase 2) + arena allocator (Phase 3)
    // Collect all entries and compute hashes upfront
    std::vector<HashedEntry> entries;
    entries.reserve(n);

    for (auto item : d) {
        py::object key = py::reinterpret_borrow<py::object>(item.first);
        py::object val = py::reinterpret_borrow<py::object>(item.second);
        uint32_t hash = pmutils::hashKey(key);

        entries.push_back(HashedEntry{hash, key, val});
    }

    // Phase 3: Create arena for fast allocation during tree construction
    BulkOpArena arena;

    // Build tree bottom-up using arena allocation
    NodeBase* root = buildTreeBulk(entries, 0, entries.size(), 0, arena);

    // CRITICAL: Arena nodes will be freed when arena goes out of scope!
    // We need to clone the entire tree from arena to heap.
    NodeBase* heap_root = root ? root->cloneToHeap() : nullptr;

    return PersistentMap(heap_root, n);
}

PersistentMap PersistentMap::create(const py::kwargs& kw) {
    size_t n = kw.size();

    // Empty map
    if (n == 0) {
        return PersistentMap();
    }

    // Small maps: use current implementation
    if (n < 1000) {
        PersistentMap m;
        for (auto item : kw) {
            m = m.assoc(py::reinterpret_borrow<py::object>(item.first),
                       py::reinterpret_borrow<py::object>(item.second));
        }
        return m;
    }

    // Large maps: use bottom-up tree construction (Phase 2) + arena allocator (Phase 3)
    std::vector<HashedEntry> entries;
    entries.reserve(n);

    for (auto item : kw) {
        py::object key = py::reinterpret_borrow<py::object>(item.first);
        py::object val = py::reinterpret_borrow<py::object>(item.second);
        uint32_t hash = pmutils::hashKey(key);

        entries.push_back(HashedEntry{hash, key, val});
    }

    // Phase 3: Create arena for fast allocation
    BulkOpArena arena;

    // Build tree bottom-up using arena
    NodeBase* root = buildTreeBulk(entries, 0, entries.size(), 0, arena);

    // Clone from arena to heap
    NodeBase* heap_root = root ? root->cloneToHeap() : nullptr;

    return PersistentMap(heap_root, n);
}

PersistentMap PersistentMap::update(const py::object& other) const {
    PersistentMap result = *this;

    // Handle dict or PersistentMap
    if (py::isinstance<py::dict>(other)) {
        py::dict d = other.cast<py::dict>();
        size_t n = d.size();

        // Small updates: use current implementation
        if (n < 100) {
            for (auto item : d) {
                result = result.assoc(py::reinterpret_borrow<py::object>(item.first),
                                     py::reinterpret_borrow<py::object>(item.second));
            }
        } else {
            // Large updates: pre-allocate entries
            std::vector<std::pair<py::object, py::object>> entries;
            entries.reserve(n);

            for (auto item : d) {
                entries.emplace_back(
                    py::reinterpret_borrow<py::object>(item.first),
                    py::reinterpret_borrow<py::object>(item.second)
                );
            }

            for (const auto& [key, val] : entries) {
                result = result.assoc(key, val);
            }
        }
    } else if (py::isinstance<PersistentMap>(other)) {
        const PersistentMap& other_map = other.cast<const PersistentMap&>();
        size_t n = other_map.count_;

        if (other_map.root_) {
            // Phase 4: Use structural merge for large updates
            if (n >= 100) {
                // Structural merge - O(n + m) instead of O(n * log m)
                NodeBase* merged = mergeNodes(root_, other_map.root_, 0);
                // Count actual entries in merged tree (handles overwrites correctly)
                size_t actual_count = 0;
                if (merged) {
                    merged->iterate([&](const py::object&, const py::object&) {
                        actual_count++;
                    });
                }
                return PersistentMap(merged, actual_count);
            } else {
                // Small updates: use iterative assoc (simpler, fine for small n)
                other_map.root_->iterate([&](const py::object& k, const py::object& v) {
                    result = result.assoc(k, v);
                });
            }
        }
    } else {
        // Try to iterate as a mapping
        try {
            py::object items = other.attr("items")();
            for (auto item : items) {
                py::tuple kv = item.cast<py::tuple>();
                result = result.assoc(kv[0], kv[1]);
            }
        } catch (...) {
            throw py::type_error("update() requires a dict, PersistentMap, or mapping");
        }
    }

    return result;
}

// ============================================================================
// Phase 3: Arena-to-Heap Node Transfer (cloneToHeap implementations)
// ============================================================================

NodeBase* BitmapNode::cloneToHeap() const {
    // Clone the array, recursively cloning any child nodes
    std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> new_array;
    new_array.reserve(array_.size());

    for (const auto& elem : array_) {
        if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
            // Entry is already heap-allocated via shared_ptr, just copy
            new_array.push_back(elem);
        } else {
            // NodeBase* - recursively clone child node to heap
            NodeBase* child = std::get<NodeBase*>(elem);
            NodeBase* heap_child = child->cloneToHeap();
            heap_child->addRef();  // Add reference for parent
            new_array.push_back(heap_child);
        }
    }

    // Allocate new BitmapNode on heap
    return new BitmapNode(bitmap_, std::move(new_array));
}

NodeBase* CollisionNode::cloneToHeap() const {
    // Clone the entries vector (Entry* are already heap-allocated)
    // Note: entries_ is a shared_ptr, so we can just copy it
    // The shared_ptr will keep the vector alive
    return new CollisionNode(hash_, entries_);
}

// ============================================================================
// Phase 4: Structural Merge Implementation
// ============================================================================

/**
 * Structural merge of two HAMT trees
 *
 * Instead of iterating and calling assoc() repeatedly, we merge trees
 * structurally at the node level. This maximizes structural sharing and
 * reduces allocations.
 *
 * Algorithm:
 * 1. For BitmapNode+BitmapNode: combine bitmaps, merge arrays slot-by-slot
 * 2. For BitmapNode+CollisionNode: handle hash collision cases
 * 3. For CollisionNode+CollisionNode: merge entry lists
 * 4. Recursively merge child nodes where trees overlap
 *
 * Performance: O(n + m) instead of O(n * log m)
 */
NodeBase* PersistentMap::mergeNodes(NodeBase* left, NodeBase* right, uint32_t shift) {
    // Handle null cases
    if (!left) {
        // Return right without addRef - caller will handle ownership
        return right;
    }
    if (!right) {
        // Return left without addRef - caller will handle ownership
        return left;
    }

    // Both nodes exist - determine types and merge appropriately
    BitmapNode* leftBitmap = dynamic_cast<BitmapNode*>(left);
    BitmapNode* rightBitmap = dynamic_cast<BitmapNode*>(right);
    CollisionNode* leftCollision = dynamic_cast<CollisionNode*>(left);
    CollisionNode* rightCollision = dynamic_cast<CollisionNode*>(right);

    // Case 1: BitmapNode + BitmapNode (most common)
    if (leftBitmap && rightBitmap) {
        uint32_t leftBmp = leftBitmap->getBitmap();
        uint32_t rightBmp = rightBitmap->getBitmap();
        uint32_t combinedBmp = leftBmp | rightBmp;  // Union of bitmaps

        const auto& leftArray = leftBitmap->getArray();
        const auto& rightArray = rightBitmap->getArray();

        std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> newArray;
        newArray.reserve(popcount(combinedBmp));

        uint32_t leftIdx = 0;
        uint32_t rightIdx = 0;

        // Iterate through all possible slots (32 max)
        for (uint32_t bit = 0; bit < MAX_BITMAP_SIZE; ++bit) {
            uint32_t mask = 1u << bit;

            if (combinedBmp & mask) {
                bool inLeft = (leftBmp & mask) != 0;
                bool inRight = (rightBmp & mask) != 0;

                if (inLeft && inRight) {
                    // Both trees have this slot - need to merge
                    const auto& leftElem = leftArray[leftIdx];
                    const auto& rightElem = rightArray[rightIdx];

                    if (std::holds_alternative<std::shared_ptr<Entry>>(leftElem) &&
                        std::holds_alternative<std::shared_ptr<Entry>>(rightElem)) {
                        // Both are entries - right wins (overwrite semantics)
                        newArray.push_back(rightElem);
                    } else if (std::holds_alternative<NodeBase*>(leftElem) &&
                               std::holds_alternative<NodeBase*>(rightElem)) {
                        // Both are nodes - recursively merge
                        NodeBase* leftChild = std::get<NodeBase*>(leftElem);
                        NodeBase* rightChild = std::get<NodeBase*>(rightElem);
                        NodeBase* merged = mergeNodes(leftChild, rightChild, shift + HASH_BITS);
                        newArray.push_back(merged);
                        merged->addRef();  // Must increment refcount for array ownership
                    } else {
                        // Mixed entry/node - right wins
                        if (std::holds_alternative<std::shared_ptr<Entry>>(rightElem)) {
                            newArray.push_back(rightElem);
                        } else {
                            NodeBase* node = std::get<NodeBase*>(rightElem);
                            node->addRef();
                            newArray.push_back(node);
                        }
                    }

                    leftIdx++;
                    rightIdx++;
                } else if (inLeft) {
                    // Only in left tree - reuse
                    const auto& elem = leftArray[leftIdx];
                    if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
                        newArray.push_back(elem);
                    } else {
                        NodeBase* node = std::get<NodeBase*>(elem);
                        node->addRef();
                        newArray.push_back(node);
                    }
                    leftIdx++;
                } else {
                    // Only in right tree - reuse
                    const auto& elem = rightArray[rightIdx];
                    if (std::holds_alternative<std::shared_ptr<Entry>>(elem)) {
                        newArray.push_back(elem);
                    } else {
                        NodeBase* node = std::get<NodeBase*>(elem);
                        node->addRef();
                        newArray.push_back(node);
                    }
                    rightIdx++;
                }
            }
        }

        return new BitmapNode(combinedBmp, std::move(newArray));
    }

    // Case 2: CollisionNode + CollisionNode
    if (leftCollision && rightCollision) {
        // Both are collision nodes - merge entry lists
        // Right entries overwrite left entries with same key
        std::vector<Entry*> mergedEntries;
        const auto& leftEntries = leftCollision->getEntries();
        const auto& rightEntries = rightCollision->getEntries();

        // Start with left entries
        for (Entry* leftEntry : leftEntries) {
            bool overwritten = false;
            // Check if right has same key
            for (Entry* rightEntry : rightEntries) {
                if (pmutils::keysEqual(leftEntry->key, rightEntry->key)) {
                    overwritten = true;
                    break;
                }
            }
            if (!overwritten) {
                mergedEntries.push_back(new Entry(leftEntry->key, leftEntry->value));
            }
        }

        // Add all right entries (they override)
        for (Entry* rightEntry : rightEntries) {
            mergedEntries.push_back(new Entry(rightEntry->key, rightEntry->value));
        }

        return new CollisionNode(leftCollision->getHash(), std::move(mergedEntries));
    }

    // Case 3: Mixed BitmapNode + CollisionNode
    // Fall back to iterative assoc for mixed cases (rare)
    if (leftCollision || rightCollision) {
        // Use assoc to add entries from one into the other
        // This is a rare case, so performance impact is minimal
        NodeBase* result = left;
        result->addRef();

        if (rightBitmap) {
            // Right is bitmap, left is collision - add collision entries
            leftCollision->iterate([&](const py::object& k, const py::object& v) {
                uint32_t hash = pmutils::hashKey(k);
                NodeBase* newResult = result->assoc(0, hash, k, v);
                result->release();
                result = newResult;
            });
        } else if (rightCollision) {
            // Right is collision - add its entries
            rightCollision->iterate([&](const py::object& k, const py::object& v) {
                uint32_t hash = pmutils::hashKey(k);
                NodeBase* newResult = result->assoc(0, hash, k, v);
                result->release();
                result = newResult;
            });
        }

        return result;
    }

    // Should never reach here
    left->addRef();
    return left;
}
