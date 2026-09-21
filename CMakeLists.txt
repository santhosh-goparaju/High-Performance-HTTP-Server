cmake_minimum_required(VERSION 3.16)
project(http_server
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "High-Performance HTTP/1.1 Server"
)

# ── C++20 Standard ─────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler Warnings ──────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast
    -Wcast-align -Wunused -Woverloaded-virtual
    -Wformat=2 -Wnull-dereference
)

# ── Sanitizer Options ──────────────────────────────────────────
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    message(STATUS "AddressSanitizer ENABLED")
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
    message(STATUS "UndefinedBehaviorSanitizer ENABLED")
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
    message(STATUS "ThreadSanitizer ENABLED")
endif()

# ── Source Files ───────────────────────────────────────────────
set(SERVER_SOURCES
    src/net/socket.cpp
    src/net/listener.cpp
    src/net/connection.cpp
    src/http/status.cpp
    src/http/request.cpp
    src/http/response.cpp
    src/http/parser.cpp
    src/http/serializer.cpp
    src/router/router.cpp
    src/server/config.cpp
    src/server/thread_pool.cpp
    src/server/server.cpp
)

# ── Library Target (for tests to link against) ─────────────────
add_library(http_server_lib STATIC ${SERVER_SOURCES})
target_include_directories(http_server_lib PUBLIC include)
target_compile_features(http_server_lib PUBLIC cxx_std_20)

# Platform-specific linking
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(http_server_lib PUBLIC pthread)
endif()

# ── Executable ─────────────────────────────────────────────────
add_executable(http_server src/main.cpp)
target_link_libraries(http_server PRIVATE http_server_lib)

# ── Testing ────────────────────────────────────────────────────
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # Prevent overriding parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Unit tests
    set(TEST_SOURCES
        tests/unit/test_parser.cpp
        tests/unit/test_serializer.cpp
        tests/unit/test_router.cpp
        tests/unit/test_thread_pool.cpp
        tests/unit/test_connection.cpp
    )

    add_executable(unit_tests ${TEST_SOURCES})
    target_link_libraries(unit_tests PRIVATE http_server_lib GTest::gtest_main)
    target_include_directories(unit_tests PRIVATE include)

    include(GoogleTest)
    gtest_discover_tests(unit_tests)

    # Integration tests
    set(INTEGRATION_TEST_SOURCES
        tests/integration/test_server.cpp
        tests/integration/test_security.cpp
    )

    add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
    target_link_libraries(integration_tests PRIVATE http_server_lib GTest::gtest_main)
    target_include_directories(integration_tests PRIVATE include)

    gtest_discover_tests(integration_tests)
endif()
