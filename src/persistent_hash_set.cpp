#include "persistent_hash_set.hpp"
#include <sstream>

// Constructors
PersistentHashSet::PersistentHashSet() : map_() {}

PersistentHashSet::PersistentHashSet(const PersistentMap& map) : map_(map) {}

// Core operations
PersistentHashSet PersistentHashSet::conj(const py::object& elem) const {
    // Add element by associating it with py::none()
    PersistentMap newMap = map_.assoc(elem, py::none());
    return PersistentHashSet(newMap);
}

PersistentHashSet PersistentHashSet::disj(const py::object& elem) const {
    // Remove element by dissociating the key
    PersistentMap newMap = map_.dissoc(elem);
    return PersistentHashSet(newMap);
}

bool PersistentHashSet::contains(const py::object& elem) const {
    return map_.contains(elem);
}

// Set operations
PersistentHashSet PersistentHashSet::union_(const PersistentHashSet& other) const {
    // Start with this set, add all elements from other
    PersistentHashSet result = *this;
    py::list items = other.map_.keysList();
    for (auto elem : items) {
        result = result.conj(elem.cast<py::object>());
    }
    return result;
}

PersistentHashSet PersistentHashSet::intersection(const PersistentHashSet& other) const {
    // Iterate smaller set, check containment in larger
    const PersistentHashSet& smaller = (size() <= other.size()) ? *this : other;
    const PersistentHashSet& larger = (size() <= other.size()) ? other : *this;

    PersistentHashSet result;
    py::list items = smaller.map_.keysList();
    for (auto elem : items) {
        if (larger.contains(elem.cast<py::object>())) {
            result = result.conj(elem.cast<py::object>());
        }
    }
    return result;
}

PersistentHashSet PersistentHashSet::difference(const PersistentHashSet& other) const {
    // Start with this set, remove all elements in other
    PersistentHashSet result = *this;
    py::list items = other.map_.keysList();
    for (auto elem : items) {
        result = result.disj(elem.cast<py::object>());
    }
    return result;
}

PersistentHashSet PersistentHashSet::symmetric_difference(const PersistentHashSet& other) const {
    // (A - B) ∪ (B - A)
    PersistentHashSet aMinusB = this->difference(other);
    PersistentHashSet bMinusA = other.difference(*this);
    return aMinusB.union_(bMinusA);
}

// Set predicates
bool PersistentHashSet::issubset(const PersistentHashSet& other) const {
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

bool PersistentHashSet::issuperset(const PersistentHashSet& other) const {
    // Other must be subset of this
    return other.issubset(*this);
}

bool PersistentHashSet::isdisjoint(const PersistentHashSet& other) const {
    // No elements in common
    const PersistentHashSet& smaller = (size() <= other.size()) ? *this : other;
    const PersistentHashSet& larger = (size() <= other.size()) ? other : *this;

    py::list items = smaller.map_.keysList();
    for (auto elem : items) {
        if (larger.contains(elem.cast<py::object>())) {
            return false;
        }
    }
    return true;
}

// Update method
PersistentHashSet PersistentHashSet::update(const py::object& other) const {
    PersistentHashSet result = *this;

    // Handle set
    if (py::isinstance<py::set>(other)) {
        py::set s = other.cast<py::set>();
        for (auto elem : s) {
            result = result.conj(py::reinterpret_borrow<py::object>(elem));
        }
        return result;
    }

    // Handle PersistentHashSet
    if (py::isinstance<PersistentHashSet>(other)) {
        return union_(other.cast<const PersistentHashSet&>());
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
        throw std::invalid_argument("update() requires a set, PersistentHashSet, list, or iterable");
    }
}

// Iteration
SetIterator PersistentHashSet::iter() const {
    return SetIterator(map_);
}

// Fast materialized iteration
py::list PersistentHashSet::list() const {
    return map_.keysList();
}

// Equality
bool PersistentHashSet::operator==(const PersistentHashSet& other) const {
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
std::string PersistentHashSet::repr() const {
    std::ostringstream oss;
    oss << "PersistentHashSet({";

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
PersistentHashSet PersistentHashSet::fromSet(const py::set& s) {
    PersistentHashSet result;
    for (auto elem : s) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}

PersistentHashSet PersistentHashSet::fromList(const py::list& l) {
    PersistentHashSet result;
    for (auto elem : l) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}

PersistentHashSet PersistentHashSet::fromIterable(const py::object& iterable) {
    PersistentHashSet result;
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

PersistentHashSet PersistentHashSet::create(const py::args& args) {
    PersistentHashSet result;
    for (auto elem : args) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}
