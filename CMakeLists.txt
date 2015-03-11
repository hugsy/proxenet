################################################################################
#
# proxenet build options for cmake
#
#

cmake_minimum_required(VERSION 2.6)
project(proxenet C)

# extra variables
set (PROGNAME proxenet)
set (AUTHOR hugsy)
set (LICENSE GPLv2)

find_program(GIT_EXECUTABLE
  NAMES git
  PATHS /usr/bin /usr/local/bin)

if(GIT_EXECUTABLE)
  execute_process( COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE GIT_RELEASE_BRANCH)
  execute_process( COMMAND ${GIT_EXECUTABLE} log -n 1 --pretty=format:%t
    OUTPUT_VARIABLE GIT_RELEASE_COMMIT)
  STRING(STRIP "${GIT_RELEASE_BRANCH}" GIT_RELEASE_BRANCH)
  STRING(STRIP "${GIT_RELEASE_COMMIT}" GIT_RELEASE_COMMIT)
  set (VERSION_REL "${GIT_RELEASE_BRANCH}:${GIT_RELEASE_COMMIT}")
else()
  set (VERSION_REL "stable")
endif()

set (VERSION_MAJOR 0)
set (VERSION_MINOR 3)

set (VERSION "${VERSION_MAJOR}.${VERSION_MINOR}-${VERSION_REL}")


# plugin cmake build path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/ ${CMAKE_MODULE_PATH})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror-implicit-function-declaration")
if(PREFIX)
        set(CMAKE_INSTALL_PREFIX ${PREFIX} CACHE PATH "Install path prefix" FORCE)
endif(PREFIX)


# scripts supports (if libraries can be found)
option(USE_C_PLUGIN           "Add C script support"                   ON)
option(USE_PYTHON_PLUGIN      "Add Python 2.x script support"          ON)
option(USE_PYTHON3_PLUGIN     "Use Python 3.x script support"          OFF)
option(USE_LUA_PLUGIN         "Add Lua script support"                 ON)
option(USE_RUBY_PLUGIN        "Add Ruby2 script support"               ON)
option(USE_PERL_PLUGIN        "Add Perl script support"                ON)
option(USE_TCL_PLUGIN         "Add Tcl script support"                 ON)
option(USE_JAVA_PLUGIN        "Add Java script support"                ON)

option(DEBUG                  "Enable Debug"                           OFF)
option(DEBUG_SSL              "Enable SSL debug"                       OFF)
option(DEBUG_LEAK             "Enable memleak checks"                  ON)

if (DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_C_FLAGS_DEBUG "-ggdb -DDEBUG -O0")
  set(CMAKE_VERBOSE_MAKEFILE ON)

  if(DEBUG_LEAK)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
  endif(DEBUG_LEAK)

  if (DEBUG_SSL)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG_SSL")
  endif(DEBUG_SSL)

else()
  set(CMAKE_BUILD_TYPE None)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fstack-protector-all -fPIE -fPIC")
  set(CMAKE_VERBOSE_MAKEFILE OFF)
  set(CMAKE_LINK_HARDEN "-Wl,-z,relro,-z,now") # Full RelRO
  set(CMAKE_LINK_HARDEN ${CMAKE_LINK_HARDEN} -pie) # PIE
endif(DEBUG)


# main files to compile
set(CORE_FILES
control-server.c control-server.h
core.c core.h
http.c http.h
main.c main.h
socket.c socket.h
ssl.c ssl.h
minica.c minica.h
utils.c utils.h
plugin.c plugin.h
)


# adding cmake directory
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# additional link libs
set(EXTRA_LIBS "-lpthread")


# check for polarssl (*REQUIRED*)
find_package(PolarSSL REQUIRED)
if(POLARSSL_FOUND  AND  POLARSSL_INCLUDE_DIR  AND  POLARSSL_LIBRARIES)
  add_definitions(-DHAVE_POLARSSL)
  set(EXTRA_LIBS ${EXTRA_LIBS} polarssl)
  set(_POLARSSL_VERSION_ ${POLARSSL_VERSION})
  message("-- PolarSSL ${POLARSSL_VERSION} found!")
else()
  return()
endif()

# prepare compilation
set(ALL_FILES ${CORE_FILES})


## Setup plugin integration

# C
if (USE_C_PLUGIN)
  find_library(DL_FOUND
              NAMES dl
              PATHS /lib /usr/lib /usr/libexec /usr/local/lib /usr/local/libexec)
  if(DL_FOUND)
    add_definitions(-DHAVE_DL)
    set(EXTRA_LIBS ${EXTRA_LIBS} dl)
    set(ALL_FILES ${ALL_FILES} plugin-c.c plugin-c.h)
    set(_C_VERSION_ "C")
    set(_C_PLUGIN 1)
  endif()
endif(USE_C_PLUGIN)


# python2
if (USE_PYTHON_PLUGIN)
  find_package(Python)
  if(PYTHON_FOUND)
     include_directories(${PYTHON_INCLUDE_PATH})
     set(EXTRA_LIBS ${EXTRA_LIBS} ${PYTHON_LIBRARY})
     set(ALL_FILES ${ALL_FILES} plugin-python.c plugin-python.h)
     set(_PYTHON_MAJOR_ ${PYTHON_VERSION_MAJOR})
     set(_PYTHON_MINOR_ ${PYTHON_VERSION_MINOR})
     set(_PYTHON_VERSION_ ${PYTHON_VERSION})
     set(_PYTHON_PLUGIN 1)
   endif(PYTHON_FOUND)
endif(USE_PYTHON_PLUGIN)


# lua
if (USE_LUA_PLUGIN)
  find_package(Lua)
  if(LUA_FOUND)
    include_directories(${LUA_INCLUDE_DIRS})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${LUA_LIBRARIES})
    set(ALL_FILES ${ALL_FILES} plugin-lua.c plugin-lua.h)
    set(_LUA_VERSION_ ${LUA_VERSION})
    set(_LUA_PLUGIN 1)
  endif(LUA_FOUND)
endif(USE_LUA_PLUGIN)


# ruby
if (USE_RUBY_PLUGIN)
  find_package(Ruby)
  if(RUBY_FOUND)
    include_directories(${RUBY_INCLUDE_DIRS})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${RUBY_LIBRARIES})
    set(ALL_FILES ${ALL_FILES} plugin-ruby.c plugin-ruby.h)
    set(_RUBY_MAJOR_ ${RUBY_VERSION_MAJOR})
    set(_RUBY_MINOR_ ${RUBY_VERSION_MINOR})
    set(_RUBY_VERSION_ ${RUBY_VERSION})
    set(_RUBY_PLUGIN 1)
  endif(RUBY_FOUND)
endif(USE_RUBY_PLUGIN)


# perl
if (USE_PERL_PLUGIN)
  find_package(Perl)
  if(PERL_FOUND)
    include_directories(${PERL_INCLUDE_PATH})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${PERL_LIBRARY})
    set(ALL_FILES ${ALL_FILES} plugin-perl.c plugin-perl.h)
    set(_PERL_VERSION_ ${PERL_VERSION})
    set(_PERL_PLUGIN 1)
  endif(PERL_FOUND)
endif(USE_PERL_PLUGIN)


# tcl
if (USE_TCL_PLUGIN)
  find_package(Tcl)
  if(TCL_FOUND)
    include_directories(${TCL_INCLUDE_PATH})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${TCL_LIBRARY})
    set(ALL_FILES ${ALL_FILES} plugin-tcl.c plugin-tcl.h)
    set(_TCL_VERSION_ ${TCL_VERSION})
    set(_TCL_PLUGIN 1)
  endif(TCL_FOUND)
endif(USE_TCL_PLUGIN)

# java
if (USE_JAVA_PLUGIN)
  find_package(Java)
  if(Java_FOUND)
    include_directories(${Java_INCLUDE_DIRS})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${Java_LIBRARIES})
    set(ALL_FILES ${ALL_FILES} plugin-java.c plugin-java.h)
    set(_JAVA_VERSION_ ${Java_VERSION_STRING})
    set(_JAVA_MAJOR_ ${Java_VERSION_MAJOR})
    set(_JAVA_MINOR_ ${Java_VERSION_MINOR})
    set(_JAVA_PLUGIN 1)
  endif(Java_FOUND)
endif(USE_JAVA_PLUGIN)


# generate config file
add_definitions(-DHAVE_CONFIG_H)
configure_file ("config.h.in" "config.h")
include_directories (".")


# wrap-up message
message("--------------------------------------------------------------------------")
message("${PROGNAME} will be compiled with support for:")
if(DL_FOUND)
  message("- C plugin")
endif()
if(PYTHON_FOUND)
  message("- Python plugin: ${_PYTHON_VERSION_}")
endif()
if(RUBY_FOUND)
  message("- Ruby plugin: ${_RUBY_VERSION_}")
endif()
if(PERL_FOUND)
  message("- Perl plugin: ${_PERL_VERSION_}")
endif()
if(LUA_FOUND)
  message("- Lua plugin: ${_LUA_VERSION_}")
endif()
if(TCL_FOUND)
  message("- Tcl plugin: ${_TCL_VERSION_}")
endif()
if(Java_FOUND)
  message("- Java plugin: ${_JAVA_VERSION_}")
endif()
message("--------------------------------------------------------------------------")


# compile and link
add_executable(proxenet ${ALL_FILES})
target_link_libraries(proxenet ${EXTRA_LIBS} ${CMAKE_LINK_HARDEN})

# install
install (TARGETS proxenet DESTINATION .)
