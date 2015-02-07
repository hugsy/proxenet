if(RUBY_FOUND)
  set(RUBY_FIND_QUIETLY TRUE)
endif()

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_search_module(RUBY ruby-2.1 ruby-2.0 ruby-1.9 ruby-1.8)

  find_program(RUBY_EXECUTABLE
    NAMES ruby1.9.3 ruby193 ruby1.9.2 ruby192 ruby1.9.1 ruby191 ruby1.9 ruby19 ruby1.8 ruby18 ruby
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

    mark_as_advanced(
      _RUBY_MAJOR_ _RUBY_MINOR_
    )
  endif(RUBY_EXECUTABLE)
endif()