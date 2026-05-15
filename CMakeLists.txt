cmake_minimum_required(VERSION 3.14)
project(term-ime VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# ============================================================
# Dependencies
# ============================================================
add_subdirectory(deps/ftxui)
add_subdirectory(deps/spdlog)
add_subdirectory(deps/json)

# Use system librime
find_package(PkgConfig REQUIRED)
pkg_check_modules(RIME REQUIRED rime)

# Use system libuv
find_library(LIBUV_LIBRARY uv REQUIRED)
find_path(LIBUV_INCLUDE_DIR uv.h REQUIRED)

# RxCpp is header-only
set(RXCPP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/deps/rxcpp/Rx/v2/src)

# ============================================================
# IME Library (independent, reusable)
# ============================================================
add_library(term-ime-lib STATIC
    src/ime/engine.cpp
    src/ime/rime_engine.cpp
    src/ime/language.cpp
    src/ime/ranker.cpp
    src/ime/neural_ranker.cpp
    src/util/utf8.cpp
    src/util/logger.cpp
)

target_include_directories(term-ime-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        ${RIME_INCLUDE_DIRS}
)

target_link_libraries(term-ime-lib
    PUBLIC ${RIME_LIBRARIES}
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC spdlog::spdlog
)

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
)

target_include_directories(term-core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
    PUBLIC ${LIBUV_INCLUDE_DIR}
)

target_link_libraries(term-core
    PUBLIC term-terminal
    PUBLIC term-ime-lib
    PUBLIC ${LIBUV_LIBRARY}
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
    src/ime/neural_ranker.hpp
    DESTINATION include/term-ime
)

install(FILES
    src/util/utf8.hpp
    DESTINATION include/term-ime/util
)

install(FILES data/pinyin.dict DESTINATION share/term-ime)

# Copy data directory to build for testing
configure_file(${CMAKE_SOURCE_DIR}/data/pinyin.dict ${CMAKE_BINARY_DIR}/data/pinyin.dict COPYONLY)