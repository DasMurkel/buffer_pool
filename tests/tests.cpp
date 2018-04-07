#include <iostream>
#include <memory>

#include "tests.hpp"

namespace
{
}  // namespace anonymous

TEST_F(buffer_pool_test, Init)
{
    ASSERT_EQ(1024, m_span.size());
    ASSERT_EQ(1024, m_pool.size());
    EXPECT_EQ(0, m_pool.used_chunks());
    EXPECT_EQ(0, m_pool.unused_chunks());
}

TEST_F(buffer_pool_test, RequestRelease)
{
    {
        auto c = m_pool.request(10);

        EXPECT_EQ(10, c.m_chunk.size());
        EXPECT_EQ(10, m_pool.used_mem());
        EXPECT_EQ(1, m_pool.used_chunks());
        EXPECT_EQ(0, m_pool.unused_chunks());
    }
    EXPECT_EQ(m_pool.free_mem(), m_span.size());
    EXPECT_EQ(0, m_pool.used_chunks());
    EXPECT_EQ(0, m_pool.unused_chunks());
}

TEST_F(buffer_pool_test, RequestReleaseTwo)
{
    {
        auto c1 = m_pool.request(10);
        {
            auto c2 = m_pool.request(20);

            EXPECT_EQ(20, c2.m_chunk.size());
            EXPECT_EQ(30, m_pool.used_mem());
        }
        EXPECT_EQ(10, c1.m_chunk.size());
        EXPECT_EQ(10, m_pool.used_mem());
    }
    EXPECT_EQ(0, m_pool.used_chunks());
    EXPECT_EQ(0, m_pool.unused_chunks());
}

TEST_F(buffer_pool_test, ReclaimFreedChunk)
{
    auto c1 = std::make_unique<typename pool_t::Chunk>(m_pool.request(10));
    auto c2 = std::make_unique<typename pool_t::Chunk>(m_pool.request(20));

    EXPECT_EQ(20, c2->m_chunk.size());
    EXPECT_EQ(30, m_pool.used_mem());
    EXPECT_EQ(2, m_pool.used_chunks());

    c1.reset();

    EXPECT_EQ(20, m_pool.used_mem());
    EXPECT_EQ(1, m_pool.used_chunks());

    auto c3 = std::make_unique<typename pool_t::Chunk>(m_pool.request(5));

    EXPECT_EQ(2, m_pool.used_chunks());
    EXPECT_EQ(25, m_pool.used_mem());
    EXPECT_EQ(std::begin(m_span), std::begin(c3->m_chunk));
}

TEST_F(buffer_pool_test, ReleaseAndMergeFollowingFreeChunk)
{
    std::vector<pool_t::Chunk> testChunks(4);

    std::generate(std::begin(testChunks), std::end(testChunks),
                  [&]() { return m_pool.request(10); });

    EXPECT_EQ(40, m_pool.used_mem());
    EXPECT_EQ(4, m_pool.used_chunks());

    testChunks.erase(begin(testChunks) + 2);
    EXPECT_EQ(30, m_pool.used_mem());
    EXPECT_EQ(3, m_pool.used_chunks());
    EXPECT_EQ(1, m_pool.unused_chunks());

    testChunks.erase(begin(testChunks) + 1);
    EXPECT_EQ(20, m_pool.used_mem());
    EXPECT_EQ(2, m_pool.used_chunks());
    EXPECT_EQ(1, m_pool.unused_chunks());

    testChunks.erase(begin(testChunks) + 1);
    EXPECT_EQ(10, m_pool.used_mem());
    EXPECT_EQ(1, m_pool.used_chunks());
    EXPECT_EQ(0, m_pool.unused_chunks());

    testChunks.erase(begin(testChunks) + 0);
    EXPECT_EQ(0, m_pool.used_mem());
    EXPECT_EQ(0, m_pool.used_chunks());
    EXPECT_EQ(0, m_pool.unused_chunks());
}

TEST_F(buffer_pool_test, ReleaseAndMergePreceedingFreeChunk)
{
    std::vector<pool_t::Chunk> testChunks(4);

    std::generate(std::begin(testChunks), std::end(testChunks),
                  [&]() { return m_pool.request(10); });

    EXPECT_EQ(40, m_pool.used_mem());
    EXPECT_EQ(4, m_pool.used_chunks());

    testChunks.erase(begin(testChunks) + 1);
    EXPECT_EQ(30, m_pool.used_mem());
    EXPECT_EQ(3, m_pool.used_chunks());
    EXPECT_EQ(1, m_pool.unused_chunks());

    testChunks.erase(begin(testChunks) + 1);
    EXPECT_EQ(20, m_pool.used_mem());
    EXPECT_EQ(2, m_pool.used_chunks());
    EXPECT_EQ(1, m_pool.unused_chunks());

    testChunks.erase(begin(testChunks) + 0);
    EXPECT_EQ(10, m_pool.used_mem());
    EXPECT_EQ(1, m_pool.used_chunks());
    EXPECT_EQ(1, m_pool.unused_chunks());

    testChunks.erase(begin(testChunks) + 0);
    EXPECT_EQ(0, m_pool.used_mem());
    EXPECT_EQ(0, m_pool.used_chunks());
    EXPECT_EQ(0, m_pool.unused_chunks());
}

TEST_F(buffer_pool_test, ShrinkChunkAndReclaimCompleteFreeMem)
{
    auto c1 = m_pool.request(20);
    auto c2 = m_pool.request(20);

    c1.shrink(10);

    EXPECT_EQ(2, m_pool.used_chunks());
    EXPECT_EQ(30, m_pool.used_mem());

    auto c3 = m_pool.request(10);

    EXPECT_EQ(3, m_pool.used_chunks());
    EXPECT_EQ(40, m_pool.used_mem());

    EXPECT_EQ(std::end(c1.m_chunk), std::begin(c3.m_chunk));
}

TEST_F(buffer_pool_test, ShrinkChunkAndReclaimPartialFreeMem)
{
    auto c1 = m_pool.request(20);
    auto c2 = m_pool.request(20);

    c1.shrink(10);

    EXPECT_EQ(2, m_pool.used_chunks());
    EXPECT_EQ(1, m_pool.unused_chunks());
    EXPECT_EQ(30, m_pool.used_mem());

    auto c3 = m_pool.request(5);

    EXPECT_EQ(3, m_pool.used_chunks());
    EXPECT_EQ(1, m_pool.unused_chunks());
    EXPECT_EQ(35, m_pool.used_mem());

    EXPECT_EQ(std::end(c1.m_chunk), std::begin(c3.m_chunk));

    auto c4 = m_pool.request(5);

    EXPECT_EQ(4, m_pool.used_chunks());
    EXPECT_EQ(0, m_pool.unused_chunks());
    EXPECT_EQ(40, m_pool.used_mem());

    EXPECT_EQ(std::end(c3.m_chunk), std::begin(c4.m_chunk));
}

TEST_F(buffer_pool_test, ShrinkLastChunkAndRelocateEndOfChunks)
{
    auto c1 = m_pool.request(10);
    auto c2 = m_pool.request(20);

    c2.shrink(5);

    auto c3 = m_pool.request(10);

    EXPECT_EQ(std::end(c2.m_chunk), std::begin(c3.m_chunk));
}
