project (proxenet C)
cmake_minimum_required (VERSION 2.8)

set(PROXENET_VERSION_MAJOR 0)
set(PROXENET_VERSION_MINOR 1)

set(PACKAGE_AUTHOR "hugsy < @__hugsy__>" )
set(PACKAGE_LICENSE "GPLv2" )
set(PACKAGE_VERSION "${PROXENET_VERSION_MAJOR}.${PROXENET_VERSION_MINOR}-git")

set(PACKAGE_NAME proxenet)
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/hugsy/proxenet/pulls")

configure_file ("${PROJECT_BINARY_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")

## command line options 
option(PROXENET_DEBUG "Enable debug messages" OFF)
if ( PROXENET_DEBUG )
   add_definitions( -DDEBUG )
endif()

add_definitions("-O2 -Wall -pthread")
link_directories(${PROXENET_BINARY_DIR}/proxenet)


## source files
set(SRC
	# headers
	control-server.h
	core.h
	http.h
	main.h
	plugin-c.h
	plugin.h
	plugin-lua.h
	plugin-perl.h
	plugin-python.h
	plugin-ruby.h
	socket.h
	ssl.h
	utils.h

	# sources
	control-server.c
	core.c
	http.c
	main.c
	plugin.c
	plugin-c.c
	plugin-lua.c
	plugin-perl.c
	plugin-python.c
	plugin-ruby.c
	socket.c
	ssl.c
	utils.c
)

add_executable(proxenet ${SRC})

## mandatory packages
find_library(DL_LIBRARY NAMES dl dlsym)
mark_as_advanced(DL_LIBRARY)
if (DL_LIBRARY)
   target_link_libraries(proxenet dl)
endif ()

find_library(PTHREAD_LIBRARY NAMES pthread pthread_create)
mark_as_advanced(PTHREAD_LIBRARY)
if (PTHREAD_LIBRARY)
   target_link_libraries(proxenet pthread)
endif ()

find_library(POLARSSL_LIBRARY polarssl)
mark_as_advanced(POLARSSL_LIBRARY)
if (POLARSSL_LIBRARY)
   target_link_libraries(proxenet ${POLARSSL_LIBRARY})
   include_directories( ${POLARSSL_INCLUDE_DIRS} )
else()
   message(FATAL_ERROR "Missing polarssl libraries")
endif()


find_package(PkgConfig)


## plugins packages 
#include(LibFindMacros)

### Python2.7
#libfind_package(PYTHON2 Python2.7)
#libfind_pkg_check_modules(PYTHON2_PKGCONF Python2.7)

#find_path(PYTHON2_INCLUDE_DIR   NAMES Python.h  PATHS ${PYTHON2_PKGCONF_INCLUDE_DIRS} )
#find_library(PYTHON2_LIBRARY	NAMES Python2.7 PATHS ${PYTHON2_PKGCONF_LIBRARY_DIRS} )

#set(INC  ${INC}  ${PYTHON2_INCLUDE_DIRS})
#set(LIBS ${LIBS} ${PYTHON2_LIBRARY})

pkg_check_modules(PC_LIBPYTHON2 QUIET python2.7)
set(LIBPYTHON2_DEFINITIONS ${PC_LIBPYTHON_CFLAGS_OTHER})

include_directories( ${INC} )

target_link_libraries(proxenet ${LIBS})
