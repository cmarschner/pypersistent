#ifndef PERSISTENT_TREE_MAP_HPP
#define PERSISTENT_TREE_MAP_HPP

#include <pybind11/pybind11.h>
#include <memory>
#include <vector>
#include <atomic>
#include <stdexcept>

namespace py = pybind11;

// Forward declarations
class TreeNode;
class PersistentTreeMap;
class TreeMapIterator;

// Color for red-black tree nodes
enum class Color { RED, BLACK };

// TreeNode - Red-black tree node with intrusive reference counting
class TreeNode {
public:
    py::object key;
    py::object value;
    TreeNode* left;
    TreeNode* right;
    Color color;

    TreeNode(const py::object& k, const py::object& v, Color c = Color::RED);
    ~TreeNode();

    void addRef();
    void release();
    TreeNode* clone() const;

    // Helper methods
    bool isRed() const { return color == Color::RED; }
    bool isBlack() const { return color == Color::BLACK; }

private:
    std::atomic<int> refcount_;
};

// PersistentTreeMap - Immutable sorted map using red-black tree
class PersistentTreeMap {
    friend class TreeMapIterator;

public:
    // Constructors
    PersistentTreeMap();
    PersistentTreeMap(TreeNode* root, size_t count);
    PersistentTreeMap(const PersistentTreeMap& other);
    PersistentTreeMap(PersistentTreeMap&& other) noexcept;
    ~PersistentTreeMap();

    // Assignment
    PersistentTreeMap& operator=(const PersistentTreeMap& other);
    PersistentTreeMap& operator=(PersistentTreeMap&& other) noexcept;

    // Core operations (functional API)
    PersistentTreeMap assoc(const py::object& key, const py::object& val) const;
    PersistentTreeMap dissoc(const py::object& key) const;
    py::object get(const py::object& key) const;
    py::object get(const py::object& key, const py::object& default_val) const;
    bool contains(const py::object& key) const;

    // Ordered operations
    py::object first() const;  // Returns [key, value] of smallest key
    py::object last() const;   // Returns [key, value] of largest key
    PersistentTreeMap subseq(const py::object& start, const py::object& end) const;
    PersistentTreeMap rsubseq(const py::object& start, const py::object& end) const;

    // Size and iteration
    size_t size() const { return count_; }
    TreeMapIterator iter() const;
    py::list keysList() const;
    py::list valuesList() const;
    py::list items() const;

    // Conversion
    py::dict dict() const;

    // Equality
    bool operator==(const PersistentTreeMap& other) const;
    bool operator!=(const PersistentTreeMap& other) const { return !(*this == other); }

    // String representation
    std::string repr() const;

    // Factory methods
    static PersistentTreeMap fromDict(const py::dict& d);
    static PersistentTreeMap create(const py::kwargs& kwargs);

    // Python protocol support
    py::object pyGetItem(const py::object& key) const;
    PersistentTreeMap pySetItem(const py::object& key, const py::object& val) const;
    bool pyContains(const py::object& key) const;

private:
    TreeNode* root_;
    size_t count_;

    // Helper methods for tree operations
    TreeNode* insert(TreeNode* node, const py::object& key, const py::object& val, bool& inserted) const;
    TreeNode* remove(TreeNode* node, const py::object& key, bool& removed) const;
    TreeNode* removeMin(TreeNode* node) const;
    TreeNode* findMin(TreeNode* node) const;
    TreeNode* findMax(TreeNode* node) const;
    TreeNode* find(TreeNode* node, const py::object& key) const;

    // Red-black tree balancing operations
    TreeNode* rotateLeft(TreeNode* node) const;
    TreeNode* rotateRight(TreeNode* node) const;
    TreeNode* flipColors(TreeNode* node) const;
    TreeNode* moveRedLeft(TreeNode* node) const;
    TreeNode* moveRedRight(TreeNode* node) const;
    TreeNode* balance(TreeNode* node) const;

    // Comparison helper
    static int compareKeys(const py::object& k1, const py::object& k2);

    // Range query helpers
    void collectRange(TreeNode* node, const py::object& start, const py::object& end,
                     std::vector<std::pair<py::object, py::object>>& result) const;
    void collectRangeReverse(TreeNode* node, const py::object& start, const py::object& end,
                            std::vector<std::pair<py::object, py::object>>& result) const;
};

// TreeMapIterator - Iterator for ordered traversal
class TreeMapIterator {
public:
    explicit TreeMapIterator(const PersistentTreeMap* map);
    TreeMapIterator(const TreeMapIterator& other);
    ~TreeMapIterator();

    bool hasNext() const;
    py::object next();

private:
    const PersistentTreeMap* map_;
    std::vector<TreeNode*> stack_;

    void pushLeft(TreeNode* node);
};

// Pybind11 iterator wrapper
class TreeMapIteratorWrapper {
public:
    explicit TreeMapIteratorWrapper(TreeMapIterator&& it) : it_(std::move(it)) {}

    TreeMapIteratorWrapper& iter() { return *this; }

    py::object next() {
        if (!it_.hasNext()) {
            throw py::stop_iteration();
        }
        return it_.next();
    }

private:
    TreeMapIterator it_;
};

#endif // PERSISTENT_TREE_MAP_HPP
