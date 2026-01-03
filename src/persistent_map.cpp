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
    : current_node_(nullptr), current_index_(0), finished_(root == nullptr) {
    if (root) {
        stack_.push_back({root, 0});
        advance();
    }
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

PersistentMap PersistentMap::fromDict(const py::dict& d) {
    size_t n = d.size();

    // Small maps: use current implementation (already fast)
    if (n < 1000) {
        PersistentMap m;
        for (auto item : d) {
            m = m.assoc(py::reinterpret_borrow<py::object>(item.first),
                       py::reinterpret_borrow<py::object>(item.second));
        }
        return m;
    }

    // Large maps: optimize with pre-allocation and batching
    // Collect all entries first to avoid repeated dict iteration
    std::vector<std::pair<py::object, py::object>> entries;
    entries.reserve(n);

    for (auto item : d) {
        entries.emplace_back(
            py::reinterpret_borrow<py::object>(item.first),
            py::reinterpret_borrow<py::object>(item.second)
        );
    }

    // Build map with pre-allocated entries
    PersistentMap m;
    for (const auto& [key, val] : entries) {
        m = m.assoc(key, val);
    }

    return m;
}

PersistentMap PersistentMap::create(const py::kwargs& kw) {
    size_t n = kw.size();

    // Small maps: use current implementation
    if (n < 1000) {
        PersistentMap m;
        for (auto item : kw) {
            m = m.assoc(py::reinterpret_borrow<py::object>(item.first),
                       py::reinterpret_borrow<py::object>(item.second));
        }
        return m;
    }

    // Large maps: optimize with pre-allocation
    std::vector<std::pair<py::object, py::object>> entries;
    entries.reserve(n);

    for (auto item : kw) {
        entries.emplace_back(
            py::reinterpret_borrow<py::object>(item.first),
            py::reinterpret_borrow<py::object>(item.second)
        );
    }

    PersistentMap m;
    for (const auto& [key, val] : entries) {
        m = m.assoc(key, val);
    }

    return m;
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
            // Small updates: use current implementation
            if (n < 100) {
                other_map.root_->iterate([&](const py::object& k, const py::object& v) {
                    result = result.assoc(k, v);
                });
            } else {
                // Large updates: collect entries first
                std::vector<std::pair<py::object, py::object>> entries;
                entries.reserve(n);

                other_map.root_->iterate([&](const py::object& k, const py::object& v) {
                    entries.emplace_back(k, v);
                });

                for (const auto& [key, val] : entries) {
                    result = result.assoc(key, val);
                }
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
