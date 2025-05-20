cmake_minimum_required(VERSION 3.20)

project(Multithread-Progress-Tracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NAME ${PROJECT_NAME})



# --------------------------- Add third party ---------------------------
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party")
include(FetchContent)

# --------------------------- JSON ---------------------------
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git

    # ### Do not use newer versions (v1.15.0 or later) due to linking errors.
    GIT_TAG v1.16.0
)

FetchContent_MakeAvailable(json googletest)

# ------------------------- httplib (header-only) -------------------------
set(HTTPLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/httplib")

# Create directory if not already there
file(MAKE_DIRECTORY ${HTTPLIB_INCLUDE_DIR})

set(HTTPLIB_URL "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h")
set(HTTPLIB_HEADER "${HTTPLIB_INCLUDE_DIR}/httplib.h")

if(NOT EXISTS ${HTTPLIB_HEADER})
    message(STATUS "Downloading httplib.h to ${HTTPLIB_HEADER}")
    file(DOWNLOAD
        ${HTTPLIB_URL}
        ${HTTPLIB_HEADER}
        SHOW_PROGRESS
        TLS_VERIFY ON)
endif()

set(RESOURCE_FILE "${CMAKE_SOURCE_DIR}/resource/icon.rc")

set(SOURCES
    src/main.cc
    src/Logger.cc
    src/ProgressTracker.cc
    src/JobDispatcher.cc
    src/Worker.cc
)


if(WIN32)
    set(RESOURCE_FILE "${CMAKE_SOURCE_DIR}/resource/icon.rc")
    list(APPEND SOURCES ${RESOURCE_FILE})
endif()

add_executable(${NAME} ${SOURCES})

target_link_libraries(${NAME} PRIVATE user32)

# Detect compiler
if(NOT WIN32)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "GCC detected")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
            target_link_libraries(${NAME} PRIVATE stdc++fs)
        endif()

    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "Clang detected")
        target_link_libraries(${NAME} PRIVATE stdc++fs)
    endif()

endif()

target_include_directories(
    ${NAME}

    # PRIVATE ${json_BINARY_DIR}/single_include
    PRIVATE ${json_BINARY_DIR}/include
)

find_package(Threads REQUIRED)

add_executable(
    job_benchmark
    src/benchmark_main.cc
    src/JobDispatcher.cc
    src/Worker.cc
    src/Logger.cc
    src/ProgressTracker.cc
)

target_link_libraries(job_benchmark Threads::Threads)

# --------------------------- Build test ---------------------------
enable_testing()
# add_subdirectory(test)

add_executable(
    job_dispatcher_test
    test/test_job_dispatcher.cc
    test/test_progress_tracker.cc
    test/test_logger.cc
    test/test_job.cc
    test/test_benchmark.cc
    src/JobDispatcher.cc
    src/ProgressTracker.cc
    src/Worker.cc
    src/Logger.cc
)

target_link_libraries(job_dispatcher_test gtest gtest_main Threads::Threads)
target_include_directories(job_dispatcher_test PRIVATE .)
