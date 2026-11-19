cmake_minimum_required(VERSION 3.16)
project(hells_tower LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
include(CTest)
if(NOT DEFINED APP_VERSION)
  execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE APP_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(NOT APP_VERSION)
    set(APP_VERSION "v0.0.0-dev")
  endif()
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(USE_EXTERNAL_GLFW OFF CACHE BOOL "" FORCE)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

find_package(raylib QUIET)
if(TARGET raylib)
  set(RAYLIB_TARGET raylib)
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(RAYLIB QUIET IMPORTED_TARGET raylib)
  endif()

  if(TARGET PkgConfig::RAYLIB)
    set(RAYLIB_TARGET PkgConfig::RAYLIB)
  else()
    include(FetchContent)
    FetchContent_Declare(
      raylib
      GIT_REPOSITORY https://github.com/raysan5/raylib.git
      GIT_TAG 5.5
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
    set(RAYLIB_TARGET raylib)
  endif()
endif()

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/menu/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/loading_screen/*.c"
)

add_executable(hells_tower main.c updater.c ${GAME_SOURCES})
if(BUILD_TESTING)
  add_executable(game_balance_test
    tests/game_balance_test.c
    game/game_zone/balance.c
    game/items/drop_rates.c
    game/items/functionnal/functionnal.c
  )
  add_test(NAME game_balance_test COMMAND game_balance_test)
endif()
if(NOT CMAKE_C_FLAGS MATCHES "(^| )-DAPP_VERSION")
  target_compile_definitions(hells_tower PRIVATE APP_VERSION="${APP_VERSION}")
endif()
if(WIN32)
  configure_file(
    packaging/windows/hells_tower.rc.in
    "${CMAKE_CURRENT_BINARY_DIR}/hells_tower.rc"
    @ONLY
  )
  target_sources(hells_tower PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/hells_tower.rc")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(hells_tower PRIVATE linux_desktop.c)
endif()
target_link_libraries(hells_tower PRIVATE ${RAYLIB_TARGET} CURL::libcurl Threads::Threads)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(hells_tower PRIVATE -Wall -Wextra)
endif()

if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_link_options(hells_tower PRIVATE -static -static-libgcc)
endif()

add_custom_command(TARGET hells_tower POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/assets"
          "$<TARGET_FILE_DIR:hells_tower>/assets"
)

if(DEFINED raylib_SOURCE_DIR AND EXISTS "${raylib_SOURCE_DIR}/LICENSE")
  file(READ "${raylib_SOURCE_DIR}/LICENSE" RAYLIB_LICENSE)
  set(THIRD_PARTY_NOTICES "raylib\n======\n${RAYLIB_LICENSE}")
  if(EXISTS "${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md")
    file(READ "${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md" GLFW_LICENSE)
    string(APPEND THIRD_PARTY_NOTICES "\n\nGLFW\n====\n${GLFW_LICENSE}")
  endif()
  file(WRITE "${CMAKE_BINARY_DIR}/THIRD_PARTY_NOTICES.txt"
       "${THIRD_PARTY_NOTICES}")
endif()

configure_file(${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt ${CMAKE_BINARY_DIR}/THIRD_PARTY_NOTICES.txt COPYONLY)
