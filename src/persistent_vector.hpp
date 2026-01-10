#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <atomic>
#include <vector>
#include <variant>
#include <memory>
#include <string>

namespace py = pybind11;

// Forward declarations
class VectorNode;
class VectorIterator;

/**
 * PersistentList - Indexed sequence with O(log₃₂ n) access
 *
 * Implements a persistent (immutable) list using a 32-way tree structure
 * with tail optimization, similar to Clojure's PersistentVector.
 *
 * Key features:
 * - O(log₃₂ n) random access and update (effectively O(1) for practical sizes)
 * - O(1) amortized append (tail optimization)
 * - Structural sharing via copy-on-write
 * - Intrusive reference counting for nodes
 *
 * Tree structure:
 * - Each node has up to 32 children (5 bits per level)
 * - Last 0-32 elements stored in separate tail for fast append
 * - Path copying for updates (only O(log n) nodes copied)
 */
class PersistentList {
private:
    VectorNode* root_;                                          // Tree root
    std::shared_ptr<std::vector<py::object>> tail_;            // Last 0-32 elements
    size_t count_;                                             // Total elements
    uint32_t shift_;                                           // Tree depth (5 * levels)

    static constexpr uint32_t BITS = 5;                        // 2^5 = 32-way branching
    static constexpr uint32_t NODE_SIZE = 1 << BITS;           // 32
    static constexpr uint32_t MASK = NODE_SIZE - 1;            // 0x1F

    // Helper: get element from tree (not from tail)
    py::object getFromTree(size_t idx) const;

    // Helper: update element in tree, returning new tree
    VectorNode* assocInTree(VectorNode* node, uint32_t level, size_t idx, const py::object& val) const;

    // Helper: push tail to tree when it's full
    VectorNode* pushTail(VectorNode* node, uint32_t level, VectorNode* tailNode) const;

    // Helper: create new path for expanding tree
    VectorNode* newPath(uint32_t level, VectorNode* node) const;

    // Helper: calculate tail offset (index where tail starts)
    size_t tailOffset() const {
        if (count_ < NODE_SIZE) return 0;
        return ((count_ - 1) >> BITS) << BITS;
    }

public:
    // Constructors
    PersistentList();
    PersistentList(VectorNode* root, std::shared_ptr<std::vector<py::object>> tail,
                   size_t count, uint32_t shift);

    // Copy constructor
    PersistentList(const PersistentList& other);

    // Move constructor
    PersistentList(PersistentList&& other) noexcept;

    // Destructor
    ~PersistentList();

    // Copy assignment
    PersistentList& operator=(const PersistentList& other);

    // Move assignment
    PersistentList& operator=(PersistentList&& other) noexcept;

    // Core operations (functional style)
    PersistentList conj(const py::object& val) const;                    // Append
    PersistentList assoc(size_t idx, const py::object& val) const;       // Update at index
    py::object nth(size_t idx) const;                                    // Get at index
    py::object get(size_t idx, const py::object& default_val) const;     // Get with default
    PersistentList pop() const;                                          // Remove last

    // Python-friendly aliases
    PersistentList append(const py::object& val) const { return conj(val); }
    PersistentList set(size_t idx, const py::object& val) const { return assoc(idx, val); }

    // Size
    size_t size() const { return count_; }

    // Iteration
    VectorIterator iter() const;

    // Fast materialized list
    py::list list() const;

    // Slicing
    PersistentList slice(Py_ssize_t start, Py_ssize_t stop) const;

    // Equality
    bool operator==(const PersistentList& other) const;
    bool operator!=(const PersistentList& other) const { return !(*this == other); }

    // String representation
    std::string repr() const;

    // Factory methods
    static PersistentList fromList(const py::list& l);
    static PersistentList fromIterable(const py::object& iterable);
    static PersistentList create(const py::args& args);
};

/**
 * VectorNode - Internal tree node for PersistentList
 *
 * Each node can hold up to 32 children, which can be either:
 * - py::object (leaf values)
 * - VectorNode* (internal nodes)
 *
 * Uses intrusive reference counting for memory management.
 */
class VectorNode {
private:
    mutable std::atomic<uint32_t> refcount_;
    std::vector<std::variant<py::object, VectorNode*>> array_;

public:
    // Constructors
    VectorNode();
    explicit VectorNode(size_t size);

    // No copy/move (managed by refcounting)
    VectorNode(const VectorNode&) = delete;
    VectorNode& operator=(const VectorNode&) = delete;

    ~VectorNode();

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

    // Array access
    size_t arraySize() const { return array_.size(); }

    const std::variant<py::object, VectorNode*>& get(size_t idx) const {
        return array_[idx];
    }

    void set(size_t idx, const py::object& val) {
        array_[idx] = val;
    }

    void set(size_t idx, VectorNode* node) {
        array_[idx] = node;
    }

    void push(const py::object& val) {
        array_.push_back(val);
    }

    void push(VectorNode* node) {
        array_.push_back(node);
    }

    // Clone for copy-on-write
    VectorNode* clone() const;
};

/**
 * VectorIterator - Iterator for PersistentList
 */
class VectorIterator {
private:
    const PersistentList* vec_;
    size_t index_;

public:
    VectorIterator(const PersistentList* vec) : vec_(vec), index_(0) {}

    bool hasNext() const { return index_ < vec_->size(); }

    py::object next() {
        if (!hasNext()) {
            throw py::stop_iteration();
        }
        return vec_->nth(index_++);
    }
};
