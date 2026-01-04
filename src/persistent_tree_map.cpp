#include "persistent_tree_map.hpp"
#include <sstream>
#include <algorithm>

// TreeNode implementation

TreeNode::TreeNode(const py::object& k, const py::object& v, Color c)
    : key(k), value(v), left(nullptr), right(nullptr), color(c), refcount_(0) {}

TreeNode::~TreeNode() {
    if (left) left->release();
    if (right) right->release();
}

void TreeNode::addRef() {
    refcount_.fetch_add(1, std::memory_order_relaxed);
}

void TreeNode::release() {
    if (refcount_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete this;
    }
}

TreeNode* TreeNode::clone() const {
    TreeNode* newNode = new TreeNode(key, value, color);
    newNode->left = left;
    newNode->right = right;
    if (left) left->addRef();
    if (right) right->addRef();
    return newNode;
}

// PersistentTreeMap implementation

PersistentTreeMap::PersistentTreeMap()
    : root_(nullptr), count_(0) {}

PersistentTreeMap::PersistentTreeMap(TreeNode* root, size_t count)
    : root_(root), count_(count) {
    // Root comes in with refcount=0 from insert()/remove()
    // Must call addRef() because destructor will call release()
    if (root_) root_->addRef();
}

PersistentTreeMap::PersistentTreeMap(const PersistentTreeMap& other)
    : root_(other.root_), count_(other.count_) {
    if (root_) root_->addRef();
}

PersistentTreeMap::PersistentTreeMap(PersistentTreeMap&& other) noexcept
    : root_(other.root_), count_(other.count_) {
    other.root_ = nullptr;
    other.count_ = 0;
}

PersistentTreeMap::~PersistentTreeMap() {
    if (root_) root_->release();
}

PersistentTreeMap& PersistentTreeMap::operator=(const PersistentTreeMap& other) {
    if (this != &other) {
        if (other.root_) other.root_->addRef();
        if (root_) root_->release();
        root_ = other.root_;
        count_ = other.count_;
    }
    return *this;
}

PersistentTreeMap& PersistentTreeMap::operator=(PersistentTreeMap&& other) noexcept {
    if (this != &other) {
        if (root_) root_->release();
        root_ = other.root_;
        count_ = other.count_;
        other.root_ = nullptr;
        other.count_ = 0;
    }
    return *this;
}

// Key comparison using Python's rich comparison
int PersistentTreeMap::compareKeys(const py::object& k1, const py::object& k2) {
    // First check equality
    int eq = PyObject_RichCompareBool(k1.ptr(), k2.ptr(), Py_EQ);
    if (eq == 1) return 0;
    if (eq == -1) throw py::error_already_set();

    // Then check less than
    int lt = PyObject_RichCompareBool(k1.ptr(), k2.ptr(), Py_LT);
    if (lt == -1) throw py::error_already_set();
    return lt ? -1 : 1;
}

// Core operations

PersistentTreeMap PersistentTreeMap::assoc(const py::object& key, const py::object& val) const {
    bool inserted = false;
    TreeNode* newRoot = insert(root_, key, val, inserted);

    // Ensure root is black
    if (newRoot && newRoot->isRed()) {
        TreeNode* blackRoot = newRoot->clone();
        blackRoot->color = Color::BLACK;
        newRoot->release();
        newRoot = blackRoot;
    }

    size_t newCount = inserted ? count_ + 1 : count_;
    return PersistentTreeMap(newRoot, newCount);
}

TreeNode* PersistentTreeMap::insert(TreeNode* node, const py::object& key, const py::object& val, bool& inserted) const {
    if (node == nullptr) {
        inserted = true;
        TreeNode* newNode = new TreeNode(key, val, Color::RED);
        // Node created with refcount=0, ownership transferred to caller
        return newNode;
    }

    int cmp = compareKeys(key, node->key);
    TreeNode* newNode = node->clone();

    if (cmp < 0) {
        TreeNode* newLeft = insert(node->left, key, val, inserted);
        if (newNode->left) newNode->left->release();
        newNode->left = newLeft;
    } else if (cmp > 0) {
        TreeNode* newRight = insert(node->right, key, val, inserted);
        if (newNode->right) newNode->right->release();
        newNode->right = newRight;
    } else {
        // Key exists, update value
        newNode->value = val;
        inserted = false;
    }

    // Balance the tree
    TreeNode* balanced = balance(newNode);
    if (balanced != newNode) {
        // newNode was replaced by balance(), clean up
        // Don't use release() - newNode has refcount=0 from clone()
        delete newNode;  // Destructor will release children correctly
    }

    return balanced;
}

PersistentTreeMap PersistentTreeMap::dissoc(const py::object& key) const {
    if (!root_) return *this;

    bool removed = false;
    TreeNode* newRoot = remove(root_, key, removed);

    if (!removed) {
        if (newRoot) newRoot->release();
        return *this;
    }

    // Ensure root is black
    if (newRoot && newRoot->isRed()) {
        TreeNode* blackRoot = newRoot->clone();
        blackRoot->color = Color::BLACK;
        newRoot->release();
        newRoot = blackRoot;
    }

    return PersistentTreeMap(newRoot, count_ - 1);
}

TreeNode* PersistentTreeMap::remove(TreeNode* node, const py::object& key, bool& removed) const {
    if (node == nullptr) {
        removed = false;
        return nullptr;
    }

    int cmp = compareKeys(key, node->key);
    TreeNode* newNode = node->clone();

    if (cmp < 0) {
        if (newNode->left) {
            TreeNode* newLeft = remove(newNode->left, key, removed);
            if (newNode->left) newNode->left->release();
            newNode->left = newLeft;
        } else {
            removed = false;
        }
    } else if (cmp > 0) {
        if (newNode->right) {
            TreeNode* newRight = remove(newNode->right, key, removed);
            if (newNode->right) newNode->right->release();
            newNode->right = newRight;
        } else {
            removed = false;
        }
    } else {
        // Found the key to remove
        removed = true;

        if (newNode->right == nullptr) {
            TreeNode* leftChild = newNode->left;
            if (leftChild) leftChild->addRef();
            // Delete newNode - destructor will release the child we just addRef'd
            // Net effect: child refcount stays the same
            delete newNode;
            return leftChild;
        } else if (newNode->left == nullptr) {
            TreeNode* rightChild = newNode->right;
            if (rightChild) rightChild->addRef();
            // Delete newNode - destructor will release the child we just addRef'd
            // Net effect: child refcount stays the same
            delete newNode;
            return rightChild;
        } else {
            // Node has two children - replace with minimum from right subtree
            TreeNode* minNode = findMin(newNode->right);
            newNode->key = minNode->key;
            newNode->value = minNode->value;

            bool dummy = false;
            TreeNode* newRight = remove(newNode->right, minNode->key, dummy);
            if (newNode->right) newNode->right->release();
            newNode->right = newRight;
        }
    }

    return newNode;
}

TreeNode* PersistentTreeMap::removeMin(TreeNode* node) const {
    if (node == nullptr) return nullptr;
    if (node->left == nullptr) {
        TreeNode* right = node->right;
        if (right) right->addRef();
        return right;
    }

    TreeNode* newNode = node->clone();
    TreeNode* newLeft = removeMin(node->left);
    if (newNode->left) newNode->left->release();
    newNode->left = newLeft;

    return newNode;
}

TreeNode* PersistentTreeMap::findMin(TreeNode* node) const {
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

TreeNode* PersistentTreeMap::findMax(TreeNode* node) const {
    while (node && node->right) {
        node = node->right;
    }
    return node;
}

TreeNode* PersistentTreeMap::find(TreeNode* node, const py::object& key) const {
    while (node) {
        int cmp = compareKeys(key, node->key);
        if (cmp < 0) {
            node = node->left;
        } else if (cmp > 0) {
            node = node->right;
        } else {
            return node;
        }
    }
    return nullptr;
}

py::object PersistentTreeMap::get(const py::object& key) const {
    TreeNode* node = find(root_, key);
    if (node) return node->value;
    throw py::key_error(py::str(key).cast<std::string>());
}

py::object PersistentTreeMap::get(const py::object& key, const py::object& default_val) const {
    TreeNode* node = find(root_, key);
    return node ? node->value : default_val;
}

bool PersistentTreeMap::contains(const py::object& key) const {
    return find(root_, key) != nullptr;
}

// Red-black tree balancing operations

TreeNode* PersistentTreeMap::rotateLeft(TreeNode* node) const {
    TreeNode* x = node->right;
    if (!x) return node;

    // Create new nodes for the rotation
    TreeNode* newNode = new TreeNode(node->key, node->value, node->color);
    TreeNode* newX = new TreeNode(x->key, x->value, x->color);

    // Set up the new structure
    // newNode gets node's left and x's left
    newNode->left = node->left;
    if (newNode->left) newNode->left->addRef();
    newNode->right = x->left;
    if (newNode->right) newNode->right->addRef();

    // newX gets newNode as left and x's right
    newX->left = newNode;
    // Don't call addRef() on newNode - we're transferring ownership (refcount already 1)
    newX->right = x->right;
    if (newX->right) newX->right->addRef();

    newX->color = newNode->color;
    newNode->color = Color::RED;

    return newX;
}

TreeNode* PersistentTreeMap::rotateRight(TreeNode* node) const {
    TreeNode* x = node->left;
    if (!x) return node;

    // Create new nodes for the rotation
    TreeNode* newNode = new TreeNode(node->key, node->value, node->color);
    TreeNode* newX = new TreeNode(x->key, x->value, x->color);

    // Set up the new structure
    // newNode gets x's right and node's right
    newNode->left = x->right;
    if (newNode->left) newNode->left->addRef();
    newNode->right = node->right;
    if (newNode->right) newNode->right->addRef();

    // newX gets x's left and newNode as right
    newX->left = x->left;
    if (newX->left) newX->left->addRef();
    newX->right = newNode;
    // Don't call addRef() on newNode - we're transferring ownership (refcount already 1)

    newX->color = newNode->color;
    newNode->color = Color::RED;

    return newX;
}

TreeNode* PersistentTreeMap::flipColors(TreeNode* node) const {
    TreeNode* newNode = node->clone();
    newNode->color = newNode->color == Color::RED ? Color::BLACK : Color::RED;

    if (newNode->left) {
        TreeNode* newLeft = newNode->left->clone();
        newLeft->color = newLeft->color == Color::RED ? Color::BLACK : Color::RED;
        newNode->left->release();
        newNode->left = newLeft;
        // Don't call addRef() - we're transferring ownership (refcount already 1)
    }

    if (newNode->right) {
        TreeNode* newRight = newNode->right->clone();
        newRight->color = newRight->color == Color::RED ? Color::BLACK : Color::RED;
        newNode->right->release();
        newNode->right = newRight;
        // Don't call addRef() - we're transferring ownership (refcount already 1)
    }

    return newNode;
}

TreeNode* PersistentTreeMap::balance(TreeNode* node) const {
    TreeNode* current = node;
    bool currentIsTemp = false;  // Track if current is a temporary we need to clean up

    // Right-leaning red - rotate left
    if (current->right && current->right->isRed() &&
        (!current->left || current->left->isBlack())) {
        TreeNode* newCurrent = rotateLeft(current);
        // rotateLeft returns a new node with refcount 0
        // We don't need to release 'current' here because it's either 'node' (owned by caller)
        // or will be cleaned up at the end if it's a temp
        currentIsTemp = true;
        current = newCurrent;
    }

    // Two reds in a row on left - rotate right
    if (current->left && current->left->isRed() &&
        current->left->left && current->left->left->isRed()) {
        TreeNode* newCurrent = rotateRight(current);
        if (currentIsTemp && current != node) {
            // Current is a temporary from previous rotation with refcount=0
            // Children were addRef'd by rotateRight, so destructor will release them correctly
            delete current;
        }
        currentIsTemp = true;
        current = newCurrent;
    }

    // Both children red - flip colors
    if (current->left && current->left->isRed() &&
        current->right && current->right->isRed()) {
        TreeNode* newCurrent = flipColors(current);
        if (currentIsTemp && current != node) {
            // Current is a temporary from previous rotation with refcount=0
            // Children were addRef'd by flipColors, so destructor will release them correctly
            delete current;
        }
        currentIsTemp = true;
        current = newCurrent;
    }

    return current;
}

TreeNode* PersistentTreeMap::moveRedLeft(TreeNode* node) const {
    TreeNode* newNode = flipColors(node);

    if (newNode->right && newNode->right->left && newNode->right->left->isRed()) {
        TreeNode* newRight = rotateRight(newNode->right);
        newNode->right->release();
        newNode->right = newRight;
        // Don't call addRef() - transferring ownership from rotateRight (refcount=0)

        TreeNode* temp = newNode;
        newNode = rotateLeft(newNode);
        // Clean up temp if it's different from the result
        if (temp != newNode) {
            delete temp;  // Destructor will release children correctly
        }

        temp = newNode;
        newNode = flipColors(newNode);
        // Clean up temp if it's different from the result
        if (temp != newNode) {
            delete temp;  // Destructor will release children correctly
        }
    }

    return newNode;
}

TreeNode* PersistentTreeMap::moveRedRight(TreeNode* node) const {
    TreeNode* newNode = flipColors(node);

    if (newNode->left && newNode->left->left && newNode->left->left->isRed()) {
        TreeNode* temp = newNode;
        newNode = rotateRight(newNode);
        // Clean up temp if it's different from the result
        if (temp != newNode) {
            delete temp;  // Destructor will release children correctly
        }

        temp = newNode;
        newNode = flipColors(newNode);
        // Clean up temp if it's different from the result
        if (temp != newNode) {
            delete temp;  // Destructor will release children correctly
        }
    }

    return newNode;
}

// Ordered operations

py::object PersistentTreeMap::first() const {
    if (!root_) throw std::runtime_error("first() called on empty map");
    TreeNode* min = findMin(root_);
    py::list result;
    result.append(min->key);
    result.append(min->value);
    return result;
}

py::object PersistentTreeMap::last() const {
    if (!root_) throw std::runtime_error("last() called on empty map");
    TreeNode* max = findMax(root_);
    py::list result;
    result.append(max->key);
    result.append(max->value);
    return result;
}

PersistentTreeMap PersistentTreeMap::subseq(const py::object& start, const py::object& end) const {
    std::vector<std::pair<py::object, py::object>> entries;
    collectRange(root_, start, end, entries);

    PersistentTreeMap result;
    for (const auto& entry : entries) {
        result = result.assoc(entry.first, entry.second);
    }
    return result;
}

PersistentTreeMap PersistentTreeMap::rsubseq(const py::object& start, const py::object& end) const {
    std::vector<std::pair<py::object, py::object>> entries;
    collectRangeReverse(root_, start, end, entries);

    PersistentTreeMap result;
    for (const auto& entry : entries) {
        result = result.assoc(entry.first, entry.second);
    }
    return result;
}

void PersistentTreeMap::collectRange(TreeNode* node, const py::object& start, const py::object& end,
                                     std::vector<std::pair<py::object, py::object>>& result) const {
    if (!node) return;

    int cmpStart = compareKeys(node->key, start);
    int cmpEnd = compareKeys(node->key, end);

    if (cmpStart > 0) {
        collectRange(node->left, start, end, result);
    }

    if (cmpStart >= 0 && cmpEnd < 0) {
        result.push_back({node->key, node->value});
    }

    if (cmpEnd < 0) {
        collectRange(node->right, start, end, result);
    }
}

void PersistentTreeMap::collectRangeReverse(TreeNode* node, const py::object& start, const py::object& end,
                                           std::vector<std::pair<py::object, py::object>>& result) const {
    if (!node) return;

    int cmpStart = compareKeys(node->key, start);
    int cmpEnd = compareKeys(node->key, end);

    if (cmpEnd < 0) {
        collectRangeReverse(node->right, start, end, result);
    }

    if (cmpStart >= 0 && cmpEnd < 0) {
        result.push_back({node->key, node->value});
    }

    if (cmpStart > 0) {
        collectRangeReverse(node->left, start, end, result);
    }
}

// Iteration and conversion

TreeMapIterator PersistentTreeMap::iter() const {
    return TreeMapIterator(this);
}

py::list PersistentTreeMap::keysList() const {
    py::list result;
    TreeMapIterator it(this);
    while (it.hasNext()) {
        py::object kv = it.next();
        result.append(kv[py::int_(0)]);
    }
    return result;
}

py::list PersistentTreeMap::valuesList() const {
    py::list result;
    TreeMapIterator it(this);
    while (it.hasNext()) {
        py::object kv = it.next();
        result.append(kv[py::int_(1)]);
    }
    return result;
}

py::list PersistentTreeMap::items() const {
    py::list result;
    TreeMapIterator it(this);
    while (it.hasNext()) {
        result.append(it.next());
    }
    return result;
}

py::dict PersistentTreeMap::dict() const {
    py::dict result;
    TreeMapIterator it(this);
    while (it.hasNext()) {
        py::object kv = it.next();
        result[kv[py::int_(0)]] = kv[py::int_(1)];
    }
    return result;
}

// Equality

bool PersistentTreeMap::operator==(const PersistentTreeMap& other) const {
    if (this == &other) return true;
    if (count_ != other.count_) return false;

    py::list items1 = items();
    py::list items2 = other.items();

    for (size_t i = 0; i < count_; ++i) {
        py::object kv1 = items1[py::int_(i)];
        py::object kv2 = items2[py::int_(i)];

        int keyEq = PyObject_RichCompareBool(kv1[py::int_(0)].ptr(), kv2[py::int_(0)].ptr(), Py_EQ);
        int valEq = PyObject_RichCompareBool(kv1[py::int_(1)].ptr(), kv2[py::int_(1)].ptr(), Py_EQ);

        if (keyEq != 1 || valEq != 1) return false;
    }

    return true;
}

// String representation

std::string PersistentTreeMap::repr() const {
    std::ostringstream oss;
    oss << "PersistentTreeMap({";

    TreeMapIterator it(this);
    size_t i = 0;
    while (it.hasNext()) {
        if (i > 0) oss << ", ";

        py::object kv = it.next();
        py::object key_repr = py::repr(kv[py::int_(0)]);
        py::object val_repr = py::repr(kv[py::int_(1)]);
        oss << key_repr.cast<std::string>() << ": " << val_repr.cast<std::string>();

        if (i >= 10 && count_ > 12) {
            oss << ", ... (" << (count_ - 11) << " more)";
            break;
        }
        i++;
    }

    oss << "})";
    return oss.str();
}

// Factory methods

PersistentTreeMap PersistentTreeMap::fromDict(const py::dict& d) {
    PersistentTreeMap result;
    for (auto item : d) {
        result = result.assoc(
            py::reinterpret_borrow<py::object>(item.first),
            py::reinterpret_borrow<py::object>(item.second)
        );
    }
    return result;
}

PersistentTreeMap PersistentTreeMap::create(const py::kwargs& kwargs) {
    PersistentTreeMap result;
    for (auto item : kwargs) {
        result = result.assoc(
            py::reinterpret_borrow<py::object>(item.first),
            py::reinterpret_borrow<py::object>(item.second)
        );
    }
    return result;
}

// Python protocol support

py::object PersistentTreeMap::pyGetItem(const py::object& key) const {
    return get(key);
}

PersistentTreeMap PersistentTreeMap::pySetItem(const py::object& key, const py::object& val) const {
    return assoc(key, val);
}

bool PersistentTreeMap::pyContains(const py::object& key) const {
    return contains(key);
}

// TreeMapIterator implementation

TreeMapIterator::TreeMapIterator(const PersistentTreeMap* map)
    : map_(map) {
    if (map_->root_) {
        pushLeft(map_->root_);
    }
}

TreeMapIterator::TreeMapIterator(const TreeMapIterator& other)
    : map_(other.map_), stack_(other.stack_) {}

TreeMapIterator::~TreeMapIterator() {}

void TreeMapIterator::pushLeft(TreeNode* node) {
    while (node) {
        stack_.push_back(node);
        node = node->left;
    }
}

bool TreeMapIterator::hasNext() const {
    return !stack_.empty();
}

py::object TreeMapIterator::next() {
    if (stack_.empty()) {
        throw std::runtime_error("Iterator exhausted");
    }

    TreeNode* node = stack_.back();
    stack_.pop_back();

    if (node->right) {
        pushLeft(node->right);
    }

    py::list result;
    result.append(node->key);
    result.append(node->value);
    return result;
}
