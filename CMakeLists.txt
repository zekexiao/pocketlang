##  Copyright (c) 2020-2021 Thakee Nathees
##  Distributed Under The MIT License

cmake_minimum_required(VERSION 3.10)
project(pocket C)

option(PK_EXE "" OFF)
option(PK_SHARED "" ON)

# Directories
set(SRC_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/core/"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/"
)

set(INC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src/include/")

# Find source files
file(GLOB_RECURSE SOURCES "")
foreach (DIR ${SRC_DIRS})
    file(GLOB DIR_SOURCES "${DIR}*.c")
    list(APPEND SOURCES ${DIR_SOURCES})
endforeach ()

# Create executable
if (PK_SHARED)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
elseif ()
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DPK_DLL -DPK_COMPILE)

# Include directories
include_directories(${INC_DIRS})

# Platform-specific settings
if (MSVC)
    # MSVC-specific settings
    target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:/DDEBUG /Od /Zi>
            $<$<CONFIG:Release>:/O2 /Zi>
    )
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    # Disable warning about unsafe functions in MSVC
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else ()
    # GCC/Clang specific settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
            C_STANDARD 11
            POSITION_INDEPENDENT_CODE ON
    )

    target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:-DDEBUG -g3 -Og>
            $<$<CONFIG:Release>:-g -O3>
    )

    # Link with math and dynamic loading libs on Unix
    if (UNIX)
        target_link_libraries(${PROJECT_NAME} m dl)
    endif ()
endif ()

# Set common properties
set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "${PROJECT_NAME}"
        DEBUG_POSTFIX "d"
        RELEASE_POSTFIX ""
)

if (PK_EXE)
    add_executable(${PROJECT_NAME}cli
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/main.c
            ${CMAKE_CURRENT_SOURCE_DIR}/cli/argparse.h
    )
    if (PK_SHARED)
        target_compile_definitions(${PROJECT_NAME}cli PRIVATE -DPK_DLL)
    endif ()
    target_link_libraries(${PROJECT_NAME}cli PRIVATE ${PROJECT_NAME})
endif ()