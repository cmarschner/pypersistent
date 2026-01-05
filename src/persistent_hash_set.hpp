#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include "persistent_map.hpp"

namespace py = pybind11;

// Forward declaration
class SetIterator;

/**
 * PersistentHashSet - Immutable set implementation
 *
 * A persistent (immutable) hash set implemented as a wrapper around
 * PersistentMap, where keys are set elements and all values are py::none().
 *
 * Inherits all performance characteristics from PersistentMap:
 * - O(log32 n) operations (conj, disj, contains)
 * - Structural sharing for memory efficiency
 * - Copy-on-write semantics
 *
 * Supports standard set operations:
 * - union, intersection, difference, symmetric_difference
 * - Pythonic operators: |, &, -, ^
 */
class PersistentHashSet {
private:
    PersistentMap map_;  // Keys are set elements, values are always py::none()

public:
    // Constructors
    PersistentHashSet();
    PersistentHashSet(const PersistentMap& map);

    // Copy constructor
    PersistentHashSet(const PersistentHashSet& other) = default;

    // Move constructor
    PersistentHashSet(PersistentHashSet&& other) noexcept = default;

    // Destructor
    ~PersistentHashSet() = default;

    // Copy assignment
    PersistentHashSet& operator=(const PersistentHashSet& other) = default;

    // Move assignment
    PersistentHashSet& operator=(PersistentHashSet&& other) noexcept = default;

    // Core operations (functional style)
    PersistentHashSet conj(const py::object& elem) const;  // Add element
    PersistentHashSet disj(const py::object& elem) const;  // Remove element
    bool contains(const py::object& elem) const;

    // Set operations
    PersistentHashSet union_(const PersistentHashSet& other) const;
    PersistentHashSet intersection(const PersistentHashSet& other) const;
    PersistentHashSet difference(const PersistentHashSet& other) const;
    PersistentHashSet symmetric_difference(const PersistentHashSet& other) const;

    // Set predicates
    bool issubset(const PersistentHashSet& other) const;
    bool issuperset(const PersistentHashSet& other) const;
    bool isdisjoint(const PersistentHashSet& other) const;

    // Python-friendly aliases
    PersistentHashSet add(const py::object& elem) const { return conj(elem); }
    PersistentHashSet remove(const py::object& elem) const { return disj(elem); }
    PersistentHashSet update(const py::object& other) const;
    PersistentHashSet clear() const { return PersistentHashSet(); }
    PersistentHashSet copy() const { return *this; }  // Immutable, so copy = self

    // Size
    size_t size() const { return map_.size(); }

    // Iteration
    SetIterator iter() const;

    // Fast materialized iteration
    py::list list() const;

    // Equality
    bool operator==(const PersistentHashSet& other) const;
    bool operator!=(const PersistentHashSet& other) const { return !(*this == other); }

    // String representation
    std::string repr() const;

    // Factory methods
    static PersistentHashSet fromSet(const py::set& s);
    static PersistentHashSet fromList(const py::list& l);
    static PersistentHashSet fromIterable(const py::object& iterable);
    static PersistentHashSet create(const py::args& args);

    // Access to underlying map (for implementation)
    const PersistentMap& getMap() const { return map_; }
};

// Iterator for set elements (just wraps map key iterator)
class SetIterator {
private:
    KeyIterator iter_;

public:
    SetIterator(const PersistentMap& map) : iter_(map.keys()) {}

    py::object next() {
        return iter_.next();
    }
};
