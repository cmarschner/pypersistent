#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include "persistent_map.hpp"
#include "persistent_array_map.hpp"
#include "persistent_hash_set.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pypersistent, m) {
    m.doc() = "High-performance persistent hash map (HAMT) implementation in C++";

    // Initialize the NOT_FOUND sentinels
    PersistentMap::NOT_FOUND = py::object();
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

        // Fast materialized iteration (returns lists instead of iterators)
        .def("items_list", &PersistentMap::itemsList,
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

        .def("keys_list", &PersistentMap::keysList,
             "Return list of all keys (3-4x faster than keys() for full iteration).\n\n"
             "Returns:\n"
             "    List of all keys in the map")

        .def("values_list", &PersistentMap::valuesList,
             "Return list of all values (3-4x faster than values() for full iteration).\n\n"
             "Returns:\n"
             "    List of all values in the map")

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
             "    other: A dict, PersistentArrayMap, PersistentMap, or mapping\n\n"
             "Returns:\n"
             "    A new PersistentArrayMap with merged entries")

        .def("merge", &PersistentArrayMap::merge,
             py::arg("other"),
             "Alias for update(). Merge mappings.\n\n"
             "Args:\n"
             "    other: A dict, PersistentArrayMap, PersistentMap, or mapping\n\n"
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

    // PersistentHashSet iterator
    py::class_<SetIterator>(m, "SetIterator")
        .def("__iter__", [](SetIterator &it) -> SetIterator& { return it; })
        .def("__next__", &SetIterator::next);

    // PersistentHashSet
    py::class_<PersistentHashSet>(m, "PersistentHashSet")
        .def(py::init<>(),
             "Create an empty PersistentHashSet")

        // Core methods
        .def("conj", &PersistentHashSet::conj,
             py::arg("elem"),
             "Add element to set, returning new set.\n\n"
             "Args:\n"
             "    elem: The element to add (must be hashable)\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with the element added")

        .def("disj", &PersistentHashSet::disj,
             py::arg("elem"),
             "Remove element from set, returning new set.\n\n"
             "Args:\n"
             "    elem: The element to remove\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with the element removed")

        .def("contains", &PersistentHashSet::contains,
             py::arg("elem"),
             "Check if element is in set.\n\n"
             "Args:\n"
             "    elem: The element to check\n\n"
             "Returns:\n"
             "    True if element is present, False otherwise")

        // Set operations
        .def("union", &PersistentHashSet::union_,
             py::arg("other"),
             "Return union of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with all elements from both sets")

        .def("intersection", &PersistentHashSet::intersection,
             py::arg("other"),
             "Return intersection of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with only elements in both sets")

        .def("difference", &PersistentHashSet::difference,
             py::arg("other"),
             "Return difference of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with elements in this set but not in other")

        .def("symmetric_difference", &PersistentHashSet::symmetric_difference,
             py::arg("other"),
             "Return symmetric difference of this set and other.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with elements in either set but not both")

        // Set predicates
        .def("issubset", &PersistentHashSet::issubset,
             py::arg("other"),
             "Test if this set is a subset of other.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    True if all elements of this set are in other")

        .def("issuperset", &PersistentHashSet::issuperset,
             py::arg("other"),
             "Test if this set is a superset of other.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    True if all elements of other are in this set")

        .def("isdisjoint", &PersistentHashSet::isdisjoint,
             py::arg("other"),
             "Test if this set has no elements in common with other.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    True if sets have no common elements")

        // Python-friendly aliases
        .def("add", &PersistentHashSet::add,
             py::arg("elem"),
             "Pythonic alias for conj(). Add element to set.\n\n"
             "Args:\n"
             "    elem: The element to add\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with the element added")

        .def("remove", &PersistentHashSet::remove,
             py::arg("elem"),
             "Pythonic alias for disj(). Remove element from set.\n\n"
             "Args:\n"
             "    elem: The element to remove\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with the element removed")

        .def("update", &PersistentHashSet::update,
             py::arg("other"),
             "Add all elements from another iterable.\n\n"
             "Args:\n"
             "    other: A set, PersistentHashSet, list, or iterable\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with all elements added")

        .def("clear", &PersistentHashSet::clear,
             "Return an empty PersistentHashSet.\n\n"
             "Returns:\n"
             "    An empty PersistentHashSet")

        .def("copy", &PersistentHashSet::copy,
             "Return self (no-op for immutable).\n\n"
             "Returns:\n"
             "    Self")

        // Python protocols
        .def("__contains__", &PersistentHashSet::contains,
             py::arg("elem"),
             "Check if element is in set.\n\n"
             "Args:\n"
             "    elem: The element to check\n\n"
             "Returns:\n"
             "    True if element is present, False otherwise")

        .def("__len__", &PersistentHashSet::size,
             "Return number of elements in the set.")

        .def("__iter__", &PersistentHashSet::iter,
             "Iterate over elements in the set.")

        .def("list", &PersistentHashSet::list,
             "Return list of all elements.\n\n"
             "Returns:\n"
             "    List of all elements in the set")

        // Set operators
        .def("__or__", &PersistentHashSet::union_,
             py::arg("other"),
             "Union using | operator.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with all elements from both sets")

        .def("__and__", &PersistentHashSet::intersection,
             py::arg("other"),
             "Intersection using & operator.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with only elements in both sets")

        .def("__sub__", &PersistentHashSet::difference,
             py::arg("other"),
             "Difference using - operator.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with elements in this set but not in other")

        .def("__xor__", &PersistentHashSet::symmetric_difference,
             py::arg("other"),
             "Symmetric difference using ^ operator.\n\n"
             "Args:\n"
             "    other: Another PersistentHashSet\n\n"
             "Returns:\n"
             "    A new PersistentHashSet with elements in either set but not both")

        .def("__le__", &PersistentHashSet::issubset,
             py::arg("other"),
             "Subset test using <= operator.")

        .def("__ge__", &PersistentHashSet::issuperset,
             py::arg("other"),
             "Superset test using >= operator.")

        .def("__lt__",
             [](const PersistentHashSet& self, const PersistentHashSet& other) -> bool {
                 return self.issubset(other) && self != other;
             },
             py::arg("other"),
             "Proper subset test using < operator.")

        .def("__gt__",
             [](const PersistentHashSet& self, const PersistentHashSet& other) -> bool {
                 return self.issuperset(other) && self != other;
             },
             py::arg("other"),
             "Proper superset test using > operator.")

        .def("__eq__",
             [](const PersistentHashSet& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentHashSet>(other)) {
                     return false;
                 }
                 return self == other.cast<const PersistentHashSet&>();
             },
             py::arg("other"),
             "Check equality with another set.\n\n"
             "Args:\n"
             "    other: Another object to compare with\n\n"
             "Returns:\n"
             "    True if sets are equal, False otherwise")

        .def("__ne__",
             [](const PersistentHashSet& self, py::object other) -> bool {
                 if (!py::isinstance<PersistentHashSet>(other)) {
                     return true;
                 }
                 return self != other.cast<const PersistentHashSet&>();
             },
             py::arg("other"),
             "Check inequality with another set.")

        .def("__repr__", &PersistentHashSet::repr,
             "String representation of the set.")

        // Factory methods
        .def_static("from_set", &PersistentHashSet::fromSet,
                   py::arg("set"),
                   "Create PersistentHashSet from Python set.\n\n"
                   "Args:\n"
                   "    set: A Python set\n\n"
                   "Returns:\n"
                   "    A new PersistentHashSet containing all elements from set")

        .def_static("from_list", &PersistentHashSet::fromList,
                   py::arg("list"),
                   "Create PersistentHashSet from list (duplicates removed).\n\n"
                   "Args:\n"
                   "    list: A Python list\n\n"
                   "Returns:\n"
                   "    A new PersistentHashSet containing unique elements from list")

        .def_static("from_iterable", &PersistentHashSet::fromIterable,
                   py::arg("iterable"),
                   "Create PersistentHashSet from any iterable.\n\n"
                   "Args:\n"
                   "    iterable: Any Python iterable\n\n"
                   "Returns:\n"
                   "    A new PersistentHashSet containing unique elements from iterable")

        .def_static("create", &PersistentHashSet::create,
                   "Create PersistentHashSet from arguments.\n\n"
                   "Example:\n"
                   "    s = PersistentHashSet.create(1, 2, 3)\n\n"
                   "Returns:\n"
                   "    A new PersistentHashSet containing the arguments");

    // Module-level documentation
    m.attr("__version__") = "1.0.1";
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
