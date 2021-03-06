if(PERL_FOUND)
   set(PERL_FIND_QUIETLY TRUE)
endif()

find_program(PERL_EXECUTABLE
  NAMES perl perl5
  PATHS /usr/bin /usr/local/bin /usr/pkg/bin
  )

if(PERL_EXECUTABLE)

  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MConfig -e "print \"\$Config{archlibexp}/CORE\""
    OUTPUT_VARIABLE PERL_INTERNAL_DIR
    )

  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MExtUtils::Embed -e ccopts
    OUTPUT_VARIABLE PERL_CFLAGS
    )

  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MExtUtils::Embed -e ldopts
    OUTPUT_VARIABLE PERL_LFLAGS
    )

  execute_process(
    COMMAND ${PERL_EXECUTABLE} -MConfig -e "print \"\$Config{version}\""
    OUTPUT_VARIABLE PERL_VERSION
    )

  # remove the new lines from the output by replacing them with empty strings
  string(REPLACE "\n" "" PERL_INTERNAL_DIR "${PERL_INTERNAL_DIR}")
  string(REPLACE "\n" "" PERL_CFLAGS "${PERL_CFLAGS}")
  string(REPLACE "\n" "" PERL_LFLAGS "${PERL_LFLAGS}")

  find_path(PERL_INCLUDE_PATH
    NAMES perl.h
    PATHS ${PERL_INTERNAL_DIR}
    )

  find_library(PERL_LIBRARY
    NAMES perl
    PATHS /usr/lib /usr/local/lib /usr/pkg/lib ${PERL_INTERNAL_DIR}
    )

  if(PERL_LIBRARY AND PERL_INCLUDE_PATH)
    set(PERL_FOUND TRUE)
  endif()

  mark_as_advanced(
    PERL_EXECUTABLE
    PERL_INCLUDE_PATH
    PERL_LIBRARY
    PERL_CFLAGS
    PERL_LFLAGS
    PERL_VERSION
    )
endif()