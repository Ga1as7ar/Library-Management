cmake_minimum_required(VERSION 3.21)
project(LibraryManagement VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LIBRARY_BUILD_WEB   "Build the HTTP/Web front-end"  ON)
option(LIBRARY_BUILD_QT    "Build the Qt desktop front-end" ON)
option(LIBRARY_BUILD_TESTS "Build unit tests"               ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(SQLite3 REQUIRED)

# Header-only vendored dependencies (versions pinned in third_party/README.md).
add_library(third_party_headers INTERFACE)
target_include_directories(third_party_headers SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_subdirectory(core)

if(LIBRARY_BUILD_WEB)
    add_subdirectory(web)
endif()

if(LIBRARY_BUILD_QT)
    find_package(Qt6 6.5 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        add_subdirectory(qt)
    else()
        message(WARNING "Qt6 not found — skipping the desktop app. Set CMAKE_PREFIX_PATH to your Qt install.")
    endif()
endif()

if(LIBRARY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
