cmake_minimum_required(VERSION 3.16)
project(BPlusTree10M VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bptree_core
    src/csv_reader.cpp
    src/memory_bptree.cpp
    src/paged_bptree.cpp
    src/benchmark.cpp
)

target_include_directories(bptree_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(bptree_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O2>
)

add_executable(bptree_benchmark src/main.cpp)
target_link_libraries(bptree_benchmark PRIVATE bptree_core)

target_compile_options(bptree_benchmark PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O2>
)

enable_testing()
add_executable(test_bptree tests/test_bptree.cpp)
target_link_libraries(test_bptree PRIVATE bptree_core)
target_compile_options(test_bptree PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O2>
)
add_test(NAME bptree_correctness COMMAND test_bptree)
