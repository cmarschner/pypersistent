#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include "persistent_dict.hpp"
#include "persistent_array_map.hpp"
#include "persistent_set.hpp"
#include "persistent_list.hpp"
#include "persistent_sorted_dict.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pypersistent, m) {
    m.doc() = "High-performance persistent hash map (HAMT) implementation in C++";

    // Initialize the NOT_FOUND sentinels
    PersistentDict::NOT_FOUND = py::object();
    PersistentArrayMap::NOT_FOUND = py::object();

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

    py::class_<PersistentDict>(m, "PersistentDict")
        .def(py::init<>(),
             "Create an empty PersistentDict")

        // Core methods
        .def("assoc", &PersistentDict::assoc,
             py::arg("key"), py::arg("val"),
             "Associate key with value, returning new map.\n\n"
             "Args:\n"
             "    key: The key (must be hashable)\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentDict with the association added")

        .def("dissoc", &PersistentDict::dissoc,
             py::arg("key"),
             "Remove key, returning new map.\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentDict with the key removed")

        .def("get", &PersistentDict::get,
             py::arg("key"), py::arg("default") = py::none(),
             "Get value for key, or default if not found.\n\n"
             "Args:\n"
             "    key: The key to look up\n"
             "    default: Value to return if key not found (default: None)\n\n"
             "Returns:\n"
             "    The value associated with key, or default")

        // Python-friendly aliases
        .def("set", &PersistentDict::set,
             py::arg("key"), py::arg("val"),
             "Pythonic alias for assoc(). Set key to value.\n\n"
             "Args:\n"
             "    key: The key\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentDict with the key set")

        .def("delete", &PersistentDict::delete_,
             py::arg("key"),
             "Pythonic alias for dissoc(). Delete key.\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentDict without the key")

        .def("update", &PersistentDict::update,
             py::arg("other"),
             "Merge another mapping, returning new map.\n\n"
             "Args:\n"
             "    other: A dict, PersistentDict, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentDict with merged entries")

        .def("merge", &PersistentDict::merge,
             py::arg("other"),
             "Alias for update(). Merge mappings.\n\n"
             "Args:\n"
             "    other: A dict, PersistentDict, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentDict with merged entries")

        .def("clear", &PersistentDict::clear,
             "Return an empty PersistentDict.\n\n"
             "Returns:\n"
             "    An empty PersistentDict")

        .def("copy", &PersistentDict::copy,
             "Return self (no-op for immutable).\n\n"
             "Returns:\n"
             "    Self")

        // Python protocols
        .def("__getitem__",
             [](const PersistentDict& m, py::object key) -> py::object {
                 py::object result = m.get(key, PersistentDict::NOT_FOUND);
                 if (result.is(PersistentDict::NOT_FOUND)) {
                     throw py::key_error(py::str(key));
                 }
                 return result;
             },
             py::arg("key"),
             "Get item using bracket notation. Raises KeyError if not found.")

        .def("__contains__", &PersistentDict::contains,
             py::arg("key"),
             "Check if key is in map.\n\n"
             "Args:\n"
             "    key: The key to check\n\n"
             "Returns:\n"
             "    True if key is present, False otherwise")

        .def("__len__", &PersistentDict::size,
             "Return number of entries in the map.")

        .def("__iter__", &PersistentDict::keys,
             "Iterate over keys in the map.")

        .def("keys", &PersistentDict::keys,
             "Return iterator over keys.\n\n"
             "Returns:\n"
             "    Iterator over all keys in the map")

        .def("values", &PersistentDict::values,
             "Return iterator over values.\n\n"
             "Returns:\n"
             "    Iterator over all values in the map")

        .def("items", &PersistentDict::items,
             "Return iterator over (key, value) pairs.\n\n"
             "Returns:\n"
             "    Iterator over all (key, value) tuples in the map")

        // Fast materialized iteration (returns lists instead of iterators)
        .def("items_list", &PersistentDict::itemsList,
             "Return list of (key, value) tuples (3-4x faster than items() for full iteration).\n\n"
             "This method materializes all items into a list at once, which is much faster\n"
             "than using the iterator for complete iteration (single boundary crossing).\n\n"
             "Returns:\n"
             "    List of all (key, value) tuples in the map\n\n"
             "Performance:\n"
             "    - 3-4x faster than items() iterator for full iteration\n"
             "    - Single Python/C++ boundary crossing vs one per item\n"
             "    - Use this when you need all items at once\n"
             "    - Use items() iterator when you want lazy evaluation")

        .def("keys_list", &PersistentDict::keysList,
             "Return list of all keys (3-4x faster than keys() for full iteration).\n\n"
             "Returns:\n"
             "    List of all keys in the map")

        .def("values_list", &PersistentDict::valuesList,
             "Return list of all values (3-4x faster than values() for full iteration).\n\n"
             "Returns:\n"
             "    List of all values in the map")

        .def("__eq__",
             [](const PersistentDict& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentDict>(other)) {
                     return false;
                 }
                 return self == other.cast<const PersistentDict&>();
             },
             py::arg("other"),
             "Check equality with another map.\n\n"
             "Args:\n"
             "    other: Another object to compare with\n\n"
             "Returns:\n"
             "    True if maps are equal, False otherwise")

        .def("__ne__",
             [](const PersistentDict& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentDict>(other)) {
                     return true;
                 }
                 return self != other.cast<const PersistentDict&>();
             },
             py::arg("other"),
             "Check inequality with another map.")

        .def("__or__",
             [](const PersistentDict& self, py::object other) -> PersistentDict {
                 return self.update(other);
             },
             py::arg("other"),
             "Merge with another mapping using | operator.\n\n"
             "Args:\n"
             "    other: A dict, PersistentDict, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentDict with merged entries\n\n"
             "Example:\n"
             "    m1 = PersistentDict.create(a=1, b=2)\n"
             "    m2 = PersistentDict.create(c=3)\n"
             "    m3 = m1 | m2  # {'a': 1, 'b': 2, 'c': 3}")

        .def("__repr__", &PersistentDict::repr,
             "String representation of the map.")

        // Factory methods
        .def_static("from_dict", &PersistentDict::fromDict,
                   py::arg("dict"),
                   "Create PersistentDict from dictionary.\n\n"
                   "Args:\n"
                   "    dict: A Python dictionary\n\n"
                   "Returns:\n"
                   "    A new PersistentDict containing all key-value pairs from dict")

        .def_static("create", &PersistentDict::create,
                   "Create PersistentDict from keyword arguments.\n\n"
                   "Example:\n"
                   "    m = PersistentDict.create(a=1, b=2, c=3)\n\n"
                   "Returns:\n"
                   "    A new PersistentDict containing the keyword arguments")

        // Pickle support
        .def(py::pickle(
            [](const PersistentDict &p) { // __getstate__
                return p.itemsList();  // Return list of (key, value) tuples
            },
            [](py::list items) { // __setstate__
                py::dict d;
                for (auto item : items) {
                    py::tuple t = item.cast<py::tuple>();
                    d[t[0]] = t[1];
                }
                return PersistentDict::fromDict(d);
            }
        ));

    // PersistentArrayMap iterators
    py::class_<ArrayMapKeyIterator>(m, "ArrayMapKeyIterator")
        .def("__iter__", [](ArrayMapKeyIterator &it) -> ArrayMapKeyIterator& { return it; })
        .def("__next__", &ArrayMapKeyIterator::next);

    py::class_<ArrayMapValueIterator>(m, "ArrayMapValueIterator")
        .def("__iter__", [](ArrayMapValueIterator &it) -> ArrayMapValueIterator& { return it; })
        .def("__next__", &ArrayMapValueIterator::next);

    py::class_<ArrayMapItemIterator>(m, "ArrayMapItemIterator")
        .def("__iter__", [](ArrayMapItemIterator &it) -> ArrayMapItemIterator& { return it; })
        .def("__next__", &ArrayMapItemIterator::next);

    // PersistentArrayMap
    py::class_<PersistentArrayMap>(m, "PersistentArrayMap")
        .def(py::init<>(),
             "Create an empty PersistentArrayMap")

        // Core methods
        .def("assoc", &PersistentArrayMap::assoc,
             py::arg("key"), py::arg("val"),
             "Associate key with value, returning new map.\n\n"
             "Args:\n"
             "    key: The key (must be hashable)\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap with the association added\n\n"
             "Note: Raises RuntimeError if map would exceed 8 entries")

        .def("dissoc", &PersistentArrayMap::dissoc,
             py::arg("key"),
             "Remove key, returning new map.\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap with the key removed")

        .def("get", &PersistentArrayMap::get,
             py::arg("key"), py::arg("default") = py::none(),
             "Get value for key, or default if not found.\n\n"
             "Args:\n"
             "    key: The key to look up\n"
             "    default: Value to return if key not found (default: None)\n\n"
             "Returns:\n"
             "    The value associated with key, or default")

        .def("contains", &PersistentArrayMap::contains,
             py::arg("key"),
             "Check if key exists in the map.\n\n"
             "Args:\n"
             "    key: The key to check\n\n"
             "Returns:\n"
             "    True if key is present, False otherwise")

        // Python-friendly aliases
        .def("set", &PersistentArrayMap::set,
             py::arg("key"), py::arg("val"),
             "Pythonic alias for assoc(). Set key to value.\n\n"
             "Args:\n"
             "    key: The key\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap with the key set")

        .def("delete", &PersistentArrayMap::delete_,
             py::arg("key"),
             "Pythonic alias for dissoc(). Delete key.\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap without the key")

        .def("update", &PersistentArrayMap::update,
             py::arg("other"),
             "Merge another mapping, returning new map.\n\n"
             "Args:\n"
             "    other: A dict, PersistentArrayMap, PersistentDict, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap with merged entries")

        .def("merge", &PersistentArrayMap::merge,
             py::arg("other"),
             "Alias for update(). Merge mappings.\n\n"
             "Args:\n"
             "    other: A dict, PersistentArrayMap, PersistentDict, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap with merged entries")

        .def("clear", &PersistentArrayMap::clear,
             "Return an empty PersistentArrayMap.\n\n"
             "Returns:\n"
             "    An empty PersistentArrayMap")

        .def("copy", &PersistentArrayMap::copy,
             "Return self (no-op for immutable).\n\n"
             "Returns:\n"
             "    Self")

        // Python protocols
        .def("__getitem__",
             [](const PersistentArrayMap& m, py::object key) -> py::object {
                 py::object result = m.get(key, PersistentArrayMap::NOT_FOUND);
                 if (result.is(PersistentArrayMap::NOT_FOUND)) {
                     throw py::key_error(py::str(key));
                 }
                 return result;
             },
             py::arg("key"),
             "Get item using bracket notation. Raises KeyError if not found.")

        .def("__contains__", &PersistentArrayMap::contains,
             py::arg("key"),
             "Check if key is in map.\n\n"
             "Args:\n"
             "    key: The key to check\n\n"
             "Returns:\n"
             "    True if key is present, False otherwise")

        .def("__len__", &PersistentArrayMap::size,
             "Return number of entries in the map.")

        .def("__iter__", &PersistentArrayMap::keys,
             "Iterate over keys in the map.")

        .def("keys", &PersistentArrayMap::keys,
             "Return iterator over keys.\n\n"
             "Returns:\n"
             "    Iterator over all keys in the map")

        .def("values", &PersistentArrayMap::values,
             "Return iterator over values.\n\n"
             "Returns:\n"
             "    Iterator over all values in the map")

        .def("items", &PersistentArrayMap::items,
             "Return iterator over (key, value) pairs.\n\n"
             "Returns:\n"
             "    Iterator over all (key, value) tuples in the map")

        // Fast materialized iteration
        .def("items_list", &PersistentArrayMap::itemsList,
             "Return list of (key, value) tuples (faster than items() for small maps).\n\n"
             "Returns:\n"
             "    List of all (key, value) tuples in the map")

        .def("keys_list", &PersistentArrayMap::keysList,
             "Return list of all keys (faster than keys() for small maps).\n\n"
             "Returns:\n"
             "    List of all keys in the map")

        .def("values_list", &PersistentArrayMap::valuesList,
             "Return list of all values (faster than values() for small maps).\n\n"
             "Returns:\n"
             "    List of all values in the map")

        .def("__eq__",
             [](const PersistentArrayMap& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentArrayMap>(other)) {
                     return false;
                 }
                 return self == other.cast<const PersistentArrayMap&>();
             },
             py::arg("other"),
             "Check equality with another map.\n\n"
             "Args:\n"
             "    other: Another object to compare with\n\n"
             "Returns:\n"
             "    True if maps are equal, False otherwise")

        .def("__ne__",
             [](const PersistentArrayMap& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentArrayMap>(other)) {
                     return true;
                 }
                 return self != other.cast<const PersistentArrayMap&>();
             },
             py::arg("other"),
             "Check inequality with another map.")

        .def("__or__",
             [](const PersistentArrayMap& self, py::object other) -> PersistentArrayMap {
                 return self.update(other);
             },
             py::arg("other"),
             "Merge with another mapping using | operator.\n\n"
             "Args:\n"
             "    other: A dict, PersistentArrayMap, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap with merged entries\n\n"
             "Example:\n"
             "    m1 = PersistentArrayMap.create(a=1, b=2)\n"
             "    m2 = PersistentArrayMap.create(c=3)\n"
             "    m3 = m1 | m2  # {'a': 1, 'b': 2, 'c': 3}")

        .def("__repr__", &PersistentArrayMap::repr,
             "String representation of the map.")

        // Factory methods
        .def_static("from_dict", &PersistentArrayMap::fromDict,
                   py::arg("dict"),
                   "Create PersistentArrayMap from dictionary.\n\n"
                   "Args:\n"
                   "    dict: A Python dictionary (max 8 entries)\n\n"
                   "Returns:\n"
                   "    A new PersistentArrayMap containing all key-value pairs from dict\n\n"
                   "Raises:\n"
                   "    RuntimeError: If dictionary has more than 8 entries")

        .def_static("create", &PersistentArrayMap::create,
                   "Create PersistentArrayMap from keyword arguments.\n\n"
                   "Example:\n"
                   "    m = PersistentArrayMap.create(a=1, b=2, c=3)\n\n"
                   "Returns:\n"
                   "    A new PersistentArrayMap containing the keyword arguments\n\n"
                   "Raises:\n"
                   "    RuntimeError: If more than 8 keyword arguments provided");

    // PersistentSet iterator
    py::class_<SetIterator>(m, "SetIterator")
        .def("__iter__", [](SetIterator &it) -> SetIterator& { return it; })
        .def("__next__", &SetIterator::next);

    // PersistentSet
    py::class_<PersistentSet>(m, "PersistentSet")
        .def(py::init<>(),
             "Create an empty PersistentSet")

        // Core methods
        .def("conj", &PersistentSet::conj,
             py::arg("elem"),
             "Add element to set, returning new set.\n\n"
             "Args:\n"
             "    elem: The element to add (must be hashable)\n\n"
             "Returns:\n"
             "    A new PersistentSet with the element added")

        .def("disj", &PersistentSet::disj,
             py::arg("elem"),
             "Remove element from set, returning new set.\n\n"
             "Args:\n"
             "    elem: The element to remove\n\n"
             "Returns:\n"
             "    A new PersistentSet with the element removed")

        .def("contains", &PersistentSet::contains,
             py::arg("elem"),
             "Check if element is in set.\n\n"
             "Args:\n"
             "    elem: The element to check\n\n"
             "Returns:\n"
             "    True if element is present, False otherwise")

        // Set operations
        .def("union", &PersistentSet::union_,
             py::arg("other"),
             "Return union of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with all elements from both sets")

        .def("intersection", &PersistentSet::intersection,
             py::arg("other"),
             "Return intersection of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with only elements in both sets")

        .def("difference", &PersistentSet::difference,
             py::arg("other"),
             "Return difference of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with elements in this set but not in other")

        .def("symmetric_difference", &PersistentSet::symmetric_difference,
             py::arg("other"),
             "Return symmetric difference of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with elements in either set but not both")

        // Set predicates
        .def("issubset", &PersistentSet::issubset,
             py::arg("other"),
             "Test if this set is a subset of other.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    True if all elements of this set are in other")

        .def("issuperset", &PersistentSet::issuperset,
             py::arg("other"),
             "Test if this set is a superset of other.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    True if all elements of other are in this set")

        .def("isdisjoint", &PersistentSet::isdisjoint,
             py::arg("other"),
             "Test if this set has no elements in common with other.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    True if sets have no common elements")

        // Python-friendly aliases
        .def("add", &PersistentSet::add,
             py::arg("elem"),
             "Pythonic alias for conj(). Add element to set.\n\n"
             "Args:\n"
             "    elem: The element to add\n\n"
             "Returns:\n"
             "    A new PersistentSet with the element added")

        .def("remove", &PersistentSet::remove,
             py::arg("elem"),
             "Pythonic alias for disj(). Remove element from set.\n\n"
             "Args:\n"
             "    elem: The element to remove\n\n"
             "Returns:\n"
             "    A new PersistentSet with the element removed")

        .def("update", &PersistentSet::update,
             py::arg("other"),
             "Add all elements from another iterable.\n\n"
             "Args:\n"
             "    other: A set, PersistentSet, list, or iterable\n\n"
             "Returns:\n"
             "    A new PersistentSet with all elements added")

        .def("clear", &PersistentSet::clear,
             "Return an empty PersistentSet.\n\n"
             "Returns:\n"
             "    An empty PersistentSet")

        .def("copy", &PersistentSet::copy,
             "Return self (no-op for immutable).\n\n"
             "Returns:\n"
             "    Self")

        // Python protocols
        .def("__contains__", &PersistentSet::contains,
             py::arg("elem"),
             "Check if element is in set.\n\n"
             "Args:\n"
             "    elem: The element to check\n\n"
             "Returns:\n"
             "    True if element is present, False otherwise")

        .def("__len__", &PersistentSet::size,
             "Return number of elements in the set.")

        .def("__iter__", &PersistentSet::iter,
             "Iterate over elements in the set.")

        .def("list", &PersistentSet::list,
             "Return list of all elements.\n\n"
             "Returns:\n"
             "    List of all elements in the set")

        // Set operators
        .def("__or__", &PersistentSet::union_,
             py::arg("other"),
             "Union using | operator.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with all elements from both sets")

        .def("__and__", &PersistentSet::intersection,
             py::arg("other"),
             "Intersection using & operator.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with only elements in both sets")

        .def("__sub__", &PersistentSet::difference,
             py::arg("other"),
             "Difference using - operator.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with elements in this set but not in other")

        .def("__xor__", &PersistentSet::symmetric_difference,
             py::arg("other"),
             "Symmetric difference using ^ operator.\n\n"
             "Args:\n"
             "    other: Another PersistentSet\n\n"
             "Returns:\n"
             "    A new PersistentSet with elements in either set but not both")

        .def("__le__", &PersistentSet::issubset,
             py::arg("other"),
             "Subset test using <= operator.")

        .def("__ge__", &PersistentSet::issuperset,
             py::arg("other"),
             "Superset test using >= operator.")

        .def("__lt__",
             [](const PersistentSet& self, const PersistentSet& other) -> bool {
                 return self.issubset(other) && self != other;
             },
             py::arg("other"),
             "Proper subset test using < operator.")

        .def("__gt__",
             [](const PersistentSet& self, const PersistentSet& other) -> bool {
                 return self.issuperset(other) && self != other;
             },
             py::arg("other"),
             "Proper superset test using > operator.")

        .def("__eq__",
             [](const PersistentSet& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentSet>(other)) {
                     return false;
                 }
                 return self == other.cast<const PersistentSet&>();
             },
             py::arg("other"),
             "Check equality with another set.\n\n"
             "Args:\n"
             "    other: Another object to compare with\n\n"
             "Returns:\n"
             "    True if sets are equal, False otherwise")

        .def("__ne__",
             [](const PersistentSet& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentSet>(other)) {
                     return true;
                 }
                 return self != other.cast<const PersistentSet&>();
             },
             py::arg("other"),
             "Check inequality with another set.")

        .def("__repr__", &PersistentSet::repr,
             "String representation of the set.")

        // Factory methods
        .def_static("from_set", &PersistentSet::fromSet,
                   py::arg("set"),
                   "Create PersistentSet from Python set.\n\n"
                   "Args:\n"
                   "    set: A Python set\n\n"
                   "Returns:\n"
                   "    A new PersistentSet containing all elements from set")

        .def_static("from_list", &PersistentSet::fromList,
                   py::arg("list"),
                   "Create PersistentSet from list (duplicates removed).\n\n"
                   "Args:\n"
                   "    list: A Python list\n\n"
                   "Returns:\n"
                   "    A new PersistentSet containing unique elements from list")

        .def_static("from_iterable", &PersistentSet::fromIterable,
                   py::arg("iterable"),
                   "Create PersistentSet from any iterable.\n\n"
                   "Args:\n"
                   "    iterable: Any Python iterable\n\n"
                   "Returns:\n"
                   "    A new PersistentSet containing unique elements from iterable")

        .def_static("create", &PersistentSet::create,
                   "Create PersistentSet from arguments.\n\n"
                   "Example:\n"
                   "    s = PersistentSet.create(1, 2, 3)\n\n"
                   "Returns:\n"
                   "    A new PersistentSet containing the arguments")

        // Pickle support
        .def(py::pickle(
            [](const PersistentSet &p) { // __getstate__
                return p.list();  // Return list of elements
            },
            [](py::list items) { // __setstate__
                return PersistentSet::fromList(items);
            }
        ));

    // PersistentList iterator
    py::class_<VectorIterator>(m, "VectorIterator")
        .def("__iter__", [](VectorIterator &it) -> VectorIterator& { return it; })
        .def("__next__", &VectorIterator::next);

    // PersistentList
    py::class_<PersistentList>(m, "PersistentList")
        .def(py::init<>(),
             "Create an empty PersistentList")

        // Core methods
        .def("conj", &PersistentList::conj,
             py::arg("val"),
             "Append value to end of vector, returning new vector.\n\n"
             "Args:\n"
             "    val: The value to append\n\n"
             "Returns:\n"
             "    A new PersistentList with the value appended\n\n"
             "Complexity: O(1) amortized")

        .def("assoc", &PersistentList::assoc,
             py::arg("idx"), py::arg("val"),
             "Update value at index, returning new vector.\n\n"
             "Args:\n"
             "    idx: The index to update\n"
             "    val: The new value\n\n"
             "Returns:\n"
             "    A new PersistentList with the value updated\n\n"
             "Complexity: O(log32 n)\n"
             "Raises:\n"
             "    IndexError: If index is out of range")

        .def("nth", &PersistentList::nth,
             py::arg("idx"),
             "Get value at index.\n\n"
             "Args:\n"
             "    idx: The index to retrieve\n\n"
             "Returns:\n"
             "    The value at the index\n\n"
             "Complexity: O(log32 n)\n"
             "Raises:\n"
             "    IndexError: If index is out of range")

        .def("get", &PersistentList::get,
             py::arg("idx"), py::arg("default") = py::none(),
             "Get value at index, or default if out of range.\n\n"
             "Args:\n"
             "    idx: The index to retrieve\n"
             "    default: Value to return if index is out of range (default: None)\n\n"
             "Returns:\n"
             "    The value at the index, or default")

        .def("pop", &PersistentList::pop,
             "Remove last element, returning new vector.\n\n"
             "Returns:\n"
             "    A new PersistentList with the last element removed\n\n"
             "Raises:\n"
             "    RuntimeError: If vector is empty")

        // Python-friendly aliases
        .def("append", &PersistentList::append,
             py::arg("val"),
             "Pythonic alias for conj(). Append value to end.\n\n"
             "Args:\n"
             "    val: The value to append\n\n"
             "Returns:\n"
             "    A new PersistentList with the value appended")

        .def("set", &PersistentList::set,
             py::arg("idx"), py::arg("val"),
             "Pythonic alias for assoc(). Set value at index.\n\n"
             "Args:\n"
             "    idx: The index to update\n"
             "    val: The new value\n\n"
             "Returns:\n"
             "    A new PersistentList with the value updated")

        // Python protocols
        .def("__getitem__",
             [](const PersistentList& v, py::object key) -> py::object {
                 // Handle slice
                 if (py::isinstance<py::slice>(key)) {
                     py::slice slice = key.cast<py::slice>();
                     Py_ssize_t start, stop, step, slicelength;
                     if (!slice.compute(v.size(), &start, &stop, &step, &slicelength)) {
                         throw py::error_already_set();
                     }
                     if (step != 1) {
                         throw std::invalid_argument("PersistentList slicing does not support step != 1");
                     }
                     return py::cast(v.slice(start, stop));
                 }

                 // Handle integer index
                 if (py::isinstance<py::int_>(key)) {
                     Py_ssize_t idx = key.cast<Py_ssize_t>();
                     // Handle negative indices
                     if (idx < 0) {
                         idx += v.size();
                     }
                     if (idx < 0 || idx >= static_cast<Py_ssize_t>(v.size())) {
                         throw py::index_error("PersistentList index out of range");
                     }
                     return v.nth(idx);
                 }

                 throw py::type_error("PersistentList indices must be integers or slices, not " +
                                     std::string(py::str(py::type::of(key))));
             },
             py::arg("key"),
             "Get item using bracket notation. Supports integers (negative allowed) and slices.\n\n"
             "Examples:\n"
             "    v[0]      # First element\n"
             "    v[-1]     # Last element\n"
             "    v[1:4]    # Slice from index 1 to 3")

        .def("__len__", &PersistentList::size,
             "Return number of elements in the vector.")

        .def("__iter__", &PersistentList::iter,
             "Iterate over elements in the vector.")

        .def("__contains__",
             [](const PersistentList& v, py::object val) -> bool {
                 for (size_t i = 0; i < v.size(); ++i) {
                     py::object elem = v.nth(i);
                     int eq = PyObject_RichCompareBool(elem.ptr(), val.ptr(), Py_EQ);
                     if (eq == 1) return true;
                 }
                 return false;
             },
             py::arg("val"),
             "Check if value is in vector.\n\n"
             "Args:\n"
             "    val: The value to check\n\n"
             "Returns:\n"
             "    True if value is present, False otherwise")

        .def("list", &PersistentList::list,
             "Convert to Python list.\n\n"
             "Returns:\n"
             "    Python list containing all elements")

        .def("slice", &PersistentList::slice,
             py::arg("start"), py::arg("stop"),
             "Return slice of vector.\n\n"
             "Args:\n"
             "    start: Start index (inclusive)\n"
             "    stop: Stop index (exclusive)\n\n"
             "Returns:\n"
             "    A new PersistentList containing the slice")

        .def("__eq__",
             [](const PersistentList& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentList>(other)) {
                     return false;
                 }
                 return self == other.cast<const PersistentList&>();
             },
             py::arg("other"),
             "Check equality with another vector.\n\n"
             "Args:\n"
             "    other: Another object to compare with\n\n"
             "Returns:\n"
             "    True if vectors are equal, False otherwise")

        .def("__ne__",
             [](const PersistentList& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentList>(other)) {
                     return true;
                 }
                 return self != other.cast<const PersistentList&>();
             },
             py::arg("other"),
             "Check inequality with another vector.")

        .def("__repr__", &PersistentList::repr,
             "String representation of the vector.")

        // Factory methods
        .def_static("from_list", &PersistentList::fromList,
                   py::arg("list"),
                   "Create PersistentList from Python list.\n\n"
                   "Args:\n"
                   "    list: A Python list\n\n"
                   "Returns:\n"
                   "    A new PersistentList containing all elements from list")

        .def_static("from_iterable", &PersistentList::fromIterable,
                   py::arg("iterable"),
                   "Create PersistentList from any iterable.\n\n"
                   "Args:\n"
                   "    iterable: Any Python iterable\n\n"
                   "Returns:\n"
                   "    A new PersistentList containing all elements from iterable")

        .def_static("create", &PersistentList::create,
                   "Create PersistentList from arguments.\n\n"
                   "Example:\n"
                   "    v = PersistentList.create(1, 2, 3, 4, 5)\n\n"
                   "Returns:\n"
                   "    A new PersistentList containing the arguments")

        // Pickle support
        .def(py::pickle(
            [](const PersistentList &p) { // __getstate__
                py::list result;
                for (size_t i = 0; i < p.size(); i++) {
                    result.append(p.nth(i));
                }
                return result;
            },
            [](py::list items) { // __setstate__
                return PersistentList::fromList(items);
            }
        ));

    // PersistentSortedDict iterator
    py::class_<TreeMapIteratorWrapper>(m, "TreeMapIteratorWrapper")
        .def("__iter__", &TreeMapIteratorWrapper::iter)
        .def("__next__", &TreeMapIteratorWrapper::next);

    // PersistentSortedDict
    py::class_<PersistentSortedDict>(m, "PersistentSortedDict")
        .def(py::init<>(),
             "Create an empty PersistentSortedDict (sorted map)")

        // Core methods
        .def("assoc", &PersistentSortedDict::assoc,
             py::arg("key"), py::arg("val"),
             "Associate key with value, returning new sorted map.\n\n"
             "Args:\n"
             "    key: The key (must support < comparison)\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with the association added\n\n"
             "Complexity: O(log n)")

        .def("dissoc", &PersistentSortedDict::dissoc,
             py::arg("key"),
             "Remove key, returning new sorted map.\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with the key removed\n\n"
             "Complexity: O(log n)")

        .def("get", py::overload_cast<const py::object&>(&PersistentSortedDict::get, py::const_),
             py::arg("key"),
             "Get value for key.\n\n"
             "Args:\n"
             "    key: The key to look up\n\n"
             "Returns:\n"
             "    The value associated with key\n\n"
             "Raises:\n"
             "    KeyError: If key not found\n\n"
             "Complexity: O(log n)")

        .def("get", py::overload_cast<const py::object&, const py::object&>(&PersistentSortedDict::get, py::const_),
             py::arg("key"), py::arg("default"),
             "Get value for key, or default if not found.\n\n"
             "Args:\n"
             "    key: The key to look up\n"
             "    default: Value to return if key not found\n\n"
             "Returns:\n"
             "    The value associated with key, or default\n\n"
             "Complexity: O(log n)")

        .def("contains", &PersistentSortedDict::contains,
             py::arg("key"),
             "Check if key exists in the map.\n\n"
             "Args:\n"
             "    key: The key to check\n\n"
             "Returns:\n"
             "    True if key is present, False otherwise\n\n"
             "Complexity: O(log n)")

        // Ordered operations
        .def("first", &PersistentSortedDict::first,
             "Get [key, value] of smallest key.\n\n"
             "Returns:\n"
             "    List [key, value] for the minimum key\n\n"
             "Raises:\n"
             "    RuntimeError: If map is empty\n\n"
             "Complexity: O(log n)")

        .def("last", &PersistentSortedDict::last,
             "Get [key, value] of largest key.\n\n"
             "Returns:\n"
             "    List [key, value] for the maximum key\n\n"
             "Raises:\n"
             "    RuntimeError: If map is empty\n\n"
             "Complexity: O(log n)")

        .def("subseq", &PersistentSortedDict::subseq,
             py::arg("start"), py::arg("end"),
             "Get subsequence of keys in range [start, end).\n\n"
             "Args:\n"
             "    start: Start key (inclusive)\n"
             "    end: End key (exclusive)\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with keys in [start, end)\n\n"
             "Complexity: O(m + log n) where m is output size")

        .def("rsubseq", &PersistentSortedDict::rsubseq,
             py::arg("start"), py::arg("end"),
             "Get reversed subsequence of keys in range [start, end).\n\n"
             "Args:\n"
             "    start: Start key (inclusive)\n"
             "    end: End key (exclusive)\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with keys in [start, end) in reverse order\n\n"
             "Complexity: O(m + log n) where m is output size")

        // Python-friendly aliases
        .def("set", &PersistentSortedDict::assoc,
             py::arg("key"), py::arg("val"),
             "Pythonic alias for assoc(). Set key to value.\n\n"
             "Args:\n"
             "    key: The key\n"
             "    val: The value\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with the key set")

        // Python protocols
        .def("__getitem__",
             [](const PersistentSortedDict& m, py::object key) -> py::object {
                 // Handle slice for range queries
                 if (py::isinstance<py::slice>(key)) {
                     py::slice slice = key.cast<py::slice>();
                     // Extract start and stop from slice (step not supported)
                     py::object start_obj = slice.attr("start");
                     py::object stop_obj = slice.attr("stop");
                     py::object step_obj = slice.attr("step");

                     // Check that step is None or 1
                     if (!step_obj.is_none() && step_obj.cast<int>() != 1) {
                         throw std::invalid_argument("PersistentSortedDict slicing does not support step != 1");
                     }

                     // Both start and stop must be provided for range queries
                     if (start_obj.is_none() || stop_obj.is_none()) {
                         throw std::invalid_argument("PersistentSortedDict slicing requires both start and stop keys");
                     }

                     // Call subseq with the start and stop keys
                     return py::cast(m.subseq(start_obj, stop_obj));
                 }

                 // Handle regular key lookup
                 return m.pyGetItem(key);
             },
             py::arg("key"),
             "Get item using bracket notation. Raises KeyError if not found.\n\n"
             "Args:\n"
             "    key: The key to look up, or a slice for range queries\n\n"
             "Returns:\n"
             "    The value associated with key, or a PersistentSortedDict for slices\n\n"
             "Raises:\n"
             "    KeyError: If key not found")

        .def("__contains__", &PersistentSortedDict::pyContains,
             py::arg("key"),
             "Check if key is in map.\n\n"
             "Args:\n"
             "    key: The key to check\n\n"
             "Returns:\n"
             "    True if key is present, False otherwise")

        .def("__len__", &PersistentSortedDict::size,
             "Return number of entries in the map.")

        .def("__iter__",
             [](const PersistentSortedDict& m) -> py::iterator {
                 return py::iter(m.keysList());
             },
             "Iterate over keys in sorted order.")

        .def("keys_list", &PersistentSortedDict::keysList,
             "Return list of all keys in sorted order.\n\n"
             "Returns:\n"
             "    List of all keys in ascending order")

        .def("values_list", &PersistentSortedDict::valuesList,
             "Return list of all values in key-sorted order.\n\n"
             "Returns:\n"
             "    List of all values ordered by their keys")

        .def("items", &PersistentSortedDict::items,
             "Return list of (key, value) pairs in sorted order.\n\n"
             "Returns:\n"
             "    List of all (key, value) tuples ordered by key")

        .def("dict", &PersistentSortedDict::dict,
             "Convert to Python dict.\n\n"
             "Returns:\n"
             "    Python dict containing all key-value pairs")

        .def("__eq__",
             [](const PersistentSortedDict& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentSortedDict>(other)) {
                     return false;
                 }
                 return self == other.cast<const PersistentSortedDict&>();
             },
             py::arg("other"),
             "Check equality with another map.\n\n"
             "Args:\n"
             "    other: Another object to compare with\n\n"
             "Returns:\n"
             "    True if maps are equal, False otherwise")

        .def("__ne__",
             [](const PersistentSortedDict& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentSortedDict>(other)) {
                     return true;
                 }
                 return self != other.cast<const PersistentSortedDict&>();
             },
             py::arg("other"),
             "Check inequality with another map.")

        .def("__or__",
             [](const PersistentSortedDict& self, py::object other) -> PersistentSortedDict {
                 PersistentSortedDict result = self;

                 // Handle dict
                 if (py::isinstance<py::dict>(other)) {
                     py::dict d = other.cast<py::dict>();
                     for (auto item : d) {
                         result = result.assoc(
                             py::reinterpret_borrow<py::object>(item.first),
                             py::reinterpret_borrow<py::object>(item.second)
                         );
                     }
                 }
                 // Handle PersistentSortedDict
                 else if (py::isinstance<PersistentSortedDict>(other)) {
                     const PersistentSortedDict& other_map = other.cast<const PersistentSortedDict&>();
                     py::list items = other_map.items();
                     for (auto item : items) {
                         py::list pair = item.cast<py::list>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 // Handle PersistentDict
                 else if (py::isinstance<PersistentDict>(other)) {
                     const PersistentDict& other_map = other.cast<const PersistentDict&>();
                     py::list items = other_map.itemsList();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 // Handle PersistentArrayMap
                 else if (py::isinstance<PersistentArrayMap>(other)) {
                     const PersistentArrayMap& other_map = other.cast<const PersistentArrayMap&>();
                     py::list items = other_map.itemsList();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 // Handle any mapping with items() method
                 else if (py::hasattr(other, "items")) {
                     py::object items_method = other.attr("items");
                     py::object items = items_method();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 else {
                     throw py::type_error("Cannot merge PersistentSortedDict with non-mapping type");
                 }

                 return result;
             },
             py::arg("other"),
             "Merge with another mapping using | operator.\n\n"
             "Args:\n"
             "    other: A dict, PersistentSortedDict, PersistentDict, PersistentArrayMap, or any mapping\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with merged entries (right side wins on conflicts)\n\n"
             "Example:\n"
             "    tm1 = PersistentSortedDict.create(a=1, b=2)\n"
             "    tm2 = PersistentSortedDict.create(c=3, d=4)\n"
             "    tm3 = tm1 | tm2  # PersistentSortedDict({a: 1, b: 2, c: 3, d: 4})")

        .def("__repr__", &PersistentSortedDict::repr,
             "String representation of the sorted map.")

        // Additional methods for API consistency with PersistentDict/ArrayMap
        .def("items_list", &PersistentSortedDict::items,
             "Return list of (key, value) tuples in sorted order.\n\n"
             "Alias for items() - provided for API consistency with PersistentDict.\n\n"
             "Returns:\n"
             "    List of (key, value) tuples in ascending key order")

        .def("keys",
             [](const PersistentSortedDict& m) -> py::iterator {
                 return py::iter(m.keysList());
             },
             "Iterate over keys in sorted order.\n\n"
             "Returns:\n"
             "    Iterator over keys in ascending order")

        .def("values",
             [](const PersistentSortedDict& m) -> py::iterator {
                 return py::iter(m.valuesList());
             },
             "Iterate over values in key-sorted order.\n\n"
             "Returns:\n"
             "    Iterator over values ordered by their keys")

        .def("update",
             [](const PersistentSortedDict& self, py::object other) -> PersistentSortedDict {
                 // Reuse the __or__ implementation
                 PersistentSortedDict result = self;

                 // Handle dict
                 if (py::isinstance<py::dict>(other)) {
                     py::dict d = other.cast<py::dict>();
                     for (auto item : d) {
                         result = result.assoc(
                             py::reinterpret_borrow<py::object>(item.first),
                             py::reinterpret_borrow<py::object>(item.second)
                         );
                     }
                 }
                 // Handle PersistentSortedDict
                 else if (py::isinstance<PersistentSortedDict>(other)) {
                     const PersistentSortedDict& other_map = other.cast<const PersistentSortedDict&>();
                     py::list items = other_map.items();
                     for (auto item : items) {
                         py::list pair = item.cast<py::list>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 // Handle PersistentDict
                 else if (py::isinstance<PersistentDict>(other)) {
                     const PersistentDict& other_map = other.cast<const PersistentDict&>();
                     py::list items = other_map.itemsList();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 // Handle PersistentArrayMap
                 else if (py::isinstance<PersistentArrayMap>(other)) {
                     const PersistentArrayMap& other_map = other.cast<const PersistentArrayMap&>();
                     py::list items = other_map.itemsList();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 // Handle any mapping with items() method
                 else if (py::hasattr(other, "items")) {
                     py::object items_method = other.attr("items");
                     py::object items = items_method();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 else {
                     throw py::type_error("Cannot update PersistentSortedDict with non-mapping type");
                 }

                 return result;
             },
             py::arg("other"),
             "Update map with entries from another mapping.\n\n"
             "Args:\n"
             "    other: A dict, PersistentSortedDict, PersistentDict, PersistentArrayMap, or any mapping\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with entries from both maps (right side wins)\n\n"
             "Example:\n"
             "    tm1 = PersistentSortedDict.create(a=1, b=2)\n"
             "    tm2 = tm1.update({'c': 3, 'd': 4})")

        .def("merge",
             [](const PersistentSortedDict& self, py::object other) -> PersistentSortedDict {
                 // Same implementation as update (code duplication for simplicity)
                 PersistentSortedDict result = self;

                 if (py::isinstance<py::dict>(other)) {
                     py::dict d = other.cast<py::dict>();
                     for (auto item : d) {
                         result = result.assoc(
                             py::reinterpret_borrow<py::object>(item.first),
                             py::reinterpret_borrow<py::object>(item.second)
                         );
                     }
                 }
                 else if (py::isinstance<PersistentSortedDict>(other)) {
                     const PersistentSortedDict& other_map = other.cast<const PersistentSortedDict&>();
                     py::list items = other_map.items();
                     for (auto item : items) {
                         py::list pair = item.cast<py::list>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 else if (py::isinstance<PersistentDict>(other)) {
                     const PersistentDict& other_map = other.cast<const PersistentDict&>();
                     py::list items = other_map.itemsList();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 else if (py::isinstance<PersistentArrayMap>(other)) {
                     const PersistentArrayMap& other_map = other.cast<const PersistentArrayMap&>();
                     py::list items = other_map.itemsList();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 else if (py::hasattr(other, "items")) {
                     py::object items_method = other.attr("items");
                     py::object items = items_method();
                     for (auto item : items) {
                         py::tuple pair = item.cast<py::tuple>();
                         result = result.assoc(pair[0], pair[1]);
                     }
                 }
                 else {
                     throw py::type_error("Cannot merge PersistentSortedDict with non-mapping type");
                 }

                 return result;
             },
             py::arg("other"),
             "Merge with another mapping (alias for update).\n\n"
             "Args:\n"
             "    other: A mapping to merge with\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict with merged entries")

        .def("clear",
             [](const PersistentSortedDict&) -> PersistentSortedDict {
                 return PersistentSortedDict();
             },
             "Return an empty PersistentSortedDict.\n\n"
             "Returns:\n"
             "    A new empty PersistentSortedDict")

        .def("copy",
             [](const PersistentSortedDict& self) -> PersistentSortedDict {
                 return self;  // Immutable, so copy is just a reference
             },
             "Create a shallow copy of the map.\n\n"
             "Since the map is immutable, this returns self.\n\n"
             "Returns:\n"
             "    The same PersistentSortedDict instance")

        .def("delete",
             &PersistentSortedDict::dissoc,
             py::arg("key"),
             "Remove key (alias for dissoc).\n\n"
             "Args:\n"
             "    key: The key to remove\n\n"
             "Returns:\n"
             "    A new PersistentSortedDict without the key")

        // Factory methods
        .def_static("from_dict", &PersistentSortedDict::fromDict,
                   py::arg("dict"),
                   "Create PersistentSortedDict from dictionary.\n\n"
                   "Args:\n"
                   "    dict: A Python dictionary\n\n"
                   "Returns:\n"
                   "    A new PersistentSortedDict containing all key-value pairs from dict\n\n"
                   "Note: Keys must support < comparison")

        .def_static("create", &PersistentSortedDict::create,
                   "Create PersistentSortedDict from keyword arguments.\n\n"
                   "Example:\n"
                   "    m = PersistentSortedDict.create(a=1, b=2, c=3)\n\n"
                   "Returns:\n"
                   "    A new PersistentSortedDict containing the keyword arguments\n\n"
                   "Note: Keys must support < comparison")

        // Pickle support
        .def(py::pickle(
            [](const PersistentSortedDict &p) { // __getstate__
                return p.items();  // Return list of (key, value) tuples
            },
            [](py::list items) { // __setstate__
                py::dict d;
                for (auto item : items) {
                    py::tuple t = item.cast<py::tuple>();
                    d[t[0]] = t[1];
                }
                return PersistentSortedDict::fromDict(d);
            }
        ));

    // Module-level documentation
    m.attr("__version__") = "2.0.0b3";
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
            >>> from pypersistent import PersistentDict
            >>> m1 = PersistentDict()
            >>> m2 = m1.assoc('name', 'Alice').assoc('age', 30)
            >>> m2.get('name')
            'Alice'
            >>> len(m2)
            2
            >>> m1  # Original unchanged
            PersistentDict({})
            >>> m2
            PersistentDict({'name': 'Alice', 'age': 30})

        Performance characteristics:
        - 5-7x faster than pure Python implementation for insertions
        - 3-5x faster for lookups
        - 4-8x faster for creating variants (structural sharing)
        - Approaches dict performance for basic operations
        - Significantly faster than dict when creating many variants
    )doc";
}
