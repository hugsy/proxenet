# Try to find PolarSSL library
#
# Once done this will define
# POLARSSL_FOUND
# POLARSSL_INCLUDE_DIR
# POLARSSL_LIBRARIES
# POLARSSL_VERSION_MAJOR
# POLARSSL_VERSION_MINOR
# POLARSSL_VERSION_PATCH
# POLARSSL_VERSION

include(FindPackageHandleStandardArgs)

find_path(POLARSSL_INCLUDE_DIR NAMES polarssl/ssl.h)

find_library(POLARSSL_LIBRARIES NAMES polarssl)

find_package_handle_standard_args(POLARSSL REQUIRED_VARS
  POLARSSL_INCLUDE_DIR POLARSSL_LIBRARIES)

if(!POLARSSL_INCLUDE_DIR AND !POLARSSL_LIBRARIES)
  message(FATAL_ERROR "Cannot find PolarSSL library")
  set(POLARSSL_FOUND False)
  return()
endif()

execute_process(
    COMMAND bash -c "echo \"#include <polarssl/version.h>\n#include <stdio.h>\nint main(){printf(POLARSSL_VERSION_STRING);return 0;}\">a.c;cc a.c -lpolarssl;./a.out;rm -f a.out a.c"
    OUTPUT_VARIABLE POLARSSL_VERSION
    )
string(REPLACE "." ";" POLARSSL_VERSION_LIST ${POLARSSL_VERSION})

list(GET ${POLARSSL_VERSION_LIST} 0 POLARSSL_VERSION_MAJOR)
list(GET ${POLARSSL_VERSION_LIST} 1 POLARSSL_VERSION_MINOR)
list(GET ${POLARSSL_VERSION_LIST} 2 POLARSSL_VERSION_PATCH)

if( ${POLARSSL_VERSION} VERSION_LESS "1.3.0")
  message(FATAL_ERROR "PolarSSL 1.3+ is required for compiling ${PROGNAME} (current is ${POLARSSL_VERSION}).")
  set(POLARSSL_FOUND False)
  return()
endif()

set(POLARSSL_FOUND True)
mark_as_advanced(
  POLARSSL_FOUND
  POLARSSL_INCLUDE_DIR
  POLARSSL_LIBRARIES
  POLARSSL_VERSION_MAJOR
  POLARSSL_VERSION_MINOR
  POLARSSL_VERSION_PATCH
  POLARSSL_VERSION
  )
