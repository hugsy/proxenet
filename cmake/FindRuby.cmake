# Ruby cmake package
#
# Returns
# RUBY_FOUND
# RUBY_INCLUDE_DIRS
# RUBY_LIBRARIES
# _RUBY_MAJOR_
# _RUBY_MINOR_

include(FindPackageHandleStandardArgs)

find_program(RUBY_EXECUTABLE
  NAMES ruby1.9.3 ruby193 ruby1.9.2 ruby192 ruby1.9.1 ruby191 ruby1.9 ruby19 ruby1.8 ruby18 ruby
  PATHS /usr/bin /usr/local/bin /usr/pkg/bin
  )

if(RUBY_EXECUTABLE)
  # Get Ruby version
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -e "print RUBY_VERSION.split('.')[0]"
    OUTPUT_VARIABLE _RUBY_MAJOR_
    )
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -e "print RUBY_VERSION.split('.')[1]"
    OUTPUT_VARIABLE _RUBY_MINOR_
    )


  # Get Ruby include directories
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['rubyhdrdir']"
    OUTPUT_VARIABLE RUBY_INCLUDE_HDR_DIR
    )
  find_path(RUBY_INCLUDE_DIR NAMES ruby.h ruby/config.h)
  if(RUBY_INCLUDE_DIR-NOTFOUND)
    return()
  endif()
  set(RUBY_INCLUDE_DIRS ${RUBY_INCLUDE_HDR_DIR} ${RUBY_INCLUDE_DIR})


  # Get Ruby libraries directories
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['RUBY_SO_NAME']"
    OUTPUT_VARIABLE RUBY_LIB
    )
  find_library(RUBY_LIBRARIES NAMES ruby${_RUBY_MAJOR_}-${_RUBY_MINOR_})
  if(RUBY_LIBRARIES-NOTFOUND)
    return()
  endif()
  set(RUBY_LIBRARIES ${RUBY_LIBRARIES} ${RUBY_LIB})


  set (RUBY_FOUND True)
  mark_as_advanced(
    RUBY_FOUND
    RUBY_LIBRARIES RUBY_INCLUDE_DIRS
    _RUBY_MAJOR_
    _RUBY_MINOR_
    )
endif(RUBY_EXECUTABLE)
