# README for buffer_pool
`buffer_pool` is a single header only class to manage a memory pool which can be used to efficiently handle buffers for binary data. Target applications are buffers for io-data coming in over over sockets or pipes.

##  What can/should buffer_pool be used for?
Consider a socket which receives arbitrary amounts of incoming data. If the user wants to store the incoming data in a modern C++ fashion and without worrying about memory management of pointers, RAII objects would be used. Since the incoming data can be of arbitrary size, an `std::vector<uint8_t>` of sufficient size to hold the maximum amount of data could be created. The data could be read into the vector and the vector could be shrunk to the actual amount of read data.

```c++
std::vector<uint8_t> data(1024);
auto bytesRead = read(fd, data.data());
data.resize(bytesRead);
```

This concept has several disadvantages:
1. Each time a data-vector is created, a heap allocation is necessary.
2. Even if the data-vector is shrunk to the correct size, the memory is not necessarily freed.
3. Even if the memory is freed, this pattern can lead to heap fragmentation if a lot of data-vectors are created and destroyed.

To avoid point 2, a static intermediate buffer could be used.

```c++
static std::array<uint8_t, 1024> buffer;
auto bytesRead = read(fd, buffer.data());
std::vector<uint8_t> data(buffer.begin(), bytesRead);
```

While this solves the problem of allocating too much memory and therefore reduces memory consumption and fragmentation, it cannot eliminate it completely. Also, it involves an unnecessary copy operation.

To solve all these problems, the buffer_pool was created. It will manage a static pool of memory of which chunks can be requested.
Those chunks can be shrunk. The unused memory will be immediately returned to the pool so that it can be re-used for the next chunk request. The Chunks will merely point to the begin and end of the data.
Therefore, they can easily be passed around.
This is based on the concept of spans from the C++ support library.

To eliminate memory leaks, the memory assigned to a Chunk is freed and returned to the pool when a Chunk goes out of scope.


## Usage

`buffer_pool` does not manage the lifetime of any memory itself. Instead, it uses the `span` type from the [Guideline Support Library](https://isocpp.org/blog/2015/09/bjarne-stroustrup-announces-cpp-core-guidelines) and must be provided with a span-type to use and a span which contains the memory which it should use.

It was tested with [gsl-lite](https://github.com/martinmoene/gsl-lite) which is a single header implementation of the GSL. Alternate implementations providing the interface of span can be provided by the template parameter.

Other than that, it is a single header file which needs to be included. No compilation units/linking.

**Attention: At the moment, buffer_pool is _not_ threadsafe!**

## Example

```c++
#include <gsl.hpp>
#include <buffer_pool.hpp>

uint8_t memory[4096] = {0};
using span_t = gsl::span<uint8_t>;
buffer_pool<span_t> pool(span_t(memory, sizeof(memory)));

buffer_pool<span_t>::Chunk readFromSomehwere()
{
  auto chunk = pool.request(1024);
  // Assuming some sort of file-descriptor esists.
  auto bytesRead = read(fd, chunk.m_chunk.data(), 1024);
  chunk.shrink(bytesRead);
  return chunk;
}
```

For more examples, please refer to the tests.


## Project
The project so far contains of:
* The header file (which is all that is needed for it to work)
* Test implementations using GTest
* Some attempts to perform benchmarking using Google-Benchmark.

All dependencies will be downloaded automatically with the call of CMake.

To run the tests, simply run:

    #> cmake PATH/TO/SOURCE
    #> make
    #> tests/buffer_pool_tests

## Tested with
* Ubuntu 16.04
   * gcc 5.4.0-6ubuntu1~16.04.9
   * clang 3.8
   * clang 5.0

## TODO
* Improve benchmarking
* Create threadsafe version of buffer_pool.
