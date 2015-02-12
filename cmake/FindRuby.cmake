# Ruby cmake package
#
# Returns
# RUBY_FOUND
# RUBY_INCLUDE_DIRS
# RUBY_LIBRARIES
# _RUBY_MAJOR_
# _RUBY_MINOR_

if(RUBY_FOUND)
   set(RUBY_FIND_QUIETLY TRUE)
endif()

find_program(RUBY_EXECUTABLE
  NAMES ruby2.2 ruby2.1 ruby2.0 ruby1.9.3 ruby193 ruby1.9.2 ruby192 ruby1.9.1 ruby191 ruby1.9 ruby19 ruby1.8 ruby18 ruby
  PATHS /usr/bin /usr/local/bin /usr/pkg/bin
  )
if(RUBY_EXECUTABLE)
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -e "print RUBY_VERSION.split('.')[0]"
    OUTPUT_VARIABLE _RUBY_MAJOR_
    )

  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -e "print RUBY_VERSION.split('.')[1]"
    OUTPUT_VARIABLE _RUBY_MINOR_
    )

  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['rubyhdrdir'] || RbConfig::CONFIG['archdir']"
    OUTPUT_VARIABLE RUBY_ARCH_DIR
    )
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['arch']"
    OUTPUT_VARIABLE RUBY_ARCH
    )
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['libdir']"
    OUTPUT_VARIABLE RUBY_POSSIBLE_LIB_PATH
    )
  execute_process(
    COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['rubylibdir']"
    OUTPUT_VARIABLE RUBY_RUBY_LIB_PATH
    )
  find_path(RUBY_INCLUDE_DIRS
    NAMES ruby.h
    PATHS ${RUBY_ARCH_DIR}
    )
  set(RUBY_INCLUDE_ARCH "${RUBY_INCLUDE_DIRS}/${RUBY_ARCH}")
  find_library(RUBY_LIB
    NAMES ruby2.2 ruby22 ruby2.1 ruby21 ruby2.0 ruby20 ruby-1.9.3 ruby1.9.3 ruby193 ruby-1.9.2 ruby1.9.2 ruby192 ruby-1.9.1 ruby1.9.1 ruby191 ruby1.9 ruby19 ruby1.8 ruby18 ruby
    PATHS ${RUBY_POSSIBLE_LIB_PATH} ${RUBY_RUBY_LIB_PATH}
    )
  if(RUBY_LIB AND RUBY_INCLUDE_DIRS)
    set(RUBY_FOUND TRUE)
  endif()
  set(RUBY_INCLUDE_DIRS "${RUBY_INCLUDE_DIRS};${RUBY_INCLUDE_ARCH}")
  mark_as_advanced(
    RUBY_INCLUDE_DIRS
    RUBY_LIBRARY_DIRS
    RUBY_LIB
    _RUBY_MAJOR_ _RUBY_MINOR_
    )
endif()
