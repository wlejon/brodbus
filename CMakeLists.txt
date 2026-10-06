cmake_minimum_required(VERSION 3.24)

project(brodbus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(_brodbus_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_brodbus_top_level ON)
endif()

option(BRODBUS_BUILD_TESTS "Build brodbus tests" ${_brodbus_top_level})

function(brodbus_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

find_package(Threads REQUIRED)

set(BRODBUS_SOURCES
    src/types.cpp)

set(BRODBUS_LINUX OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BRODBUS_LINUX ON)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
    list(APPEND BRODBUS_SOURCES
        src/bus.cpp
        src/error.cpp
        src/message.cpp
        src/private_bus.cpp
        src/property_cache.cpp
        src/slot.cpp)
else()
    message(STATUS "brodbus: not Linux; D-Bus entry points report themselves unavailable")
    list(APPEND BRODBUS_SOURCES src/unavailable.cpp)
endif()

add_library(brodbus STATIC ${BRODBUS_SOURCES})
add_library(brodbus::brodbus ALIAS brodbus)

target_include_directories(brodbus
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

brodbus_warnings(brodbus)

target_link_libraries(brodbus PUBLIC Threads::Threads)
if(BRODBUS_LINUX)
    target_link_libraries(brodbus PRIVATE PkgConfig::SYSTEMD)
endif()

if(BRODBUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
