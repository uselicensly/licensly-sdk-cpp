cmake_minimum_required(VERSION 3.20)
project(licensly VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(CURL REQUIRED)

# libsodium: prefer system install; fall back to pkg-config
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM libsodium)
endif()
if(NOT SODIUM_FOUND)
    find_library(SODIUM_LIBRARIES NAMES sodium REQUIRED)
    find_path(SODIUM_INCLUDE_DIRS NAMES sodium.h PATH_SUFFIXES sodium REQUIRED)
endif()

# ── Library ───────────────────────────────────────────────────────────────────
add_library(licensly STATIC
    src/client.cpp
    src/session.cpp
    src/signing.cpp
)

target_include_directories(licensly
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${SODIUM_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
)

target_link_libraries(licensly
    PRIVATE
        CURL::libcurl
        ${SODIUM_LIBRARIES}
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
