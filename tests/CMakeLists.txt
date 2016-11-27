################################################################################
#
# proxenet test build for cmake
#
#
cmake_minimum_required(VERSION 2.6)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
set(EXTRA_LIBS "")
set(EXTRA_LIBS ${EXTRA_LIBS} "-lpthread -lmbedtls")

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})

find_package(MbedTLS REQUIRED)
if(MBEDTLS_FOUND  AND  MBEDTLS_INCLUDE_DIR  AND  MBEDTLS_LIBRARIES)
  add_definitions(-DHAVE_MBEDTLS)
  include_directories(${MBEDTLS_INCLUDE_DIR})
  set(EXTRA_LIBS ${EXTRA_LIBS} ${MBEDTLS_LIBRARIES})
  set(_MBEDTLS_VERSION_ ${MBEDTLS_VERSION})
  message("-- mbedTLS ${MBEDTLS_VERSION} found!")
else()
  return()
endif()

add_definitions(-DCCHECK_TEST)

add_executable(check_utils   ../utils.c ../utils.h ./check_utils.c)
target_link_libraries(check_utils ${EXTRA_LIBS} ${CHECK_LIBRARIES})

add_executable(check_http  ./check_http.c
  ../http.c ../http.h
  ../utils.h ../utils.c
  ../ssl.c ../ssl.h
  ../minica.c ../minica.h
  ../socket.c ../socket.h
  ../socks.c ../socks.h
  )
target_link_libraries(check_http ${EXTRA_LIBS} ${CHECK_LIBRARIES})
