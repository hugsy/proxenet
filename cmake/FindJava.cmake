# Java cmake package
#
# Returns
# Java_FOUND
# Java_LIBRARIES
# Java_INCLUDE_DIRS
# Java_VERSION_STRING
#

SET(_JAVA_HINTS $ENV{JAVA_HOME}/bin)

SET(_JAVA_PATHS
  /usr/lib/java/bin
  /usr/share/java/bin
  /usr/local/java/bin
  /usr/local/java/share/bin
  /usr/java/j2sdk1.4.2_04
  /usr/lib/j2sdk1.4-sun/bin
  /usr/java/j2sdk1.4.2_09/bin
  /usr/lib/j2sdk1.5-sun/bin
  /opt/sun-jdk-1.5.0.04/bin
  )

FIND_PROGRAM(Java_JAVA_EXECUTABLE
  NAMES java
  HINTS ${_JAVA_HINTS}
  PATHS ${_JAVA_PATHS}
)

IF(Java_JAVA_EXECUTABLE)
  EXECUTE_PROCESS(COMMAND ${Java_JAVA_EXECUTABLE} -version
    RESULT_VARIABLE res
    OUTPUT_VARIABLE var
    ERROR_VARIABLE var
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)

  IF( res )
    IF(${Java_FIND_REQUIRED})
      MESSAGE( FATAL_ERROR "Error executing java -version" )
    ELSE()
      MESSAGE( STATUS "Warning, could not run java --version")
    ENDIF()

  ELSE( res )
    IF(var MATCHES "java version \"[0-9]+\\.[0-9]+\\.[0-9_.]+[oem-]*\".*")
      STRING( REGEX REPLACE ".* version \"([0-9]+\\.[0-9]+\\.[0-9_.]+)[oem-]*\".*"
        "\\1" Java_VERSION_STRING "${var}" )
    ELSEIF(var MATCHES "java full version \"kaffe-[0-9]+\\.[0-9]+\\.[0-9_]+\".*")
      STRING( REGEX REPLACE "java full version \"kaffe-([0-9]+\\.[0-9]+\\.[0-9_]+).*"
        "\\1" Java_VERSION_STRING "${var}" )
    ELSE()
      IF(NOT Java_FIND_QUIETLY)
        message(WARNING "regex not supported: ${var}. Please report")
      ENDIF(NOT Java_FIND_QUIETLY)
    ENDIF()
    STRING( REGEX REPLACE "([0-9]+).*" "\\1" Java_VERSION_MAJOR "${Java_VERSION_STRING}" )
    STRING( REGEX REPLACE "[0-9]+\\.([0-9]+).*" "\\1" Java_VERSION_MINOR "${Java_VERSION_STRING}" )
    STRING( REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" Java_VERSION_PATCH "${Java_VERSION_STRING}" )
    STRING( REGEX REPLACE "[0-9]+\\.[0-9]+\\.[0-9]+\\_?\\.?([0-9]*)$" "\\1" Java_VERSION_TWEAK "${Java_VERSION_STRING}" )
    if( Java_VERSION_TWEAK STREQUAL "" )
      set(Java_VERSION ${Java_VERSION_MAJOR}.${Java_VERSION_MINOR}.${Java_VERSION_PATCH})
    else( )
      set(Java_VERSION ${Java_VERSION_MAJOR}.${Java_VERSION_MINOR}.${Java_VERSION_PATCH}.${Java_VERSION_TWEAK})
    endif( )

    IF(NOT Java_FIND_QUIETLY)
      MESSAGE( STATUS "Java version ${Java_VERSION} found!" )
    ENDIF(NOT Java_FIND_QUIETLY)

  ENDIF( res )
ENDIF(Java_JAVA_EXECUTABLE)


  if(!$ENV{JAVA_HOME})
    message("Cannot find JAVA_HOME. Please setup the path to the base of the Java JDK to JAVA_HOME before compiling.")
  endif()

  message("-- Found JAVA_HOME as $ENV{JAVA_HOME}")
  set(JAVA_ROOT "$ENV{JAVA_HOME}")
  set(_JAVA_POSSIBLE_INCLUDE_DIRS ${JAVA_ROOT}/include)
  set(_JAVA_POSSIBLE_LIBRARIES_PATH ${JAVA_ROOT}/jre/lib/)

  find_path(Java_INCLUDE_DIRS
    NAMES jni.h
    PATHS ${_JAVA_POSSIBLE_INCLUDE_DIRS}
    )
  if(Java_INCLUDE_DIRS)
    set(Java_INCLUDE_DIRS ${Java_INCLUDE_DIRS} ${Java_INCLUDE_DIRS}/linux)
  endif()

  find_library(Java_LIBRARIES
    NAMES jvm
    PATHS ${_JAVA_POSSIBLE_LIBRARIES_PATH}/amd64/server
    )

include(FindPackageHandleStandardArgs)

if (Java_INCLUDE_DIRS AND Java_LIBRARIES)
  set(Java_FOUND TRUE)
  mark_as_advanced(
    Java_FOUND
    Java_LIBRARIES
    Java_INCLUDE_DIRS
    Java_VERSION_STRING
    Java_VERSION_MAJOR
    Java_VERSION_MINOR
  )
else()
  set(Java_FOUND FALSE)
  set(Java_NOT_FOUND TRUE)
endif()
