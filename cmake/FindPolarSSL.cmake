# Try to find PolarSSL library
#
# Once done this will define
# POLARSSL_FOUND
# POLARSSL_INCLUDE_DIR
# POLARSSL_LIBRARIES

include(FindPackageHandleStandardArgs)

find_path(POLARSSL_INCLUDE_DIR NAMES polarssl/ssl.h)

find_library(POLARSSL_LIBRARIES NAMES polarssl)

find_package_handle_standard_args(POLARSSL REQUIRED_VARS
  POLARSSL_INCLUDE_DIR POLARSSL_LIBRARIES)

set(POLARSSL_FOUND True)
mark_as_advanced(
  POLARSSL_FOUND
  POLARSSL_INCLUDE_DIR
  POLARSSL_LIBRARIES
  )
