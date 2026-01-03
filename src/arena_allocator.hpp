#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <new>
#include <stdexcept>

/**
 * BulkOpArena - Fast bump-pointer arena allocator for bulk operations
 *
 * This allocator provides O(1) allocation by using a simple bump pointer
 * within pre-allocated memory chunks. It's designed for temporary use during
 * bulk operations (from_dict, merge) where many small allocations happen.
 *
 * Key Features:
 * - Bump-pointer allocation: ~10-100x faster than malloc
 * - Large chunk pre-allocation: Better cache locality
 * - Automatic cleanup: All memory freed when arena is destroyed
 * - Template-based: Works with any node type
 *
 * Memory Trade-offs:
 * - During construction: +25-30% temporary overhead (arena pre-allocation)
 * - After construction: 0% overhead (arena released, nodes transferred to heap)
 *
 * Usage:
 *   BulkOpArena arena;
 *   NodeBase* node = arena.allocate<BitmapNode>(bitmap, array);
 *   // ... use node ...
 *   // Arena automatically cleans up when destroyed
 */
class BulkOpArena {
private:
    // Memory chunk for bump-pointer allocation
    struct Chunk {
        std::unique_ptr<uint8_t[]> memory;
        size_t size;
        size_t used;

        Chunk(size_t chunk_size)
            : memory(new uint8_t[chunk_size])
            , size(chunk_size)
            , used(0) {}
    };

    static constexpr size_t DEFAULT_CHUNK_SIZE = 1024 * 1024;  // 1MB chunks
    static constexpr size_t ALIGNMENT = alignof(std::max_align_t);  // 16 bytes on most platforms

    std::vector<Chunk> chunks_;
    size_t current_chunk_idx_;

    // Align size to ALIGNMENT boundary
    static size_t alignSize(size_t size) {
        return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    // Allocate a new chunk
    void allocateNewChunk(size_t min_size) {
        size_t chunk_size = std::max(DEFAULT_CHUNK_SIZE, min_size);
        chunks_.emplace_back(chunk_size);
        current_chunk_idx_ = chunks_.size() - 1;
    }

public:
    BulkOpArena() : current_chunk_idx_(0) {
        // Pre-allocate first chunk
        allocateNewChunk(DEFAULT_CHUNK_SIZE);
    }

    // Disable copy and move (arena is not copyable/movable)
    BulkOpArena(const BulkOpArena&) = delete;
    BulkOpArena& operator=(const BulkOpArena&) = delete;
    BulkOpArena(BulkOpArena&&) = delete;
    BulkOpArena& operator=(BulkOpArena&&) = delete;

    /**
     * Allocate and construct an object of type T in the arena
     *
     * Template parameters:
     *   T - Type to allocate (must be a NodeBase subclass)
     *   Args - Constructor argument types
     *
     * Returns:
     *   Pointer to newly constructed object
     *
     * Example:
     *   auto* node = arena.allocate<BitmapNode>(bitmap, array);
     */
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        size_t size = alignSize(sizeof(T));

        // Check if current chunk has space
        Chunk& current = chunks_[current_chunk_idx_];
        if (current.used + size > current.size) {
            // Need a new chunk
            allocateNewChunk(size);
        }

        // Bump-pointer allocation
        Chunk& chunk = chunks_[current_chunk_idx_];
        void* ptr = chunk.memory.get() + chunk.used;
        chunk.used += size;

        // Construct object in-place
        return new (ptr) T(std::forward<Args>(args)...);
    }

    /**
     * Get total bytes allocated by this arena
     */
    size_t totalAllocated() const {
        size_t total = 0;
        for (const auto& chunk : chunks_) {
            total += chunk.used;
        }
        return total;
    }

    /**
     * Get total bytes reserved (including unused space)
     */
    size_t totalReserved() const {
        size_t total = 0;
        for (const auto& chunk : chunks_) {
            total += chunk.size;
        }
        return total;
    }

    /**
     * Get number of chunks allocated
     */
    size_t chunkCount() const {
        return chunks_.size();
    }

    /**
     * Reset the arena (reuse existing chunks)
     *
     * This allows reusing the arena for multiple bulk operations
     * without reallocating memory.
     *
     * WARNING: All previously allocated objects become invalid!
     */
    void reset() {
        for (auto& chunk : chunks_) {
            chunk.used = 0;
        }
        current_chunk_idx_ = 0;
    }

    /**
     * Destructor - automatically frees all chunks
     *
     * Note: Does NOT call destructors on allocated objects!
     * For nodes with reference counting, ensure proper cleanup before
     * destroying the arena.
     */
    ~BulkOpArena() = default;
};
