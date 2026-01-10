#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include "persistent_map.hpp"

namespace py = pybind11;

// Forward declaration
class SetIterator;

/**
 * PersistentSet - Immutable set implementation
 *
 * A persistent (immutable) hash set implemented as a wrapper around
 * PersistentDict, where keys are set elements and all values are py::none().
 *
 * Inherits all performance characteristics from PersistentDict:
 * - O(log32 n) operations (conj, disj, contains)
 * - Structural sharing for memory efficiency
 * - Copy-on-write semantics
 *
 * Supports standard set operations:
 * - union, intersection, difference, symmetric_difference
 * - Pythonic operators: |, &, -, ^
 */
class PersistentSet {
private:
    PersistentDict map_;  // Keys are set elements, values are always py::none()

public:
    // Constructors
    PersistentSet();
    PersistentSet(const PersistentDict& map);

    // Copy constructor
    PersistentSet(const PersistentSet& other) = default;

    // Move constructor
    PersistentSet(PersistentSet&& other) noexcept = default;

    // Destructor
    ~PersistentSet() = default;

    // Copy assignment
    PersistentSet& operator=(const PersistentSet& other) = default;

    // Move assignment
    PersistentSet& operator=(PersistentSet&& other) noexcept = default;

    // Core operations (functional style)
    PersistentSet conj(const py::object& elem) const;  // Add element
    PersistentSet disj(const py::object& elem) const;  // Remove element
    bool contains(const py::object& elem) const;

    // Set operations
    PersistentSet union_(const PersistentSet& other) const;
    PersistentSet intersection(const PersistentSet& other) const;
    PersistentSet difference(const PersistentSet& other) const;
    PersistentSet symmetric_difference(const PersistentSet& other) const;

    // Set predicates
    bool issubset(const PersistentSet& other) const;
    bool issuperset(const PersistentSet& other) const;
    bool isdisjoint(const PersistentSet& other) const;

    // Python-friendly aliases
    PersistentSet add(const py::object& elem) const { return conj(elem); }
    PersistentSet remove(const py::object& elem) const { return disj(elem); }
    PersistentSet update(const py::object& other) const;
    PersistentSet clear() const { return PersistentSet(); }
    PersistentSet copy() const { return *this; }  // Immutable, so copy = self

    // Size
    size_t size() const { return map_.size(); }

    // Iteration
    SetIterator iter() const;

    // Fast materialized iteration
    py::list list() const;

    // Equality
    bool operator==(const PersistentSet& other) const;
    bool operator!=(const PersistentSet& other) const { return !(*this == other); }

    // String representation
    std::string repr() const;

    // Factory methods
    static PersistentSet fromSet(const py::set& s);
    static PersistentSet fromList(const py::list& l);
    static PersistentSet fromIterable(const py::object& iterable);
    static PersistentSet create(const py::args& args);

    // Access to underlying map (for implementation)
    const PersistentDict& getMap() const { return map_; }
};

// Iterator for set elements (just wraps map key iterator)
class SetIterator {
private:
    KeyIterator iter_;

public:
    SetIterator(const PersistentDict& map) : iter_(map.keys()) {}

    py::object next() {
        return iter_.next();
    }
};
