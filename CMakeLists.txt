cmake_minimum_required(VERSION 3.14)
project(term-ime VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# ============================================================
# Sanitizers (product-grade hardening / leak detection)
# ============================================================
option(ENABLE_ASAN "Enable AddressSanitizer + LeakSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if(ENABLE_ASAN)
    message(STATUS "Enabling AddressSanitizer + LeakSanitizer")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()
if(ENABLE_UBSAN)
    message(STATUS "Enabling UndefinedBehaviorSanitizer")
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
endif()

# ============================================================
# Hardware acceleration options (configurable)
# ============================================================
option(LLAMA_USE_CUDA "Enable CUDA acceleration" OFF)
option(LLAMA_USE_METAL "Enable Metal/MPS acceleration (Mac)" OFF)
option(LLAMA_USE_VULKAN "Enable Vulkan acceleration" OFF)
option(LLAMA_USE_NPU "Enable NPU acceleration" OFF)

# ============================================================
# Dependencies
# ============================================================
add_subdirectory(deps/ftxui)
add_subdirectory(deps/spdlog)
add_subdirectory(deps/json)
add_subdirectory(deps/googletest)

# Build librime from source (minimal configuration). Self-contained build
# (no system librime-dev dependency) for reproducibility across distros.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_DATA OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(ENABLE_LOGGING OFF CACHE BOOL "" FORCE)
set(ENABLE_THREADING ON CACHE BOOL "" FORCE)
add_subdirectory(deps/librime EXCLUDE_FROM_ALL)

# llama.cpp - minimal build for LLM ranking with optional acceleration
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Configure acceleration backends
if(LLAMA_USE_CUDA)
    message(STATUS "Enabling CUDA acceleration for llama.cpp")
    set(LLAMA_CUDA ON CACHE BOOL "" FORCE)
endif()

if(LLAMA_USE_METAL)
    message(STATUS "Enabling Metal/MPS acceleration for llama.cpp")
    set(LLAMA_METAL ON CACHE BOOL "" FORCE)
endif()

if(LLAMA_USE_VULKAN)
    message(STATUS "Enabling Vulkan acceleration for llama.cpp")
    set(LLAMA_VULKAN ON CACHE BOOL "" FORCE)
endif()

# NPU support via Vulkan or custom backend
if(LLAMA_USE_NPU)
    message(STATUS "Enabling NPU acceleration (via Vulkan)")
    set(LLAMA_VULKAN ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(deps/llama.cpp EXCLUDE_FROM_ALL)

# Use system libuv
find_library(LIBUV_LIBRARY uv REQUIRED)
find_path(LIBUV_INCLUDE_DIR uv.h REQUIRED)

# Use system curl for model downloads
find_package(CURL REQUIRED)

# SML (State Machine Library) - header-only
set(SML_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/deps/sml/include)

# utf8proc - UTF-8 processing library
add_library(utf8proc STATIC
    deps/utf8proc/utf8proc.c
)
target_include_directories(utf8proc PUBLIC ${CMAKE_SOURCE_DIR}/deps/utf8proc)
target_compile_definitions(utf8proc PRIVATE UTF8PROC_STATIC)

# ============================================================
# IME Library (independent, reusable)
# ============================================================
add_library(term-ime-lib STATIC
    src/ime/engine.cpp
    src/ime/rime_engine.cpp
    src/ime/language.cpp
    src/ime/ranker.cpp
    src/ime/llama_ranker.cpp
    src/ime/kaomoji.cpp
    src/util/utf8.cpp
    src/util/logger.cpp
    src/util/i18n.cpp
    src/util/model_downloader.cpp
)

target_include_directories(term-ime-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        ${CMAKE_SOURCE_DIR}/deps/librime/include
        ${CMAKE_SOURCE_DIR}/deps/librime/src
        ${CMAKE_SOURCE_DIR}/deps/llama.cpp/include
)

target_link_libraries(term-ime-lib
    PUBLIC rime-static
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC spdlog::spdlog
    PUBLIC utf8proc
    PUBLIC llama
    PUBLIC CURL::libcurl
)
# librime (static) pulls in Boost.regex and friends; its own target_link
# dependencies don't always propagate through the static chain, so link them
# explicitly here. Required at final link on systems without a system librime.
find_package(Boost REQUIRED COMPONENTS regex filesystem system)
target_link_libraries(term-ime-lib PUBLIC Boost::regex Boost::filesystem Boost::system)

set_target_properties(term-ime-lib PROPERTIES
    OUTPUT_NAME "termime"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# ============================================================
# Terminal Library
# ============================================================
add_library(term-terminal STATIC
    src/terminal/pty.cpp
    src/terminal/screen.cpp
    src/terminal/parser.cpp
    src/ui/renderer.cpp
    src/ui/jsx.cpp
    src/ui/components.cpp
    src/ui/settings.cpp
)

target_include_directories(term-terminal
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(term-terminal
    PUBLIC term-ime-lib
    PRIVATE util
    PRIVATE spdlog::spdlog
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE ftxui::dom
    PRIVATE ftxui::screen
)

# ============================================================
# Core Event System
# ============================================================
add_library(term-core STATIC
    src/core/event_loop.cpp
    src/core/app.cpp
    src/core/config.cpp
    src/core/input_processor.cpp
)

target_include_directories(term-core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
    PUBLIC ${LIBUV_INCLUDE_DIR}
    PUBLIC ${SML_INCLUDE_DIR}
)

target_link_libraries(term-core
    PUBLIC term-terminal
    PUBLIC term-ime-lib
    PUBLIC ${LIBUV_LIBRARY}
    PRIVATE ftxui::dom
    PRIVATE ftxui::screen
)

# ============================================================
# Main Executable
# ============================================================
add_executable(term-ime src/main.cpp)

target_link_libraries(term-ime
    PRIVATE term-core
    PRIVATE term-terminal
    PRIVATE term-ime-lib
    PRIVATE spdlog::spdlog
    PRIVATE ftxui::dom
    PRIVATE ftxui::screen
)

# ============================================================
# Install
# ============================================================
install(TARGETS term-ime-lib term-terminal term-core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    src/ime/engine.hpp
    src/ime/rime_engine.hpp
    src/ime/language.hpp
    src/ime/ranker.hpp
    DESTINATION include/term-ime
)

install(FILES
    src/util/utf8.hpp
    DESTINATION include/term-ime/util
)

install(FILES data/pinyin.dict DESTINATION share/term-ime)

# Copy data directory to build for testing
configure_file(${CMAKE_SOURCE_DIR}/data/pinyin.dict ${CMAKE_BINARY_DIR}/data/pinyin.dict COPYONLY)

# Copy translation files
file(GLOB TRANSLATION_FILES "${CMAKE_SOURCE_DIR}/data/translations/*.json")
foreach(TRANSLATION_FILE ${TRANSLATION_FILES})
    get_filename_component(FILENAME ${TRANSLATION_FILE} NAME)
    configure_file(${TRANSLATION_FILE} ${CMAKE_BINARY_DIR}/data/translations/${FILENAME} COPYONLY)
endforeach()

# ============================================================
# Tests
# ============================================================
enable_testing()

# Test executable
add_executable(term-ime-tests
    tests/test_main.cpp
    tests/test_utf8.cpp
    tests/test_config.cpp
    tests/test_ime_state.cpp
    tests/test_input_processor.cpp
)

target_link_libraries(term-ime-tests
    PRIVATE term-ime-lib
    PRIVATE term-core
    PRIVATE GTest::gtest
    PRIVATE GTest::gtest_main
)

# UI JSX test
add_executable(test-ui-jsx
    tests/test_ui_jsx.cpp
)

target_link_libraries(test-ui-jsx
    PRIVATE term-terminal
    PRIVATE term-ime-lib
    PRIVATE spdlog::spdlog
    PRIVATE ftxui::dom
    PRIVATE ftxui::screen
)

# Settings test
add_executable(test-settings
    tests/test_settings.cpp
)

target_link_libraries(test-settings
    PRIVATE term-terminal
    PRIVATE term-core
    PRIVATE term-ime-lib
    PRIVATE spdlog::spdlog
    PRIVATE ftxui::dom
    PRIVATE ftxui::screen
)

# Input processor end-to-end test
add_executable(test-input-e2e
    tests/test_input_e2e.cpp
)

target_link_libraries(test-input-e2e
    PRIVATE term-core
    PRIVATE spdlog::spdlog
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(term-ime-tests)