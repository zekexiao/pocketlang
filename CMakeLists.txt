# Copyright (c) 2020-2021 Thakee Nathees
# Distributed Under The MIT License

cmake_minimum_required(VERSION 3.5)
project(pocket C)

set(POCKET_SRC_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libs
)

add_compile_definitions(PK_DLL)

file(GLOB POCKET_SOURCES "")
foreach (POCKET_SRC_DIR ${POCKET_SRC_DIRS})
    file(GLOB SOURCES "${POCKET_SRC_DIR}/*.c")
    list(APPEND POCKET_SOURCES ${SOURCES})
endforeach ()

add_library(${PROJECT_NAME} ${POCKET_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
target_compile_definitions(${PROJECT_NAME} PRIVATE PK_COMPILE)
if (NOT MSVC)
    target_link_libraries(${PROJECT_NAME} m dl)
    target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
endif ()

# if pocket is top level proejct build cli
if (${PROJECT_NAME} STREQUAL ${CMAKE_PROJECT_NAME})
    add_executable(${PROJECT_NAME}cli ${CMAKE_SOURCE_DIR}/cli/main.c)
    target_link_libraries(${PROJECT_NAME}cli ${PROJECT_NAME})
endif ()
