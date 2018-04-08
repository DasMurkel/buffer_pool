
#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>

/**
 * A buffer_pool is a management entity for a range of memory.
 *
 * Consider a socket which receives arbitrary amounts of incoming data.
 * If the user wants to store the incoming data in a modern C++ fashion
 * and without worrying about memory management of pointers RAII objects
 * would be used. Since the incoming data can be or arbitrary size, an
 * std::vector<uint8_t> of sufficient size to hold the maximum amount of data
 * could be created. The data could be read into the vector und the vector
 * could be shrunk to the actual amount of read data.
 *
 *     std::vector<uint8_t> data(1024);
 *     auto bytesRead = read(fd, data.data());
 *     data.resize(bytesRead);
 *
 * This concept has several disadvantages:
 * 1. Each time a data-vector is created, a heap allocation is necessary.
 * 2. Even if the data-vector is shrunk to the correct size, the memory is not
 * necessarily freed.
 * 3. Even if the memory is freed, this pattern can lead to heap fragmentation
 * if a lot of data-vectors are created and destroyed.
 *
 * To avoid point 2, a static intermediate buffer could be used.
 *
 *     static std::array<uint8_t, 1024> buffer;
 *     auto bytesRead = read(fd, buffer.data());
 *     std::vector<uint8_t> data(buffer.begin(), bytesRead);
 *
 * While this solves the problem of allocating too much memory and therefore
 * reduces memory consumption and fragmentation, it cannot eliminate it
 * completely. Also, it involves an unnecessary copy operation.
 *
 *
 * To solve all these problems, the buffer_pool was created. It will manage
 * a static pool of memory of which chunks can be requested.
 * Those chunks can be shrunk. The unused memory will be immediately returned
 * to the pool so that it can be re-used for the next chunk request.
 * The Chunks will merely point to the begin and end of the data.
 * Therefore, they can easily be passed around.
 * This is based on the concept of spans from the C++ support library.
 *
 * To eliminate memory leaks, the memory assigned to a chunk is freed and
 * returned to the pool when a Chunk goes out of scope.
 *
 *     uint8_t memory[4096] = {0};
 *     using span_t = gsl::span<uint8_t>;
 *     buffer_pool<span_t> pool(span_t(memory, sizeof(memory)));
 *     auto chunk = pool.request(1024);
 *     auto bytesRead = read(fd, chunk.m_chunk.data());
 *     chunk.shrink(bytesRead);
 *
 */
template <class SPAN>
class buffer_pool
{
public:
    using span_t = SPAN;
    using pointer_t = typename span_t::pointer;

private:
    const span_t m_memory;

    /**
     * @brief The mgm_chunk struct is used internally by the buffer_pool to
     * manage the memory.
     */
    struct mgm_chunk
    {
        pointer_t m_first;  // Points to the first address in the mgm_chunk.
        bool m_inUse;       // Is the mgm_chunk in use by a Chunk?

        mgm_chunk(pointer_t first, bool inUse = true)
            : m_first(first), m_inUse(inUse)
        {
        }
    };

    using chunkVec_t = std::vector<mgm_chunk>;
    chunkVec_t m_chunks;

    pointer_t m_last;  // The first unused address in the managed memory.

public:
    /**
     * @brief The Chunk struct is a chunk of memory inside the buffer_pool and
     * managed by the buffer bool.
     *
     * Chunks can be invalid in which case they have size 0 and do not point
     * to any buffer_pool they would be managed by.
     *
     * The lifetime of buffer_pool *must* exceed the lifetime of all Chunks
     * it manages.
     */
    struct Chunk
    {
        // A span which points to the memory that can be used by the Chunk.
        span_t m_chunk;

        // A pointer to the buffer_pool which managed this Chunk.
        buffer_pool* m_pool = nullptr;

        Chunk() = default;

        Chunk(pointer_t begin, size_t size, buffer_pool& pool)
            : m_chunk(begin, size), m_pool(&pool)
        {
        }

        // Chunks can not be copied. If this is needed, move the Chunk into a
        // shared_ptr.
        Chunk(Chunk& orig) = delete;
        Chunk& operator=(const Chunk& orig) = delete;

        // Chunks can be moved.
        Chunk(Chunk&& orig)
        {
            using std::swap;
            swap(orig.m_chunk, m_chunk);
            swap(orig.m_pool, m_pool);
        }

        Chunk& operator=(Chunk&& orig)
        {
            using std::swap;
            swap(orig.m_chunk, m_chunk);
            swap(orig.m_pool, m_pool);
            return *this;
        }

        ~Chunk() { release(); }

        /**
         * @brief shrink Shrink the Chunk.
         * @param newSize The target size of the Chunk. Must be smaller than
         * or equal to m_chunk.size().
         */
        void shrink(const size_t newSize)
        {
            assert(newSize <= m_chunk.size());
            if (newSize != m_chunk.size())
            {
                m_chunk = span_t(m_chunk.begin(), newSize);
                m_pool->resize(*this);
            }
        }

        /**
         * @brief release Releases the memory managed by the Chunk. The chunk
         * becomes invalid after being released.
         */
        void release()
        {
            if (m_pool != nullptr) m_pool->release(*this);
            m_chunk = span_t();
        }

        /**
         * @brief valid Test if the Chunk is valid and points to memory managed
         * by a buffer_pool.
         * @return True if it is valid, false otherwise.
         */
        bool valid() const { return m_pool != nullptr; }
    };

    // Chunk needs to call some private methods on buffer_bool, nobody else
    // should call, so make it a friend.
    friend struct Chunk;

    buffer_pool(span_t memory) : m_memory(memory), m_last(std::begin(m_memory))
    {
    }

    // Buffer_pools cannot be copied ...
    buffer_pool(const buffer_pool& orig) = delete;
    buffer_pool& operator=(const buffer_pool& orig) = delete;

    // ... or moved.
    buffer_pool(buffer_pool&& other) = delete;
    buffer_pool& operator=(buffer_pool&& other) = delete;

    /// All Chunks managed by this buffer_pool *must* have been
    /// released/destroyed before destroying the pool!
    ~buffer_pool() = default;

    /**
     * @brief request Creates a new Chunk managed by this buffer_pool.
     * @param size The size of the requested Chunk. Must be smaller than the
     * size of the buffer_pool. If no Chunk of a suitable size can be found,
     * an exception is thrown.
     * @return A Chunk which manages the memory of the requested size.
     *
     */
    Chunk request(size_t size)
    {
        assert(size < m_memory.size());
        pointer_t begin = nullptr;

        // Search from the back of the vector to create kind of a
        // fragmented stack and try to keep the reorganizing of the
        // vector to a minimum as opposed to erasing/inserting at the front.
        auto rit = std::find_if(
            std::rbegin(m_chunks), std::rend(m_chunks),
            [&](auto c) { return !c.m_inUse && this->size(c) >= size; });

        if (rit == rend(m_chunks))
        {
            // Check if rest of memory is large enough
            const auto rest = std::distance(m_last, m_memory.end());
            assert(rest >= 0);
            if (static_cast<size_t>(rest) < size)
                throw std::overflow_error("error");

            // No chunk of suitable size found - create new one
            begin = m_last;
            m_chunks.emplace_back(begin);
            m_last = begin + size;
        }
        else
        {
            // Re-use existing chunk.
            auto it = --rit.base();
            begin = it->m_first;
            it->m_inUse = true;

            // If the new chunk doesn't fix exactly, we need to create a new one
            // for the memory that is left to the beginning of the next Chunk.
            if (this->size(*it) != size)
                m_chunks.insert(next(it), mgm_chunk(begin + size, false));
        }

        return Chunk(begin, size, *this);
    }

    /**
     * @brief used_mem Calculates the amount of used memory in the buffer_pool.
     * @return The amount of memory used in Chunks.
     */
    size_t used_mem() const
    {
        return std::accumulate(begin(m_chunks), end(m_chunks), size_t(0),
                               [this](const auto& a, const auto& b) {
                                   return a + (b.m_inUse ? this->size(b) : 0);
                               });
    }

    /**
     * @brief free_mem Calculates the remaining free memory in the buffer_pool.
     * @return The amount of free memory in the buffer_pool. Not continuous.
     */
    size_t free_mem() const { return size() - used_mem(); }

    /**
     * @brief size The size of the memory assigned to the buffer_bool.
     * @return The size in bytes.
     */
    size_t size() const { return m_memory.size(); }

    /**
     * @brief num_chunks Used for testing and statistical purposes.
     * @return The number of mgm_chunks managed by the buffer_pool.
     */
    size_t num_chunks() const { return m_chunks.size(); }

    /**
     * @brief used_chunks Calculates the number of assigned mgm_chunks.
     * @return The number of used mgm_chunks.
     * Must be equal to the number of active Chunks.
     */
    size_t used_chunks()
    {
        return std::count_if(begin(m_chunks), end(m_chunks),
                             [](const auto& c) { return c.m_inUse; });
    }

    /**
     * @brief unused_chunks Calculates the number of unused mgm_chunks.
     * Can be used for tests and for measuring fragmentation.
     * @return The number of unused mgm_chunks.
     */
    size_t unused_chunks()
    {
        return std::count_if(begin(m_chunks), end(m_chunks),
                             [](const auto& c) { return !c.m_inUse; });
    }

private:
    size_t size(const mgm_chunk& c) const
    {
        const auto it =
            std::find_if(begin(m_chunks), end(m_chunks),
                         [=](const auto e) { return e.m_first == c.m_first; });

        assert(it != end(m_chunks));

        const auto n = next(it);
        return n != end(m_chunks) ? std::distance(c.m_first, n->m_first)
                                  : std::distance(c.m_first, m_last);
    }

    typename chunkVec_t::iterator find_chunk(const Chunk& chunk)
    {
        return std::find_if(begin(m_chunks), end(m_chunks),
                            [&](const mgm_chunk& c) {
                                return c.m_first == chunk.m_chunk.data();
                            });
    }

    void release(const Chunk& chunk)
    {
        auto it = find_chunk(chunk);
        assert(it != end(m_chunks));

        // First, invalidate!
        it->m_inUse = false;

        // Then, see if we can merge it with a previous mgm_chunk.
        if (it != begin(m_chunks) && !prev(it)->m_inUse)
            it = prev(m_chunks.erase(it));

        const auto nextIt = next(it);
        if (nextIt == end(m_chunks))
        {
            // If it's the last mgm_chunk, we can simply delete it and set the
            // m_last pointer to its beginning.
            m_last = it->m_first;
            m_chunks.erase(it);
        }
        else
        {
            // If the next mgm_chunk is not used, we can merge those two.
            if (!nextIt->m_inUse) m_chunks.erase(nextIt);
        }
    }

    void resize(const Chunk& chunk)
    {
        const auto it = find_chunk(chunk);
        assert(it != end(m_chunks));

        const auto nextIt = next(it);
        if (nextIt != end(m_chunks))
        {
            // Either insert a new unused mgm_chunk or extend the adjacent one.
            if (nextIt->m_inUse)
                m_chunks.insert(nextIt, mgm_chunk(chunk.m_chunk.end(), false));
            else
                nextIt->m_first = chunk.m_chunk.end();
        }
        else
        {
            // If this was the last mgm_chunk, we need to relocate m_last.
            m_last = chunk.m_chunk.end();
        }
    }
};
