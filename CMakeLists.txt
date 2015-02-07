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
else(GIT_EXECUTABLE)
  set (VERSION_REL "dev")
endif(GIT_EXECUTABLE)

set (VERSION_MAJOR 0)
set (VERSION_MINOR 3)

set (VERSION "${VERSION_MAJOR}.${VERSION_MINOR}-${VERSION_REL}")


# plugin cmake build path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/ ${CMAKE_MODULE_PATH})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE -Wall -Wextra -Werror-implicit-function-declaration -fstack-protector-all")
if(PREFIX)
        set(CMAKE_INSTALL_PREFIX ${PREFIX} CACHE PATH "Install path prefix" FORCE)
endif(PREFIX)
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,relro")


# scripts supports (if libraries can be found)
option(_C_PLUGIN           "Add C script support"                   ON)
option(_PYTHON_PLUGIN      "Add Python 2.x script support"          ON)
option(_PYTHON3_PLUGIN     "Use Python 3.x script support"          OFF)
option(_LUA_PLUGIN         "Add Lua script support"                 ON)
option(_RUBY18_PLUGIN      "Add Ruby 1.8 script support"            OFF)
option(_RUBY_PLUGIN        "Use Ruby 1.9/2.x script support"        OFF)
option(_PERL_PLUGIN        "Add Perl script support"                ON)
option(_TCL_PLUGIN         "Add Tcl script support"                 ON)
option(DEBUG               "Enable Debug"                           OFF)
option(DEBUG_SSL           "Enable SSL debug"                       OFF)

if (DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_C_FLAGS_DEBUG "-ggdb -DDEBUG -O0")
  set(CMAKE_VERBOSE_MAKEFILE OFF)
else()
  set(CMAKE_BUILD_TYPE None)
  set(CMAKE_C_FLAGS "-O3")
endif(DEBUG)

# main files to compile
set(CORE_FILES
control-server.c control-server.h
core.c core.h
http.c http.h
main.c main.h
socket.c socket.h
ssl.c ssl.h
utils.c utils.h
plugin.c plugin.h
)


# adding cmake directory
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# additional link libs
set(EXTRA_LIBS "-lpthread")


# check for polarssl (*REQUIRED*)
find_package(PolarSSL REQUIRED)
add_definitions(-DHAVE_POLARSSL)
set(EXTRA_LIBS ${EXTRA_LIBS} polarssl)

# prepare compilation
set(ALL_FILES ${CORE_FILES})


## Setup plugin integration

# C
if (_C_PLUGIN)
  find_library(DL_LIBRARY
              NAMES dl
              PATHS /lib /usr/lib /usr/libexec /usr/local/lib /usr/local/libexec
              )
  add_definitions(-DHAVE_DL)
  set(EXTRA_LIBS ${EXTRA_LIBS} dl)
  set(ALL_FILES ${ALL_FILES} plugin-c.c plugin-c.h)
endif(_C_PLUGIN)


# python2
if (_PYTHON_PLUGIN)
  find_package(Python)
  if(PYTHON_FOUND)
     include_directories(${PYTHON_INCLUDE_PATH})
     set(EXTRA_LIBS ${EXTRA_LIBS} ${PYTHON_LIBRARY})
     set(ALL_FILES ${ALL_FILES} plugin-python.c plugin-python.h)
     set(_PYTHON_VERSION_ ${_PYTHON_MAJOR_}.${_PYTHON_MINOR_})
   endif(PYTHON_FOUND)
endif (_PYTHON_PLUGIN)


# lua
if (_LUA_PLUGIN)
  find_package(Lua)
  if(LUA_FOUND)
    include_directories(${LUA_INCLUDE_DIRS})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${LUA_LIBRARIES})
    set(ALL_FILES ${ALL_FILES} plugin-lua.c plugin-lua.h)
  endif(LUA_FOUND)
endif(_LUA_PLUGIN)


# ruby
if (_RUBY_PLUGIN)
  find_package(Ruby)
  if(RUBY_FOUND)
    include_directories(${RUBY_INCLUDE_DIRS})
    set(EXTRA_LIBS ${EXTRA_LIBS} ${RUBY_LIBRARIES})
    set(ALL_FILES ${ALL_FILES} plugin-ruby.c plugin-ruby.h)
    set(_RUBY_VERSION_ ${_RUBY_MAJOR_}.${_RUBY_MINOR_})
  endif(RUBY_FOUND)
endif(_RUBY_PLUGIN)


# perl
if (_PERL_PLUGIN)
  find_package(Perl)
  if(PERL_FOUND)
    include_directories(${PERL_INCLUDE_PATH})
    set (EXTRA_LIBS ${EXTRA_LIBS} ${PERL_LIBRARY})
    set (ALL_FILES ${ALL_FILES} plugin-perl.c plugin-perl.h)
  endif(PERL_FOUND)
endif(_PERL_PLUGIN)


# tcl
if (_TCL_PLUGIN)
  find_package(Tcl)
  if(TCL_FOUND)
    include_directories(${TCL_INCLUDE_PATH})
    set (EXTRA_LIBS ${EXTRA_LIBS} ${TCL_LIBRARY})
    set (ALL_FILES ${ALL_FILES} plugin-tcl.c plugin-tcl.h)
  endif(TCL_FOUND)
endif(_TCL_PLUGIN)


add_definitions(-DHAVE_CONFIG_H)


configure_file ("config.h.in" "config.h")
include_directories (".")

# compile and link
add_executable(proxenet ${ALL_FILES})
target_link_libraries(proxenet ${EXTRA_LIBS})

# install
install (TARGETS proxenet DESTINATION .)
