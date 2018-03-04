# Download and unpack googlebench at configure time
configure_file(CMakeLists.txt.in googlebench-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googlebench-download )
if(result)
  message(FATAL_ERROR "CMake step for googlebench failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googlebench-download )
if(result)
  message(FATAL_ERROR "Build step for googlebench failed: ${result}")
endif()

# Add googlebench directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googlebench-src
                 ${CMAKE_BINARY_DIR}/googlebench-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(buffer_pool_bench bench.cpp)
target_link_libraries(buffer_pool_bench benchmark)
