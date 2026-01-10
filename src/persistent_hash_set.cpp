#include "persistent_hash_set.hpp"
#include <sstream>

// Constructors
PersistentSet::PersistentSet() : map_() {}

PersistentSet::PersistentSet(const PersistentDict& map) : map_(map) {}

// Core operations
PersistentSet PersistentSet::conj(const py::object& elem) const {
    // Add element by associating it with py::none()
    PersistentDict newMap = map_.assoc(elem, py::none());
    return PersistentSet(newMap);
}

PersistentSet PersistentSet::disj(const py::object& elem) const {
    // Remove element by dissociating the key
    PersistentDict newMap = map_.dissoc(elem);
    return PersistentSet(newMap);
}

bool PersistentSet::contains(const py::object& elem) const {
    return map_.contains(elem);
}

// Set operations
PersistentSet PersistentSet::union_(const PersistentSet& other) const {
    // Start with this set, add all elements from other
    PersistentSet result = *this;
    py::list items = other.map_.keysList();
    for (auto elem : items) {
        result = result.conj(elem.cast<py::object>());
    }
    return result;
}

PersistentSet PersistentSet::intersection(const PersistentSet& other) const {
    // Iterate smaller set, check containment in larger
    const PersistentSet& smaller = (size() <= other.size()) ? *this : other;
    const PersistentSet& larger = (size() <= other.size()) ? other : *this;

    PersistentSet result;
    py::list items = smaller.map_.keysList();
    for (auto elem : items) {
        if (larger.contains(elem.cast<py::object>())) {
            result = result.conj(elem.cast<py::object>());
        }
    }
    return result;
}

PersistentSet PersistentSet::difference(const PersistentSet& other) const {
    // Start with this set, remove all elements in other
    PersistentSet result = *this;
    py::list items = other.map_.keysList();
    for (auto elem : items) {
        result = result.disj(elem.cast<py::object>());
    }
    return result;
}

PersistentSet PersistentSet::symmetric_difference(const PersistentSet& other) const {
    // (A - B) ∪ (B - A)
    PersistentSet aMinusB = this->difference(other);
    PersistentSet bMinusA = other.difference(*this);
    return aMinusB.union_(bMinusA);
}

// Set predicates
bool PersistentSet::issubset(const PersistentSet& other) const {
    // All elements of this must be in other
    if (size() > other.size()) return false;

    py::list items = map_.keysList();
    for (auto elem : items) {
        if (!other.contains(elem.cast<py::object>())) {
            return false;
        }
    }
    return true;
}

bool PersistentSet::issuperset(const PersistentSet& other) const {
    // Other must be subset of this
    return other.issubset(*this);
}

bool PersistentSet::isdisjoint(const PersistentSet& other) const {
    // No elements in common
    const PersistentSet& smaller = (size() <= other.size()) ? *this : other;
    const PersistentSet& larger = (size() <= other.size()) ? other : *this;

    py::list items = smaller.map_.keysList();
    for (auto elem : items) {
        if (larger.contains(elem.cast<py::object>())) {
            return false;
        }
    }
    return true;
}

// Update method
PersistentSet PersistentSet::update(const py::object& other) const {
    PersistentSet result = *this;

    // Handle set
    if (py::isinstance<py::set>(other)) {
        py::set s = other.cast<py::set>();
        for (auto elem : s) {
            result = result.conj(py::reinterpret_borrow<py::object>(elem));
        }
        return result;
    }

    // Handle PersistentSet
    if (py::isinstance<PersistentSet>(other)) {
        return union_(other.cast<const PersistentSet&>());
    }

    // Handle list or other iterable
    if (py::isinstance<py::list>(other)) {
        py::list l = other.cast<py::list>();
        for (auto elem : l) {
            result = result.conj(py::reinterpret_borrow<py::object>(elem));
        }
        return result;
    }

    // Handle generic iterable
    try {
        py::iterator it = py::iter(other);
        while (it != py::iterator::sentinel()) {
            result = result.conj(py::reinterpret_borrow<py::object>(*it));
            ++it;
        }
        return result;
    } catch (const py::error_already_set&) {
        throw std::invalid_argument("update() requires a set, PersistentSet, list, or iterable");
    }
}

// Iteration
SetIterator PersistentSet::iter() const {
    return SetIterator(map_);
}

// Fast materialized iteration
py::list PersistentSet::list() const {
    return map_.keysList();
}

// Equality
bool PersistentSet::operator==(const PersistentSet& other) const {
    // Fast path: same object
    if (this == &other) return true;

    // Different sizes
    if (size() != other.size()) return false;

    // Check all elements
    py::list items = map_.keysList();
    for (auto elem : items) {
        if (!other.contains(elem.cast<py::object>())) {
            return false;
        }
    }
    return true;
}

// String representation
std::string PersistentSet::repr() const {
    std::ostringstream oss;
    oss << "PersistentSet({";

    if (size() > 0) {
        py::list items = map_.keysList();
        bool first = true;
        for (auto elem : items) {
            if (!first) oss << ", ";
            first = false;

            py::object elem_repr = py::repr(elem);
            oss << elem_repr.cast<std::string>();
        }
    }

    oss << "})";
    return oss.str();
}

// Factory methods
PersistentSet PersistentSet::fromSet(const py::set& s) {
    PersistentSet result;
    for (auto elem : s) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}

PersistentSet PersistentSet::fromList(const py::list& l) {
    PersistentSet result;
    for (auto elem : l) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}

PersistentSet PersistentSet::fromIterable(const py::object& iterable) {
    PersistentSet result;
    try {
        py::iterator it = py::iter(iterable);
        while (it != py::iterator::sentinel()) {
            result = result.conj(py::reinterpret_borrow<py::object>(*it));
            ++it;
        }
        return result;
    } catch (const py::error_already_set&) {
        throw std::invalid_argument("fromIterable() requires an iterable object");
    }
}

PersistentSet PersistentSet::create(const py::args& args) {
    PersistentSet result;
    for (auto elem : args) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}
