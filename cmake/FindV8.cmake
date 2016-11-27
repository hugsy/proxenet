#
# Custom cmake file for libv8
#
# Once done this will define
# V8_FOUND
# V8_INCLUDE_DIR
# V8_LIBRARIES
# V8_VERSION_MAJOR
# V8_VERSION_MINOR
# V8_VERSION_PATCH
# V8_VERSION

if(V8_FOUND)
  # Already in cache, be silent
  SET(V8_FIND_QUIETLY TRUE)
endif()

set(V8_INC_PATHS
  /usr/include
  ${CMAKE_INCLUDE_PATH}
)
find_path(V8_INCLUDE_DIR v8.h PATHS ${V8_INC_PATHS})
find_library(V8_LIBRARY
  NAMES v8
  PATHS /lib /usr/lib /usr/local/lib /usr/pkg/lib
)

find_package_handle_standard_args(V8 DEFAULT_MSG V8_LIBRARY V8_INCLUDE_DIR)

if(V8_LIBRARY-NOTFOUND)
  set(JAVASCRIPT_FOUND False)
  return()
endif()

execute_process(
    COMMAND bash -c "echo \"#include <v8.h>\nusing namespace v8;\nint main(){printf(V8::GetVersion());return 0;}\">a.c;c++ a.c -I${V8_INCLUDE_DIR} ${V8_LIBRARY} ;./a.out;rm -f a.c a.out"
    OUTPUT_VARIABLE V8_VERSION
    )

string(REPLACE "." ";" V8_VERSION_LIST ${V8_VERSION})

list(GET V8_VERSION_LIST 0 V8_VERSION_MAJOR)
list(GET V8_VERSION_LIST 1 V8_VERSION_MINOR)
list(GET V8_VERSION_LIST 2 V8_VERSION_PATCH)

set(JAVASCRIPT_FOUND True)
mark_as_advanced(
  JAVASCRIPT_FOUND
  V8_INCLUDE_DIR
  V8_LIBRARY
  V8_VERSION
  V8_VERSION_MAJOR
  V8_VERSION_MINOR
  V8_VERSION_PATCH
)
