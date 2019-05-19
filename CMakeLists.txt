cmake_minimum_required(VERSION 3.5)

# === Build type (default: RelWithDebInfo, O2) ===========
if (NOT CMAKE_BUILD_TYPE)
    set(BUILD_TYPE_OPTIONS
        "Choose the type of build, "
        "options are: Debug Release RelWithDebInfo MinSizeRel.")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo"
        CACHE ${BUILD_TYPE_OPTIONS} FORCE)
    message(STATUS "Build type is not given, use default.")
endif ()
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})


set(ROOT_SRC ${PROJECT_SOURCE_DIR}/src)
set(TEST_DIR ${PROJECT_SOURCE_DIR}/tests)
set(EXAMPLE_DIR ${PROJECT_SOURCE_DIR}/example)

set(LIBDL dl)

# Includes
include_directories(BEFORE ./)
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/src)
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/tests)

# Compiler flags
if (NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    if (NOT APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    endif ()
endif()
    
if (ADDRESS_SANITIZER GREATER 0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
    message(STATUS "---- ADDRESS SANITIZER IS ON ----")
endif()

if (THREAD_SANITIZER GREATER 0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=gold")
    add_definitions(-DSUPPRESS_TSAN_FALSE_ALARMS=1)
    message(STATUS "---- THREAD SANITIZER IS ON ----")
endif()

if (LEAK_SANITIZER GREATER 0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
    message(STATUS "---- LEAK SANITIZER IS ON ----")
endif()


if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd5045 /wd4571 /wd4774 /wd4820 /wd5039 /wd4626 /wd4625 /wd5026 /wd5027 /wd4623 /wd4996 /W3")
    message(STATUS "---- WIN32 ----")
endif()


# === Program flags ===

if ( (TESTSUITE_NO_COLOR GREATER 0) OR (WIN32) )
    add_definitions(-DTESTSUITE_NO_COLOR=1)
    add_definitions(-DLOGGER_NO_COLOR=1)    
    message(STATUS "---- NO ANSI COLOR ----")
endif()

if (APPLE)
    set(OPEN_MEMSTREAM ${COMMON_SRC}/open_memstream.c)
endif ()


# === Test ===

set(LOGGER_TEST 
    ${TEST_DIR}/logger_test.cc
    ${ROOT_SRC}/logger.cc)
add_executable(logger_test ${LOGGER_TEST})


# === Example ===

set(BASIC_EXAMPLE 
    ${EXAMPLE_DIR}/basic_example.cc
    ${ROOT_SRC}/logger.cc)
add_executable(basic_example ${BASIC_EXAMPLE})

set(CRASH_EXAMPLE 
    ${EXAMPLE_DIR}/crash_example.cc
    ${ROOT_SRC}/logger.cc)
add_executable(crash_example ${CRASH_EXAMPLE})
