#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>
#include <memory>
#include <string>
#include "persistent_dict.hpp"  // For Entry struct and pmutils

namespace py = pybind11;

// Forward declarations
class ArrayMapIterator;
class ArrayMapKeyIterator;
class ArrayMapValueIterator;
class ArrayMapItemIterator;

/**
 * PersistentArrayMap - Small map optimization for ≤8 entries
 *
 * Uses a simple vector of key-value pairs with linear scan.
 * For small maps (≤8 entries), this is 5-60x faster than HashMap
 * due to better cache locality and avoiding tree traversal.
 *
 * Performance characteristics:
 * - Get: O(n) where n ≤ 8 (very fast in practice)
 * - Assoc: O(n) copy + insert
 * - Dissoc: O(n) copy + remove
 * - Memory: ~200-400 bytes for 8 entries vs ~1KB+ for HashMap
 *
 * Copy-on-write semantics via shared_ptr ensure immutability
 * and structural sharing.
 */
class PersistentArrayMap {
private:
    std::shared_ptr<std::vector<Entry>> entries_;

    static constexpr size_t MAX_SIZE = 8;  // Maximum entries before conversion recommended

    // Helper: find index of key (linear scan)
    int findIndex(const py::object& key) const;

public:
    // Sentinel value for "not found"
    static py::object NOT_FOUND;

    // Constructors
    PersistentArrayMap();
    PersistentArrayMap(std::shared_ptr<std::vector<Entry>> entries);

    // Copy constructor
    PersistentArrayMap(const PersistentArrayMap& other) = default;

    // Move constructor
    PersistentArrayMap(PersistentArrayMap&& other) noexcept = default;

    // Destructor
    ~PersistentArrayMap() = default;

    // Copy assignment
    PersistentArrayMap& operator=(const PersistentArrayMap& other) = default;

    // Move assignment
    PersistentArrayMap& operator=(PersistentArrayMap&& other) noexcept = default;

    // Core operations (functional style)
    PersistentArrayMap assoc(const py::object& key, const py::object& val) const;
    PersistentArrayMap dissoc(const py::object& key) const;
    py::object get(const py::object& key, const py::object& default_val = py::none()) const;
    bool contains(const py::object& key) const;

    // Python-friendly aliases
    PersistentArrayMap set(const py::object& key, const py::object& val) const { return assoc(key, val); }
    PersistentArrayMap delete_(const py::object& key) const { return dissoc(key); }
    PersistentArrayMap update(const py::object& other) const;
    PersistentArrayMap merge(const py::object& other) const { return update(other); }
    PersistentArrayMap clear() const { return PersistentArrayMap(); }
    PersistentArrayMap copy() const { return *this; }  // Immutable, so copy = self

    // Size
    size_t size() const { return entries_ ? entries_->size() : 0; }

    // Iteration
    ArrayMapKeyIterator keys() const;
    ArrayMapValueIterator values() const;
    ArrayMapItemIterator items() const;

    // Fast materialized iteration (returns pre-allocated list)
    py::list itemsList() const;
    py::list keysList() const;
    py::list valuesList() const;

    // Equality
    bool operator==(const PersistentArrayMap& other) const;
    bool operator!=(const PersistentArrayMap& other) const { return !(*this == other); }

    // String representation
    std::string repr() const;

    // Factory methods
    static PersistentArrayMap fromDict(const py::dict& d);
    static PersistentArrayMap create(const py::kwargs& kw);

    // Access to entries (for iterators)
    const std::vector<Entry>* getEntries() const { return entries_.get(); }
};

// Simple iterator for ArrayMap (just iterate over vector)
class ArrayMapIterator {
private:
    const std::vector<Entry>* entries_;
    size_t index_;

public:
    ArrayMapIterator(const std::vector<Entry>* entries);

    bool hasNext() const { return entries_ && index_ < entries_->size(); }
    std::pair<py::object, py::object> next();
};

// Wrapper iterators for different iteration modes
class ArrayMapKeyIterator {
private:
    ArrayMapIterator iter_;

public:
    ArrayMapKeyIterator(const std::vector<Entry>* entries) : iter_(entries) {}

    py::object next() {
        auto pair = iter_.next();
        return pair.first;
    }

    bool hasNext() const { return iter_.hasNext(); }
};

class ArrayMapValueIterator {
private:
    ArrayMapIterator iter_;

public:
    ArrayMapValueIterator(const std::vector<Entry>* entries) : iter_(entries) {}

    py::object next() {
        auto pair = iter_.next();
        return pair.second;
    }

    bool hasNext() const { return iter_.hasNext(); }
};

class ArrayMapItemIterator {
private:
    ArrayMapIterator iter_;

public:
    ArrayMapItemIterator(const std::vector<Entry>* entries) : iter_(entries) {}

    py::tuple next() {
        auto pair = iter_.next();
        return py::make_tuple(pair.first, pair.second);
    }

    bool hasNext() const { return iter_.hasNext(); }
};
