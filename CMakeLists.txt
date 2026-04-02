cmake_minimum_required(VERSION 3.15)

file(READ "VERSION" VER_STR)
string(STRIP "${VER_STR}" VER_STR)

project(bicep-metric-service VERSION ${VER_STR} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build unit tests" OFF)

include(FetchContent)

# Fetching httplib
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

# Fetching nlohmann_json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Main library (to be shared with tests)
add_library(metric_service_lib STATIC
    src/collector.cpp
    src/exporter.cpp
    src/utils.cpp
)
target_link_libraries(metric_service_lib PUBLIC httplib::httplib nlohmann_json::nlohmann_json)

# Coverage flags
if(BUILD_TESTS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(metric_service_lib PRIVATE --coverage)
        target_link_options(metric_service_lib PRIVATE --coverage)
    endif()
endif()

# Main executable
add_executable(bicep-metric-service src/main.cpp)
target_link_libraries(bicep-metric-service PRIVATE metric_service_lib)

# For Docker socket support, we might need pthread
find_package(Threads REQUIRED)
target_link_libraries(bicep-metric-service PRIVATE Threads::Threads)

# Testing
if(BUILD_TESTS)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
    )
    # For Windows: prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(bicep_tests
        tests/main_tests.cpp
        tests/test_collector.cpp
        tests/test_exporter.cpp
        tests/test_utils.cpp
    )
    target_link_libraries(bicep_tests PRIVATE
        metric_service_lib
        GTest::gtest_main
    )
    
    # Coverage flags for tests too
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(bicep_tests PRIVATE --coverage)
        target_link_options(bicep_tests PRIVATE --coverage)
    endif()

    include(GoogleTest)
    gtest_discover_tests(bicep_tests)
endif()
