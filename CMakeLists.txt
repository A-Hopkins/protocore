cmake_minimum_required(VERSION 3.15)
project(protocore VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(BUILD_TESTING true)

set(SOURCES
    src/message_queue.cpp
    src/task.cpp
    src/state_manager.cpp
    src/memory_pool.cpp
    src/heart_beat.cpp
    src/logger.cpp
)

add_library(protocore ${SOURCES})
target_include_directories(protocore PUBLIC include)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

file(GLOB_RECURSE ALL_SOURCE_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp"
  )

if(BUILD_TESTING)
  add_custom_target(
    clang_format
    COMMAND clang-format -i ${ALL_SOURCE_FILES}
  )
endif()