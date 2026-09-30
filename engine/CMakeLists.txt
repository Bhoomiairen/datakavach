cmake_minimum_required(VERSION 3.16)
project(datakavach VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

# The engine as a library, so the CLI and the tests share exactly the same code.
add_library(dkcore
  src/aho_corasick.cpp
  src/validators.cpp
  src/detector.cpp
  src/masker.cpp
  src/scanner.cpp
)
target_include_directories(dkcore PUBLIC include)
target_link_libraries(dkcore PUBLIC Threads::Threads)

add_executable(datakavach src/main.cpp)
target_link_libraries(datakavach PRIVATE dkcore)

enable_testing()
add_executable(dktests
  tests/test_main.cpp
  tests/test_validators.cpp
  tests/test_aho_corasick.cpp
  tests/test_detector.cpp
  tests/test_masker.cpp
  tests/test_scanner.cpp
)
target_link_libraries(dktests PRIVATE dkcore)
target_compile_definitions(dktests PRIVATE DK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
add_test(NAME dktests COMMAND dktests)
