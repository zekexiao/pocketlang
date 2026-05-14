cmake_minimum_required(VERSION 3.16)
project(pocketlang LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(POCKETLANG_CORE_SOURCES
  src/core/compiler.c
  src/core/core.c
  src/core/debug.c
  src/core/public.c
  src/core/utils.c
  src/core/value.c
  src/core/vm.c
)

set(POCKETLANG_LIB_SOURCES
  src/libs/libs.c
  src/libs/std_dummy.c
  src/libs/std_io.c
  src/libs/std_json.c
  src/libs/std_math.c
  src/libs/std_os.c
  src/libs/std_path.c
  src/libs/std_term.c
  src/libs/std_time.c
  src/libs/std_types.c
  src/libs/thirdparty/cJSON/cJSON.c
  src/libs/thirdparty/cwalk/cwalk.c
)

set(POCKETLANG_CLI_SOURCES
  cli/CliParser.cpp
  cli/PocketApp.cpp
  cli/PocketVm.cpp
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
