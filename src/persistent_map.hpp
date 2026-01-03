#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <atomic>
#include <vector>
#include <variant>
#include <memory>
#include <functional>
#include <cstdint>
#include <string>
#include "arena_allocator.hpp"

namespace py = pybind11;

// Constants for HAMT structure
constexpr uint32_t HASH_BITS = 5;
constexpr uint32_t HASH_MASK = (1 << HASH_BITS) - 1;  // 0b11111
constexpr uint32_t MAX_BITMAP_SIZE = 1 << HASH_BITS;  // 32

// Forward declarations
class BitmapNode;
class CollisionNode;

// Utility functions for popcount (bit counting)
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    inline uint32_t popcount(uint32_t x) {
        return _mm_popcnt_u32(x);  // POPCNT instruction
    }
#elif defined(__GNUC__) || defined(__clang__)
    inline uint32_t popcount(uint32_t x) {
        return __builtin_popcount(x);  // Compiler intrinsic
    }
#else
    // Fallback implementation
    inline uint32_t popcount(uint32_t x) {
        x = x - ((x >> 1) & 0x55555555);
        x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
        return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
    }
#endif

// Python utility functions
namespace pmutils {
    inline uint32_t hashKey(const py::object& key) {
        Py_hash_t h = PyObject_Hash(key.ptr());
        if (h == -1) {
            throw py::error_already_set();
        }
        // Convert to positive uint32
        return static_cast<uint32_t>(h >= 0 ? h : -h);
    }

    inline bool keysEqual(const py::object& k1, const py::object& k2) {
        // Fast path: same object
        if (k1.is(k2)) return true;

        // Use Python's rich comparison
        int result = PyObject_RichCompareBool(k1.ptr(), k2.ptr(), Py_EQ);
        if (result == -1) {
            throw py::error_already_set();
        }
        return result == 1;
    }
}

// Entry structure for key-value pairs
struct Entry {
    py::object key;
    py::object value;

    Entry(const py::object& k, const py::object& v) : key(k), value(v) {}
};

// Abstract base class for all node types with intrusive reference counting
class NodeBase {
protected:
    mutable std::atomic<uint32_t> refcount_;

public:
    NodeBase() : refcount_(0) {}
    virtual ~NodeBase() = default;

    // Reference counting
    void addRef() const {
        refcount_.fetch_add(1, std::memory_order_relaxed);
    }

    void release() const {
        if (refcount_.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
    }

    uint32_t getRefCount() const {
        return refcount_.load(std::memory_order_relaxed);
    }

    // Pure virtual methods that all nodes must implement
    virtual py::object get(uint32_t shift, uint32_t hash,
                          const py::object& key, const py::object& notFound) const = 0;

    virtual NodeBase* assoc(uint32_t shift, uint32_t hash,
                           const py::object& key, const py::object& val) const = 0;

    virtual NodeBase* dissoc(uint32_t shift, uint32_t hash,
                            const py::object& key) const = 0;

    virtual void iterate(const std::function<void(const py::object&, const py::object&)>& callback) const = 0;

    // Clone node from arena to heap (deep copy for Phase 3 arena allocator)
    virtual NodeBase* cloneToHeap() const = 0;
};

// BitmapNode: Main HAMT node using bitmap indexing
class BitmapNode : public NodeBase {
private:
    uint32_t bitmap_;
    std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>> array_;  // shared_ptr<Entry> OR NodeBase*

    // Helper to create a new node with two key-value pairs
    NodeBase* createNode(uint32_t shift,
                        const py::object& key1, const py::object& val1,
                        uint32_t hash2, const py::object& key2, const py::object& val2) const;

public:
    BitmapNode(uint32_t bitmap, const std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>>& array)
        : bitmap_(bitmap), array_(array) {}

    BitmapNode(uint32_t bitmap, std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>>&& array)
        : bitmap_(bitmap), array_(std::move(array)) {}

    ~BitmapNode() override {
        // Clean up all nodes (Entries managed by shared_ptr, Nodes by manual refcount)
        for (const auto& elem : array_) {
            if (std::holds_alternative<NodeBase*>(elem)) {
                std::get<NodeBase*>(elem)->release();
            }
            // shared_ptr<Entry> cleans up automatically
        }
    }

    // Implement virtual methods
    py::object get(uint32_t shift, uint32_t hash,
                  const py::object& key, const py::object& notFound) const override;

    NodeBase* assoc(uint32_t shift, uint32_t hash,
                   const py::object& key, const py::object& val) const override;

    NodeBase* dissoc(uint32_t shift, uint32_t hash,
                    const py::object& key) const override;

    void iterate(const std::function<void(const py::object&, const py::object&)>& callback) const override;

    NodeBase* cloneToHeap() const override;

    uint32_t getBitmap() const { return bitmap_; }
    const std::vector<std::variant<std::shared_ptr<Entry>, NodeBase*>>& getArray() const { return array_; }
};

// CollisionNode: Handles hash collisions when multiple keys have the same hash
class CollisionNode : public NodeBase {
private:
    uint32_t hash_;
    std::shared_ptr<std::vector<Entry*>> entries_;

public:
    CollisionNode(uint32_t hash, const std::vector<Entry*>& entries)
        : hash_(hash), entries_(std::make_shared<std::vector<Entry*>>(entries)) {}

    CollisionNode(uint32_t hash, std::vector<Entry*>&& entries)
        : hash_(hash), entries_(std::make_shared<std::vector<Entry*>>(std::move(entries))) {}

    CollisionNode(uint32_t hash, std::shared_ptr<std::vector<Entry*>> entries)
        : hash_(hash), entries_(entries) {}

    ~CollisionNode() override {
        // Only delete entries if we're the last owner
        if (entries_.use_count() == 1) {
            for (Entry* entry : *entries_) {
                delete entry;
            }
        }
    }

    // Implement virtual methods
    py::object get(uint32_t shift, uint32_t hash,
                  const py::object& key, const py::object& notFound) const override;

    NodeBase* assoc(uint32_t shift, uint32_t hash,
                   const py::object& key, const py::object& val) const override;

    NodeBase* dissoc(uint32_t shift, uint32_t hash,
                    const py::object& key) const override;

    void iterate(const std::function<void(const py::object&, const py::object&)>& callback) const override;

    NodeBase* cloneToHeap() const override;

    uint32_t getHash() const { return hash_; }
    const std::vector<Entry*>& getEntries() const { return *entries_; }
};

// Forward declaration
class PersistentMap;

// Base iterator for tree traversal (O(log n) memory, not O(n))
class MapIterator {
private:
    struct StackFrame {
        const NodeBase* node;
        size_t index;
    };
    std::vector<StackFrame> stack_;
    const NodeBase* current_node_;
    size_t current_index_;
    bool finished_;

    void advance();

public:
    MapIterator(const NodeBase* root);
    bool hasNext() const { return !finished_; }
    std::pair<py::object, py::object> next();
};

// Wrapper iterators for different iteration modes
class KeyIterator {
private:
    MapIterator iter_;
public:
    KeyIterator(const NodeBase* root) : iter_(root) {}
    py::object next() {
        auto pair = iter_.next();
        return pair.first;
    }
};

class ValueIterator {
private:
    MapIterator iter_;
public:
    ValueIterator(const NodeBase* root) : iter_(root) {}
    py::object next() {
        auto pair = iter_.next();
        return pair.second;
    }
};

class ItemIterator {
private:
    MapIterator iter_;
public:
    ItemIterator(const NodeBase* root) : iter_(root) {}
    py::tuple next() {
        auto pair = iter_.next();
        return py::make_tuple(pair.first, pair.second);
    }
};

// PersistentMap: Main container class exposing the public API
class PersistentMap {
private:
    NodeBase* root_;
    size_t count_;

    // Helper structure for bulk construction
    struct HashedEntry {
        uint32_t hash;
        py::object key;
        py::object value;
    };

    // Bottom-up tree construction for bulk operations
    static NodeBase* buildTreeBulk(std::vector<HashedEntry>& entries,
                                   size_t start, size_t end, uint32_t shift,
                                   BulkOpArena& arena);

    // Structural merge helpers (Phase 4)
    static NodeBase* mergeNodes(NodeBase* left, NodeBase* right, uint32_t shift);

public:
    // Sentinel value for "not found"
    static py::object NOT_FOUND;

    // Constructors
    PersistentMap() : root_(nullptr), count_(0) {}

    PersistentMap(NodeBase* root, size_t count) : root_(root), count_(count) {
        if (root_) root_->addRef();
    }

    // Copy constructor
    PersistentMap(const PersistentMap& other) : root_(other.root_), count_(other.count_) {
        if (root_) root_->addRef();
    }

    // Move constructor
    PersistentMap(PersistentMap&& other) noexcept
        : root_(other.root_), count_(other.count_) {
        other.root_ = nullptr;
        other.count_ = 0;
    }

    // Destructor
    ~PersistentMap() {
        if (root_) root_->release();
    }

    // Copy assignment
    PersistentMap& operator=(const PersistentMap& other) {
        if (this != &other) {
            if (other.root_) other.root_->addRef();
            if (root_) root_->release();
            root_ = other.root_;
            count_ = other.count_;
        }
        return *this;
    }

    // Move assignment
    PersistentMap& operator=(PersistentMap&& other) noexcept {
        if (this != &other) {
            if (root_) root_->release();
            root_ = other.root_;
            count_ = other.count_;
            other.root_ = nullptr;
            other.count_ = 0;
        }
        return *this;
    }

    // Core operations (functional style)
    PersistentMap assoc(const py::object& key, const py::object& val) const;
    PersistentMap dissoc(const py::object& key) const;
    py::object get(const py::object& key, const py::object& default_val = py::none()) const;
    bool contains(const py::object& key) const;

    // Python-friendly aliases
    PersistentMap set(const py::object& key, const py::object& val) const { return assoc(key, val); }
    PersistentMap delete_(const py::object& key) const { return dissoc(key); }
    PersistentMap update(const py::object& other) const;
    PersistentMap merge(const py::object& other) const { return update(other); }
    PersistentMap clear() const { return PersistentMap(); }
    PersistentMap copy() const { return *this; }  // Immutable, so copy = self

    // Size
    size_t size() const { return count_; }

    // Iteration (O(log n) memory, not O(n)!)
    KeyIterator keys() const;
    ValueIterator values() const;
    ItemIterator items() const;

    // Fast materialized iteration (returns pre-allocated list)
    // 3-4x faster than items() iterator for full iteration
    py::list itemsList() const;
    py::list keysList() const;
    py::list valuesList() const;

    // Equality
    bool operator==(const PersistentMap& other) const;
    bool operator!=(const PersistentMap& other) const { return !(*this == other); }

    // String representation
    std::string repr() const;

    // Factory methods
    static PersistentMap fromDict(const py::dict& d);
    static PersistentMap create(const py::kwargs& kw);
};
