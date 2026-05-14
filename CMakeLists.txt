cmake_minimum_required(VERSION 3.14)
project(term-ime VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# ============================================================
# IME Library (independent, reusable)
# ============================================================
add_library(term-ime-lib STATIC
    src/ime/engine.cpp
    src/ime/pinyin.cpp
    src/ime/dict.cpp
    src/util/utf8.cpp
)

target_include_directories(term-ime-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
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
)

# ============================================================
# Core Event System
# ============================================================
add_library(term-core STATIC
    src/core/event_loop.cpp
    src/core/app.cpp
)

target_include_directories(term-core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(term-core
    PUBLIC term-terminal
    PUBLIC term-ime-lib
)

# ============================================================
# Main Executable
# ============================================================
add_executable(term-ime src/main.cpp)

target_link_libraries(term-ime
    PRIVATE term-core
    PRIVATE util
)

# ============================================================
# Event-driven version
# ============================================================
add_executable(term-ime-event src/main_event.cpp)

target_link_libraries(term-ime-event
    PRIVATE term-core
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
    src/ime/pinyin.hpp
    src/ime/dict.hpp
    DESTINATION include/term-ime
)

install(FILES
    src/util/utf8.hpp
    DESTINATION include/term-ime/util
)

install(FILES data/pinyin.dict DESTINATION share/term-ime)

# Copy data directory to build for testing
configure_file(${CMAKE_SOURCE_DIR}/data/pinyin.dict ${CMAKE_BINARY_DIR}/data/pinyin.dict COPYONLY)