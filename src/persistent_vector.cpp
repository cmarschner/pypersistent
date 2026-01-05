#include "persistent_vector.hpp"
#include <sstream>
#include <stdexcept>

// VectorNode implementation

VectorNode::VectorNode() : refcount_(0) {}

VectorNode::VectorNode(size_t size) : refcount_(0) {
    array_.reserve(size);
}

VectorNode::~VectorNode() {
    // Release all child nodes (py::object handles itself)
    for (auto& elem : array_) {
        if (std::holds_alternative<VectorNode*>(elem)) {
            VectorNode* child = std::get<VectorNode*>(elem);
            if (child) child->release();
        }
    }
}

VectorNode* VectorNode::clone() const {
    VectorNode* newNode = new VectorNode(array_.size());
    for (const auto& elem : array_) {
        if (std::holds_alternative<py::object>(elem)) {
            newNode->array_.push_back(std::get<py::object>(elem));
        } else {
            VectorNode* child = std::get<VectorNode*>(elem);
            if (child) child->addRef();
            newNode->array_.push_back(child);
        }
    }
    return newNode;
}

// PersistentVector implementation

PersistentVector::PersistentVector()
    : root_(nullptr)
    , tail_(std::make_shared<std::vector<py::object>>())
    , count_(0)
    , shift_(BITS) {}

PersistentVector::PersistentVector(VectorNode* root,
                                   std::shared_ptr<std::vector<py::object>> tail,
                                   size_t count, uint32_t shift)
    : root_(root), tail_(tail), count_(count), shift_(shift) {
    if (root_) root_->addRef();
}

PersistentVector::PersistentVector(const PersistentVector& other)
    : root_(other.root_), tail_(other.tail_), count_(other.count_), shift_(other.shift_) {
    if (root_) root_->addRef();
}

PersistentVector::PersistentVector(PersistentVector&& other) noexcept
    : root_(other.root_), tail_(std::move(other.tail_)),
      count_(other.count_), shift_(other.shift_) {
    other.root_ = nullptr;
    other.count_ = 0;
}

PersistentVector::~PersistentVector() {
    if (root_) root_->release();
}

PersistentVector& PersistentVector::operator=(const PersistentVector& other) {
    if (this != &other) {
        if (other.root_) other.root_->addRef();
        if (root_) root_->release();
        root_ = other.root_;
        tail_ = other.tail_;
        count_ = other.count_;
        shift_ = other.shift_;
    }
    return *this;
}

PersistentVector& PersistentVector::operator=(PersistentVector&& other) noexcept {
    if (this != &other) {
        if (root_) root_->release();
        root_ = other.root_;
        tail_ = std::move(other.tail_);
        count_ = other.count_;
        shift_ = other.shift_;
        other.root_ = nullptr;
        other.count_ = 0;
    }
    return *this;
}

// Core operations

PersistentVector PersistentVector::conj(const py::object& val) const {
    // Fast path: append to tail if there's room
    if (tail_->size() < NODE_SIZE) {
        auto newTail = std::make_shared<std::vector<py::object>>(*tail_);
        newTail->push_back(val);
        return PersistentVector(root_, newTail, count_ + 1, shift_);
    }

    // Tail is full, need to push it to tree
    VectorNode* tailNode = new VectorNode(NODE_SIZE);
    for (const auto& elem : *tail_) {
        tailNode->push(elem);
    }

    // Check if we need to expand the tree height
    size_t newCount = count_ + 1;
    if ((count_ >> BITS) > (1UL << shift_)) {
        // Tree is full at current height, need to add a level
        VectorNode* newRoot = new VectorNode(2);
        if (root_) {
            root_->addRef();
            newRoot->push(root_);
        }
        VectorNode* rightPath = newPath(shift_, tailNode);
        rightPath->addRef();
        newRoot->push(rightPath);

        auto newTail = std::make_shared<std::vector<py::object>>();
        newTail->push_back(val);
        return PersistentVector(newRoot, newTail, newCount, shift_ + BITS);
    }

    // Push tail into existing tree
    VectorNode* newRoot = pushTail(root_, shift_, tailNode);
    auto newTail = std::make_shared<std::vector<py::object>>();
    newTail->push_back(val);
    return PersistentVector(newRoot, newTail, newCount, shift_);
}

VectorNode* PersistentVector::pushTail(VectorNode* node, uint32_t level, VectorNode* tailNode) const {
    // Base case: at level 0, return the tail node (it's a leaf)
    if (level == 0) {
        return tailNode;
    }

    VectorNode* newNode;
    size_t subidx = ((count_ - 1) >> level) & MASK;

    if (node == nullptr) {
        // Creating new path
        newNode = new VectorNode();
        VectorNode* child = pushTail(nullptr, level - BITS, tailNode);
        child->addRef();
        newNode->push(child);
        return newNode;
    }

    newNode = node->clone();

    if (subidx < newNode->arraySize()) {
        // Recurse into existing child
        VectorNode* child = std::get<VectorNode*>(newNode->get(subidx));
        VectorNode* newChild = pushTail(child, level - BITS, tailNode);

        // Release old child and set new one
        if (child) child->release();
        newChild->addRef();
        newNode->set(subidx, newChild);
    } else {
        // Add new child
        VectorNode* newChild = pushTail(nullptr, level - BITS, tailNode);
        newChild->addRef();
        newNode->push(newChild);
    }

    return newNode;
}

VectorNode* PersistentVector::newPath(uint32_t level, VectorNode* node) const {
    if (level == 0) {
        return node;
    }
    VectorNode* newNode = new VectorNode(1);
    VectorNode* child = newPath(level - BITS, node);
    child->addRef();
    newNode->push(child);
    return newNode;
}

py::object PersistentVector::nth(size_t idx) const {
    if (idx >= count_) {
        throw std::out_of_range("Index out of range");
    }

    // Check if in tail
    if (idx >= tailOffset()) {
        return (*tail_)[idx - tailOffset()];
    }

    // Traverse tree
    return getFromTree(idx);
}

py::object PersistentVector::getFromTree(size_t idx) const {
    VectorNode* node = root_;
    uint32_t level = shift_;

    // Descend through internal nodes until we reach a leaf
    while (level > 0) {
        size_t subidx = (idx >> level) & MASK;
        node = std::get<VectorNode*>(node->get(subidx));
        level -= BITS;
    }

    // Now at leaf level, get the py::object
    return std::get<py::object>(node->get(idx & MASK));
}

py::object PersistentVector::get(size_t idx, const py::object& default_val) const {
    if (idx >= count_) {
        return default_val;
    }
    return nth(idx);
}

PersistentVector PersistentVector::assoc(size_t idx, const py::object& val) const {
    if (idx >= count_) {
        throw std::out_of_range("Index out of range");
    }

    // Check if in tail
    if (idx >= tailOffset()) {
        size_t tailIdx = idx - tailOffset();
        if ((*tail_)[tailIdx].is(val)) {
            return *this;  // No change
        }

        auto newTail = std::make_shared<std::vector<py::object>>(*tail_);
        (*newTail)[tailIdx] = val;
        return PersistentVector(root_, newTail, count_, shift_);
    }

    // In tree - path copying
    VectorNode* newRoot = assocInTree(root_, shift_, idx, val);
    return PersistentVector(newRoot, tail_, count_, shift_);
}

VectorNode* PersistentVector::assocInTree(VectorNode* node, uint32_t level, size_t idx, const py::object& val) const {
    VectorNode* newNode = node->clone();

    if (level == 0) {
        // Leaf level - node contains py::objects
        newNode->set(idx & MASK, val);
    } else {
        // Internal node - node contains VectorNode* children
        size_t subidx = (idx >> level) & MASK;
        // IMPORTANT: Get child from newNode (the clone), not the original node
        // The clone's children were addRef'd, so we need to release from the clone
        VectorNode* child = std::get<VectorNode*>(newNode->get(subidx));
        VectorNode* newChild = assocInTree(child, level - BITS, idx, val);

        child->release();
        newChild->addRef();
        newNode->set(subidx, newChild);
    }

    return newNode;
}

PersistentVector PersistentVector::pop() const {
    if (count_ == 0) {
        throw std::runtime_error("Can't pop empty vector");
    }

    if (count_ == 1) {
        return PersistentVector();
    }

    // If tail has more than one element, just remove the last
    if (tail_->size() > 1) {
        auto newTail = std::make_shared<std::vector<py::object>>(tail_->begin(), tail_->end() - 1);
        return PersistentVector(root_, newTail, count_ - 1, shift_);
    }

    // Need to pull from tree
    // For simplicity, rebuild the vector without the last element
    // (A full implementation would handle tree trimming)
    py::list items = list();
    items = items[py::slice(0, py::int_(count_ - 1), py::int_(1))];
    return fromList(items);
}

// Iteration and conversion

VectorIterator PersistentVector::iter() const {
    return VectorIterator(this);
}

py::list PersistentVector::list() const {
    py::list result;
    for (size_t i = 0; i < count_; ++i) {
        result.append(nth(i));
    }
    return result;
}

// Slicing

PersistentVector PersistentVector::slice(ssize_t start, ssize_t stop) const {
    // Handle negative indices
    if (start < 0) start += count_;
    if (stop < 0) stop += count_;

    // Clamp to valid range
    if (start < 0) start = 0;
    if (stop > static_cast<ssize_t>(count_)) stop = count_;
    if (start >= stop) return PersistentVector();

    // Build new vector from slice
    PersistentVector result;
    for (ssize_t i = start; i < stop; ++i) {
        result = result.conj(nth(i));
    }
    return result;
}

// Equality

bool PersistentVector::operator==(const PersistentVector& other) const {
    if (this == &other) return true;
    if (count_ != other.count_) return false;

    for (size_t i = 0; i < count_; ++i) {
        py::object v1 = nth(i);
        py::object v2 = other.nth(i);
        int eq = PyObject_RichCompareBool(v1.ptr(), v2.ptr(), Py_EQ);
        if (eq != 1) return false;
    }
    return true;
}

// String representation

std::string PersistentVector::repr() const {
    std::ostringstream oss;
    oss << "PersistentVector([";

    for (size_t i = 0; i < count_; ++i) {
        if (i > 0) oss << ", ";
        py::object elem_repr = py::repr(nth(i));
        oss << elem_repr.cast<std::string>();

        // Limit output for large vectors
        if (i >= 10 && count_ > 12) {
            oss << ", ... (" << (count_ - 11) << " more)";
            i = count_ - 2;  // Show last element
        }
    }

    oss << "])";
    return oss.str();
}

// Factory methods

PersistentVector PersistentVector::fromList(const py::list& l) {
    PersistentVector result;
    for (auto elem : l) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}

PersistentVector PersistentVector::fromIterable(const py::object& iterable) {
    PersistentVector result;
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

PersistentVector PersistentVector::create(const py::args& args) {
    PersistentVector result;
    for (auto elem : args) {
        result = result.conj(py::reinterpret_borrow<py::object>(elem));
    }
    return result;
}
