
#include <algorithm>
#include <vector>

template <class SPAN>
struct buffer_pool
{
    using span_t = SPAN;
    using pointer_t = typename span_t::pointer;

    const span_t m_memory;


    struct Chunk
    {
        span_t m_chunk;
        buffer_pool *m_pool = nullptr;

        Chunk(pointer_t begin, size_t size, buffer_pool &pool)
            : m_chunk(begin, size), m_pool(&pool)
        {
        }

        Chunk(Chunk &orig) = delete;
        Chunk &operator=(const Chunk &orig) = delete;

        Chunk(Chunk &&orig)
        {
            using std::swap;
            swap(orig.m_chunk, m_chunk);
            swap(orig.m_pool, m_pool);
        }

        Chunk &operator=(Chunk &&orig) = delete;

        ~Chunk()
        {
            if (m_pool != nullptr) m_pool->release(*this);
        }
    };

    friend class Chunk;

    struct mgm_chunk
    {
        pointer_t m_first;
        bool m_inUse;

        mgm_chunk(pointer_t first, bool inUse = true)
            : m_first(first), m_inUse(inUse)
        {
        }
    };

    std::vector<mgm_chunk> m_chunks;

    pointer_t m_last;

    buffer_pool(span_t memory) : m_memory(memory), m_last(std::begin(m_memory))
    {
    }

    buffer_pool(const buffer_pool &orig) = delete;
    buffer_pool &operator=(const buffer_pool &orig) = delete;

    buffer_pool(buffer_pool &&other) = delete;
    buffer_pool &operator=(buffer_pool &&other) = delete;

    ~buffer_pool() = default;

    Chunk request(size_t size)
    {
        assert(size < m_memory.size());
        pointer_t begin = nullptr;

        // Search from the back of the vector to create kind of a
        // fragmented stack and try to keep the reorganizing of the
        // vector to a minimum as opposed to erasing/inserting at the front.
        auto rit = std::find_if(std::rbegin(m_chunks), std::rend(m_chunks), [&](auto c) {
            return !c.m_inUse && this->size(c) >= size;
        });

        if (rit == rend(m_chunks))
        {
            // No chunk of suitable size found - create new one
            begin = m_last;
            m_chunks.emplace_back(begin);
            m_last = begin + size;

            // Check if rest of memory is large enough
            if (std::distance(m_chunks.back().m_first, m_memory.end()) < size)
                throw std::runtime_error("error");
        }
        else
        {
            auto it = --rit.base();
            begin = it->m_first;
            it->m_inUse = true;
            m_chunks.insert(next(it), mgm_chunk(begin + size, false));
        }

        return Chunk(begin, size, *this);
    }

    size_t used_mem() const
    {
        return std::accumulate(begin(m_chunks), end(m_chunks), size_t(0),
                               [this](const auto &a, const auto &b) {
                                   return a + (b.m_inUse ? this->size(b) : 0);
                               });
    }

    size_t free_mem() const { return size() - used_mem(); }

    size_t size() const { return m_memory.size(); }

private:
    size_t size(const mgm_chunk &c) const
    {
        const auto it =
            std::find_if(begin(m_chunks), end(m_chunks),
                         [=](const auto e) { return e.m_first == c.m_first; });

        if (it != end(m_chunks))
        {
            const auto n = next(it);
            return n != end(m_chunks) ? std::distance(c.m_first, n->m_first)
                                      : std::distance(c.m_first, m_last);
        }
        else
            return 0;
    }

    void release(const Chunk &chunk)
    {
        auto it = std::find_if(begin(m_chunks), end(m_chunks),
                               [&](const mgm_chunk &c) {
                                   return c.m_first == chunk.m_chunk.data();
                               });
        if (it != end(m_chunks))
        {
            if (next(it) == end(m_chunks)) m_last = it->m_first;
            it->m_inUse = false;
        }
    }
};
