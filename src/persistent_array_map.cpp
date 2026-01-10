#include "persistent_array_map.hpp"
#include <sstream>
#include <stdexcept>

// Initialize static member
py::object PersistentArrayMap::NOT_FOUND;

// Constructors
PersistentArrayMap::PersistentArrayMap()
    : entries_(std::make_shared<std::vector<Entry>>()) {}

PersistentArrayMap::PersistentArrayMap(std::shared_ptr<std::vector<Entry>> entries)
    : entries_(entries) {}

// Helper: find index of key (linear scan)
int PersistentArrayMap::findIndex(const py::object& key) const {
    if (!entries_) return -1;

    for (size_t i = 0; i < entries_->size(); ++i) {
        if (pmutils::keysEqual((*entries_)[i].key, key)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Core operations
PersistentArrayMap PersistentArrayMap::assoc(const py::object& key, const py::object& val) const {
    int idx = findIndex(key);

    if (idx >= 0) {
        // Key exists - check if value is the same
        if ((*entries_)[idx].value.is(val)) {
            return *this;  // No change needed
        }

        // Copy-on-write: copy vector, update one entry
        auto newEntries = std::make_shared<std::vector<Entry>>(*entries_);
        (*newEntries)[idx].value = val;
        return PersistentArrayMap(newEntries);
    } else {
        // Key doesn't exist
        if (size() >= MAX_SIZE) {
            throw std::runtime_error("PersistentArrayMap max size exceeded (8 entries). Consider using PersistentDict for larger maps.");
        }

        // Copy vector and append
        auto newEntries = std::make_shared<std::vector<Entry>>(*entries_);
        newEntries->push_back(Entry(key, val));
        return PersistentArrayMap(newEntries);
    }
}

PersistentArrayMap PersistentArrayMap::dissoc(const py::object& key) const {
    int idx = findIndex(key);
    if (idx < 0) return *this;  // Key not found, no change

    // Copy vector without the entry at idx
    auto newEntries = std::make_shared<std::vector<Entry>>();
    newEntries->reserve(entries_->size() - 1);

    for (size_t i = 0; i < entries_->size(); ++i) {
        if (i != static_cast<size_t>(idx)) {
            newEntries->push_back((*entries_)[i]);
        }
    }

    return PersistentArrayMap(newEntries);
}

py::object PersistentArrayMap::get(const py::object& key, const py::object& default_val) const {
    int idx = findIndex(key);
    if (idx >= 0) {
        return (*entries_)[idx].value;
    }
    return default_val;
}

bool PersistentArrayMap::contains(const py::object& key) const {
    return findIndex(key) >= 0;
}

// Update method
PersistentArrayMap PersistentArrayMap::update(const py::object& other) const {
    PersistentArrayMap result = *this;

    // Handle dict
    if (py::isinstance<py::dict>(other)) {
        py::dict d = other.cast<py::dict>();
        for (auto item : d) {
            result = result.assoc(py::reinterpret_borrow<py::object>(item.first),
                                  py::reinterpret_borrow<py::object>(item.second));
        }
        return result;
    }

    // Handle PersistentArrayMap
    if (py::isinstance<PersistentArrayMap>(other)) {
        const PersistentArrayMap& otherMap = other.cast<const PersistentArrayMap&>();
        if (otherMap.entries_) {
            for (const auto& entry : *otherMap.entries_) {
                result = result.assoc(entry.key, entry.value);
            }
        }
        return result;
    }

    // Handle PersistentDict
    if (py::isinstance<PersistentDict>(other)) {
        // Use items_list() for efficiency
        const PersistentDict& otherMap = other.cast<const PersistentDict&>();
        py::list items = otherMap.itemsList();
        for (auto item : items) {
            py::tuple t = item.cast<py::tuple>();
            result = result.assoc(t[0], t[1]);
        }
        return result;
    }

    // Handle generic mapping (items() method)
    if (py::hasattr(other, "items")) {
        py::object items = other.attr("items")();
        for (auto item : items) {
            py::tuple t = item.cast<py::tuple>();
            result = result.assoc(t[0], t[1]);
        }
        return result;
    }

    throw std::invalid_argument("update() requires a dict, PersistentArrayMap, PersistentDict, or mapping");
}

// Iteration
ArrayMapKeyIterator PersistentArrayMap::keys() const {
    return ArrayMapKeyIterator(entries_.get());
}

ArrayMapValueIterator PersistentArrayMap::values() const {
    return ArrayMapValueIterator(entries_.get());
}

ArrayMapItemIterator PersistentArrayMap::items() const {
    return ArrayMapItemIterator(entries_.get());
}

// Fast materialized iteration
py::list PersistentArrayMap::itemsList() const {
    py::list result;
    if (entries_) {
        for (const auto& entry : *entries_) {
            result.append(py::make_tuple(entry.key, entry.value));
        }
    }
    return result;
}

py::list PersistentArrayMap::keysList() const {
    py::list result;
    if (entries_) {
        for (const auto& entry : *entries_) {
            result.append(entry.key);
        }
    }
    return result;
}

py::list PersistentArrayMap::valuesList() const {
    py::list result;
    if (entries_) {
        for (const auto& entry : *entries_) {
            result.append(entry.value);
        }
    }
    return result;
}

// Equality
bool PersistentArrayMap::operator==(const PersistentArrayMap& other) const {
    // Fast path: same object
    if (this == &other) return true;

    // Different sizes
    if (size() != other.size()) return false;

    // Empty maps
    if (size() == 0) return true;

    // Compare all entries
    for (const auto& entry : *entries_) {
        int idx = other.findIndex(entry.key);
        if (idx < 0) return false;  // Key not in other

        // Check value equality
        const py::object& otherVal = (*other.entries_)[idx].value;
        int eq = PyObject_RichCompareBool(entry.value.ptr(), otherVal.ptr(), Py_EQ);
        if (eq != 1) return false;
    }

    return true;
}

// String representation
std::string PersistentArrayMap::repr() const {
    std::ostringstream oss;
    oss << "PersistentArrayMap({";

    if (entries_ && !entries_->empty()) {
        bool first = true;
        for (const auto& entry : *entries_) {
            if (!first) oss << ", ";
            first = false;

            // Use Python's repr for key and value
            py::object key_repr = py::repr(entry.key);
            py::object val_repr = py::repr(entry.value);
            oss << key_repr.cast<std::string>() << ": " << val_repr.cast<std::string>();
        }
    }

    oss << "})";
    return oss.str();
}

// Factory methods
PersistentArrayMap PersistentArrayMap::fromDict(const py::dict& d) {
    if (d.size() > MAX_SIZE) {
        throw std::runtime_error("Dictionary too large for PersistentArrayMap (max 8 entries). Use PersistentDict instead.");
    }

    auto entries = std::make_shared<std::vector<Entry>>();
    entries->reserve(d.size());

    for (auto item : d) {
        entries->push_back(Entry(py::reinterpret_borrow<py::object>(item.first),
                                 py::reinterpret_borrow<py::object>(item.second)));
    }

    return PersistentArrayMap(entries);
}

PersistentArrayMap PersistentArrayMap::create(const py::kwargs& kw) {
    if (kw.size() > MAX_SIZE) {
        throw std::runtime_error("Too many keyword arguments for PersistentArrayMap (max 8). Use PersistentDict instead.");
    }

    auto entries = std::make_shared<std::vector<Entry>>();
    entries->reserve(kw.size());

    for (auto item : kw) {
        entries->push_back(Entry(py::reinterpret_borrow<py::object>(item.first),
                                 py::reinterpret_borrow<py::object>(item.second)));
    }

    return PersistentArrayMap(entries);
}

// ArrayMapIterator implementation
ArrayMapIterator::ArrayMapIterator(const std::vector<Entry>* entries)
    : entries_(entries), index_(0) {}

std::pair<py::object, py::object> ArrayMapIterator::next() {
    if (!hasNext()) {
        throw py::stop_iteration();
    }

    const Entry& entry = (*entries_)[index_++];
    return std::make_pair(entry.key, entry.value);
}
