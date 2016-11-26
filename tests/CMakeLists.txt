cmake_minimum_required(VERSION 3.7)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
set(EXTRA_LIBS "")
set(EXTRA_LIBS ${EXTRA_LIBS} "-lpthread")

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})

set(TEST_SOURCES
  ../utils.c ../utils.h ./check_utils.c
)


add_executable(check_utils ${TEST_SOURCES})
target_link_libraries(check_utils ${EXTRA_LIBS} ${CHECK_LIBRARIES})
