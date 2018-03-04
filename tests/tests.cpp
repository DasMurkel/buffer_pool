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
}

TEST_F(buffer_pool_test, RequestRelease)
{
    {
        auto c = m_pool.request(10);

        EXPECT_EQ(10, c.m_chunk.size());
        EXPECT_EQ(10, m_pool.used_mem());
    }
    ASSERT_EQ(m_pool.free_mem(), m_span.size());
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
}

TEST_F(buffer_pool_test, RequestReleaseRequestAgain)
{
    auto c1 = std::make_unique<typename pool_t::Chunk>(m_pool.request(10));
    auto c2 = std::make_unique<typename pool_t::Chunk>(m_pool.request(20));

    EXPECT_EQ(20, c2->m_chunk.size());
    EXPECT_EQ(30, m_pool.used_mem());

    c1.reset();

    EXPECT_EQ(20, m_pool.used_mem());

    auto c3 = std::make_unique<typename pool_t::Chunk>(m_pool.request(5));

    EXPECT_EQ(25, m_pool.used_mem());
    EXPECT_EQ(std::begin(m_span), std::begin(c3->m_chunk));
}

TEST_F(buffer_pool_test, Stuff)
{
}
