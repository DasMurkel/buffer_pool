#ifndef TESTS_H
#define TESTS_H

#include <gtest/gtest.h>
#include <gsl.hpp>

#include <buffer_pool.hpp>


class buffer_pool_test : public ::testing::Test
{
   public:
    buffer_pool_test() : m_span(m_memory, sizeof(m_memory)), m_pool(m_span) {}
    using span_t = gsl::span<uint8_t>;

   protected:
    uint8_t m_memory[1024] = {0};
    span_t m_span;

    using pool_t = buffer_pool<span_t>;
    pool_t m_pool;
};

#endif  // TESTS_H
