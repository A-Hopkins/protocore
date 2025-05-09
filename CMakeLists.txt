cmake_minimum_required(VERSION 3.15)
project(protocore VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES
    src/message_queue.cpp
    src/task.cpp
    src/state_manager.cpp
    src/memory_pool.cpp
    src/heart_beat.cpp
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
