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

FetchContent_MakeAvailable(json)

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
