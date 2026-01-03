#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include "persistent_map.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pypersistent, m) {
    m.doc() = "High-performance persistent hash map (HAMT) implementation in C++";

    // Initialize the NOT_FOUND sentinel
    PersistentMap::NOT_FOUND = py::object();

    // Expose iterators as Python iterators
    py::class_<KeyIterator>(m, "KeyIterator")
        .def("__iter__", [](KeyIterator &it) -> KeyIterator& { return it; })
        .def("__next__", &KeyIterator::next);

    py::class_<ValueIterator>(m, "ValueIterator")
        .def("__iter__", [](ValueIterator &it) -> ValueIterator& { return it; })
        .def("__next__", &ValueIterator::next);

    py::class_<ItemIterator>(m, "ItemIterator")
        .def("__iter__", [](ItemIterator &it) -> ItemIterator& { return it; })
        .def("__next__", &ItemIterator::next);

    py::class_<PersistentMap>(m, "PersistentMap")
        .def(py::init<>(),
             "Create an empty PersistentMap")

        // Core methods
        .def("assoc", &PersistentMap::assoc,
             py::arg("key"), py::arg("val"),
             "Associate key with value, returning new map.\n\n"
             "Args:\n"
             "    key: The key (must be hashable)\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentMap with the association added")

        .def("dissoc", &PersistentMap::dissoc,
             py::arg("key"),
             "Remove key, returning new map.\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentMap with the key removed")

        .def("get", &PersistentMap::get,
             py::arg("key"), py::arg("default") = py::none(),
             "Get value for key, or default if not found.\n\n"
             "Args:\n"
             "    key: The key to look up\n"
             "    default: Value to return if key not found (default: None)\n\n"
             "Returns:\n"
             "    The value associated with key, or default")

        // Python-friendly aliases
        .def("set", &PersistentMap::set,
             py::arg("key"), py::arg("val"),
             "Pythonic alias for assoc(). Set key to value.\n\n"
             "Args:\n"
             "    key: The key\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentMap with the key set")

        .def("delete", &PersistentMap::delete_,
             py::arg("key"),
             "Pythonic alias for dissoc(). Delete key.\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentMap without the key")

        .def("update", &PersistentMap::update,
             py::arg("other"),
             "Merge another mapping, returning new map.\n\n"
             "Args:\n"
             "    other: A dict, PersistentMap, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentMap with merged entries")

        .def("merge", &PersistentMap::merge,
             py::arg("other"),
             "Alias for update(). Merge mappings.\n\n"
             "Args:\n"
             "    other: A dict, PersistentMap, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentMap with merged entries")

        .def("clear", &PersistentMap::clear,
             "Return an empty PersistentMap.\n\n"
             "Returns:\n"
             "    An empty PersistentMap")

        .def("copy", &PersistentMap::copy,
             "Return self (no-op for immutable).\n\n"
             "Returns:\n"
             "    Self")

        // Python protocols
        .def("__getitem__",
             [](const PersistentMap& m, py::object key) -> py::object {
                 py::object result = m.get(key, PersistentMap::NOT_FOUND);
                 if (result.is(PersistentMap::NOT_FOUND)) {
                     throw py::key_error(py::str(key));
                 }
                 return result;
             },
             py::arg("key"),
             "Get item using bracket notation. Raises KeyError if not found.")

        .def("__contains__", &PersistentMap::contains,
             py::arg("key"),
             "Check if key is in map.\n\n"
             "Args:\n"
             "    key: The key to check\n\n"
             "Returns:\n"
             "    True if key is present, False otherwise")

        .def("__len__", &PersistentMap::size,
             "Return number of entries in the map.")

        .def("__iter__", &PersistentMap::keys,
             "Iterate over keys in the map.")

        .def("keys", &PersistentMap::keys,
             "Return iterator over keys.\n\n"
             "Returns:\n"
             "    Iterator over all keys in the map")

        .def("values", &PersistentMap::values,
             "Return iterator over values.\n\n"
             "Returns:\n"
             "    Iterator over all values in the map")

        .def("items", &PersistentMap::items,
             "Return iterator over (key, value) pairs.\n\n"
             "Returns:\n"
             "    Iterator over all (key, value) tuples in the map")

        .def("__eq__",
             [](const PersistentMap& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentMap>(other)) {
                     return false;
                 }
                 return self == other.cast<const PersistentMap&>();
             },
             py::arg("other"),
             "Check equality with another map.\n\n"
             "Args:\n"
             "    other: Another object to compare with\n\n"
             "Returns:\n"
             "    True if maps are equal, False otherwise")

        .def("__ne__",
             [](const PersistentMap& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentMap>(other)) {
                     return true;
                 }
                 return self != other.cast<const PersistentMap&>();
             },
             py::arg("other"),
             "Check inequality with another map.")

        .def("__or__",
             [](const PersistentMap& self, py::object other) -> PersistentMap {
                 return self.update(other);
             },
             py::arg("other"),
             "Merge with another mapping using | operator.\n\n"
             "Args:\n"
             "    other: A dict, PersistentMap, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentMap with merged entries\n\n"
             "Example:\n"
             "    m1 = PersistentMap.create(a=1, b=2)\n"
             "    m2 = PersistentMap.create(c=3)\n"
             "    m3 = m1 | m2  # {'a': 1, 'b': 2, 'c': 3}")

        .def("__repr__", &PersistentMap::repr,
             "String representation of the map.")

        // Factory methods
        .def_static("from_dict", &PersistentMap::fromDict,
                   py::arg("dict"),
                   "Create PersistentMap from dictionary.\n\n"
                   "Args:\n"
                   "    dict: A Python dictionary\n\n"
                   "Returns:\n"
                   "    A new PersistentMap containing all key-value pairs from dict")

        .def_static("create", &PersistentMap::create,
                   "Create PersistentMap from keyword arguments.\n\n"
                   "Example:\n"
                   "    m = PersistentMap.create(a=1, b=2, c=3)\n\n"
                   "Returns:\n"
                   "    A new PersistentMap containing the keyword arguments");

    // Module-level documentation
    m.attr("__version__") = "1.0.0";
    m.attr("__doc__") = R"doc(
        Persistent Hash Map (HAMT) Implementation in C++

        This module provides a high-performance implementation of a persistent
        (immutable) hash map data structure using a Hash Array Mapped Trie (HAMT).

        Key features:
        - Immutability: All operations return new maps, leaving originals unchanged
        - Structural sharing: New versions share most of their structure with old versions
        - O(log32 n) operations: Efficient lookups, insertions, and deletions
        - Memory efficient: Structural sharing minimizes memory overhead
        - Thread-safe: Immutability makes concurrent access safe without locks

        Example usage:
            >>> from pypersistent import PersistentMap
            >>> m1 = PersistentMap()
            >>> m2 = m1.assoc('name', 'Alice').assoc('age', 30)
            >>> m2.get('name')
            'Alice'
            >>> len(m2)
            2
            >>> m1  # Original unchanged
            PersistentMap({})
            >>> m2
            PersistentMap({'name': 'Alice', 'age': 30})

        Performance characteristics:
        - 5-7x faster than pure Python implementation for insertions
        - 3-5x faster for lookups
        - 4-8x faster for creating variants (structural sharing)
        - Approaches dict performance for basic operations
        - Significantly faster than dict when creating many variants
    )doc";
}
