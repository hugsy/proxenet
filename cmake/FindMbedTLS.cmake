# Try to find mbedTLS library
#
# Once done this will define
# MBEDTLS_FOUND
# MBEDTLS_INCLUDE_DIR
# MBEDTLS_LIBRARIES
# MBEDTLS_VERSION_MAJOR
# MBEDTLS_VERSION_MINOR
# MBEDTLS_VERSION_PATCH
# MBEDTLS_VERSION

include(FindPackageHandleStandardArgs)

find_path(MBEDTLS_INCLUDE_DIR NAMES mbedtls/ssl.h)
find_library(MBEDX509_LIB NAMES mbedx509)
find_package_handle_standard_args(MBEDTLS REQUIRED_VARS MBEDTLS_INCLUDE_DIR MBEDTLS_LIBRARIES)

if( ${MBEDTLS_LIBRARIES-NOTFOUND} )
  message(FATAL_ERROR "Failed to get info from Mbedtls library, check your Mbedtls installation")
  set(MBEDTLS_FOUND False)
  return()
endif()

execute_process(
    COMMAND bash -c "echo \"#include <mbedtls/version.h>\n#include <stdio.h>\nint main(){printf(MBEDTLS_VERSION_STRING);return 0;}\">a.c;cc a.c -I${MBEDTLS_INCLUDE_DIR} ${MBEDTLS_LIBRARIES} ;./a.out;rm -f a.c a.out"
    OUTPUT_VARIABLE MBEDTLS_VERSION
    )

string(REPLACE "." ";" MBEDTLS_VERSION_LIST ${MBEDTLS_VERSION})

list(GET ${MBEDTLS_VERSION_LIST} 0 MBEDTLS_VERSION_MAJOR)
list(GET ${MBEDTLS_VERSION_LIST} 1 MBEDTLS_VERSION_MINOR)
list(GET ${MBEDTLS_VERSION_LIST} 2 MBEDTLS_VERSION_PATCH)

if( ${MBEDTLS_VERSION} VERSION_LESS "2.1.0")
  message(FATAL_ERROR "Mbedtls 2.1+ is required for compiling ${PROGNAME} (current is ${MBEDTLS_VERSION}).")
  set(MBEDTLS_FOUND False)
  return()
endif()

find_library(MBEDCRYPTO_LIB NAMES mbedcrypto)
find_library(MBEDTLS_LIBRARIES NAMES mbedtls)
set(MBEDTLS_LIBRARIES ${MBEDX509_LIB} ${MBEDTLS_LIB} ${MBEDCRYPTO_LIB})

set(MBEDTLS_FOUND True)
mark_as_advanced(
  MBEDTLS_FOUND
  MBEDTLS_INCLUDE_DIR
  MBEDTLS_LIBRARIES
  MBEDTLS_VERSION_MAJOR
  MBEDTLS_VERSION_MINOR
  MBEDTLS_VERSION_PATCH
  MBEDTLS_VERSION
  )
