cmake_minimum_required(VERSION 3.16)
project(pocketlang LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(POCKETLANG_CORE_SOURCES
  src/core/compiler.cpp
  src/core/core.cpp
  src/core/debug.cpp
  src/core/public.cpp
  src/core/utils.cpp
  src/core/value.cpp
  src/core/vm.cpp
)

set(POCKETLANG_LIB_SOURCES
  src/libs/libs.cpp
  src/libs/std_dummy.cpp
  src/libs/std_io.cpp
  src/libs/std_json.cpp
  src/libs/std_math.cpp
  src/libs/std_os.cpp
  src/libs/std_path.cpp
  src/libs/std_term.cpp
  src/libs/std_time.cpp
  src/libs/std_types.cpp
  src/libs/thirdparty/cJSON/cJSON.c
  src/libs/thirdparty/cwalk/cwalk.c
)

set(POCKETLANG_CLI_SOURCES
  cli/cli_parser.cpp
  cli/pocket_app.cpp
  cli/pocket_vm.cpp
  cli/main.cpp
)

add_library(pocketlang STATIC
  ${POCKETLANG_CORE_SOURCES}
  ${POCKETLANG_LIB_SOURCES}
)

target_include_directories(pocketlang
  PUBLIC
    src/include
  PRIVATE
    cli
)

if(UNIX AND NOT APPLE)
  target_link_libraries(pocketlang PUBLIC m dl)
elseif(APPLE)
  target_link_libraries(pocketlang PUBLIC m)
endif()

add_executable(pocket ${POCKETLANG_CLI_SOURCES})

target_include_directories(pocket PRIVATE src/include cli)
target_link_libraries(pocket PRIVATE pocketlang)
