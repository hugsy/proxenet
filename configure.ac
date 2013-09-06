# 
# This file is part of proxenet project
#
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([proxenet], [0.1], [proxenet@pyc.li])
AC_CONFIG_SRCDIR([src])


AM_INIT_AUTOMAKE

AC_DEFINE(DEBUG)
# AC_DEFINE(DEBUG_SSL)

# Checks for programs.
AC_PROG_CC(cc clang gcc)

# Checks for core libraries.
AC_CHECK_LIB([polarssl], [x509_free])

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h strings.h sys/param.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset realpath socket strdup strerror strstr])
AM_PROG_LIBTOOL

# Checks for plugins libraries.
AC_ARG_ENABLE(python,    [ --disable-python  turn off Python script plugin (default=compiled if found)], enable_python=$enableval, enable_python=yes)
AC_ARG_ENABLE(python3,   [ --enable-python3  use Python 3.x instead of Python 2.x (default=off)], enable_python3=$enableval, enable_python3=no)
AC_ARG_ENABLE(c,         [ --disable-c       turn off C script plugin (default=on)],enable_lua=$enableval,enable_c=yes)
AC_ARG_ENABLE(lua,       [ --disable-lua     turn off Lua script plugin (default=compiled if found)],enable_lua=$enableval,enable_lua=yes)
AC_ARG_ENABLE(ruby,      [ --enable-ruby     turn on Ruby script plugin (default=off)],enable_ruby=$enableval,enable_ruby=no)
AC_ARG_ENABLE(perl,      [ --enable-perl     turn on Ruby script plugin (default=off)],enable_ruby=$enableval,enable_perl=no)


# ------------------------------[ Python ]------------------------------

PYTHON_VERSION=

if test "x$enable_python" = "xyes" ; then
    if test "x$enable_python3" = "xyes" ; then
        AC_PATH_PROGS(PYTHON, python3.4 python3.3 python3.2 python3.1 python3.0 python3 python2.7 python2.6 python2.5 python2 python)
    else
        AC_PATH_PROGS(PYTHON, python2.7 python2.6 python2.5 python2 python)
    fi
    if test -z $PYTHON ; then
        AC_MSG_WARN([
*** Python must be installed on your system but python interpreter couldn't be found in path.
*** Please check that python is in path, or install it with your software package manager.
*** proxenet will be built without Python support.])
        enable_python="no"
        not_found="$not_found python"
    else
        PYTHON_SYSPREFIX=`$PYTHON -c 'import sys; sys.stdout.write("%s" % sys.prefix)'`
        PYTHON_VERSION=`$PYTHON -c 'import sys; sys.stdout.write(sys.version[[:3]])'`
        PYTHON_INCLUDE=`$PYTHON -c "import sys, distutils.sysconfig, string; sys.stdout.write(distutils.sysconfig.get_config_var('INCLUDEPY'))"`
        PYTHON_OLD_VERSION=`$PYTHON -c "import sys; sys.stdout.write(str(sys.version_info < (2,5)))"`
        if test "x$PYTHON_OLD_VERSION" = "xTrue" ; then
            AC_MSG_WARN([
*** Python >= 2.5 is needed to build Python plugin, version found: $PYTHON_VERSION.
*** proxenet will be built without Python support.])
            enable_python="no"
            not_found="$not_found python"
        else
            AC_MSG_CHECKING(for Python header files)
            if test -r "$PYTHON_INCLUDE/Python.h"; then
                PYTHON_CFLAGS="-I$PYTHON_INCLUDE"
                AC_MSG_RESULT(found)
                PYTHON_LIB=`$PYTHON -c "import sys, distutils.sysconfig; sys.stdout.write(distutils.sysconfig.get_config_var('LIBPL'))"`
                PYTHON_LFLAGS="-lpython$PYTHON_VERSION "`$PYTHON -c "import sys, distutils.sysconfig; sys.stdout.write(distutils.sysconfig.get_config_var('LIBS')+' '+distutils.sysconfig.get_config_var('SYSLIBS')+' '+distutils.sysconfig.get_config_var('LINKFORSHARED'))"`
                AC_MSG_CHECKING(for Python library)
                if test -r "$PYTHON_LIB/libpython$PYTHON_VERSION.so"; then
                   PYTHON_LFLAGS="-L$PYTHON_LIB $PYTHON_LFLAGS"
                   AC_MSG_RESULT(found)
                elif test -r "$PYTHON_LIB/libpython$PYTHON_VERSION.a"; then
                   PYTHON_LFLAGS="-L$PYTHON_LIB $PYTHON_LFLAGS"
                   AC_MSG_RESULT(found)
                elif test -r "$PYTHON_SYSPREFIX/lib/libpython$PYTHON_VERSION.so"; then
                   PYTHON_LFLAGS="-L$PYTHON_SYSPREFIX/lib/ $PYTHON_LFLAGS"
                   AC_MSG_RESULT(found)
                else
                    AC_MSG_WARN([
*** Python library couldn't be found on your system.
*** Try to install it with your software package manager.
*** proxenet will be built without Python support.])
                    enable_python="no"
                    not_found="$not_found python"
                fi
            else
                AC_MSG_WARN([
*** Python header files couldn't be found on your system.
*** Try to install them with your software package manager.
*** proxenet will be built without Python support.])
                enable_python="no"
                not_found="$not_found python"
            fi
        fi
    fi
else
    not_asked="$not_asked python"
fi


if test "x$enable_python" = "xyes" ; then
   AC_SUBST(PYTHON_CFLAGS)   
   AC_SUBST(PYTHON_LFLAGS)
   AC_DEFINE(_PYTHON_PLUGIN)
   
   if test "x$enable_python3" = "xyes" ; then
      AC_DEFINE(_PYTHON3_)
   else
      AC_DEFINE(_PYTHON2_)
   fi
fi


# ------------------------------[ C ]------------------------------

C_LFLAGS=

if test "x$enable_c" = "xyes" ; then
   AC_CHECK_FUNCS(dlopen, LIBDL_FOUND=yes, LIBDL_FOUND=no)
     if test "$LIBDL_FOUND" != "yes"; then
     	AC_CHECK_LIB(dl, dlopen, [LIBDL_FOUND=yes; C_LFLAGS=-ldl], LIBDL_FOUND=no)
     fi
     
     if test "$LIBDL_FOUND" != "yes"; then
     	AC_MSG_ERROR([
*** "dl" library (dynamic library loader) couldn't be found on your system.
*** Try to install it with your software package manager.])
    else
	enable_c="no"
    fi
fi

if test "x$enable_c" = "xyes" ; then
   AC_SUBST(C_CFLAGS)   
   AC_SUBST(C_LFLAGS)
   AC_DEFINE(_C_PLUGIN)
fi


# ------------------------------[ Lua ]------------------------------

LUA_VERSION=

if test "x$enable_lua" = "xyes" ; then
    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"

    LUA_CFLAGS=""
    LUA_LFLAGS=""

    if test -n "$lua_inc"; then
       CFLAGS="$CFLAGS -I$lua_inc"
       CPPFLAGS="$CPPFLAGS -I$lua_inc"
    fi
    if test -n "$lua_lib"; then
       LDFLAGS="$LDFLAGS -L$lua_lib"
    fi

    if test "x$LUA_CFLAGS" = "x" -o "x$LUA_LFLAGS" = "x" ; then
        PKGCONFIG=""
        AC_CHECK_PROGS(PKGCONFIG, pkg-config)
        if test "x$PKGCONFIG" != "x"; then
            AC_MSG_CHECKING(for Lua headers and librairies with pkg-config)
            echo
            for l in "$lua_suffix" "" "51" "5.1" "52" "5.2" ; do
                pkgconfig_lua_found=`$PKGCONFIG --exists lua$l 2>/dev/null`
                if test "x$?" = "x0" ; then
                    LUA_VERSION=`$PKGCONFIG --modversion lua$l`
                    LUA_CFLAGS="$LUA_CFLAGS "`$PKGCONFIG --cflags lua$l`
                    LUA_LFLAGS="$LUA_LFLAGS "`$PKGCONFIG --libs lua$l`

                    pkgconfig_lualib_found=`$PKGCONFIG --exists lualib$l 2>/dev/null`
                    if test  "x$?" = "x0"; then
                        LUA_CFLAGS="$LUA_CFLAGS "`$PKGCONFIG --cflags lualib$l`
                        LUA_LFLAGS="$LUA_LFLAGS "`$PKGCONFIG --libs lualib$l`
                    fi
                    break
                fi
            done
        fi
    fi

    if test "x$LUA_CFLAGS" = "x" -o "x$LUA_LFLAGS" = "x" ; then
        LUACONFIG=""
        AC_CHECK_PROGS(LUACONFIG, lua-config lua-config51 lua-config5.1 lua-config50 lua-config5.0)
        if test "x$LUACONFIG" != "x" ; then
            AC_MSG_CHECKING(for Lua headers and librairies with lua-config)
            echo
            LUA_CFLAGS=`$LUACONFIG --include`
            LUA_LFLAGS=`$LUACONFIG --libs`
            LUA_VERSION="5.0.x"
        fi
    fi

    if test "x$LUA_CFLAGS" = "x" -o "x$LUA_LFLAGS" = "x" ; then
        AC_MSG_CHECKING(for Lua headers and librairies)
        echo
        AC_CHECK_HEADER(lua.h,ac_found_lua_header="yes",ac_found_lua_header="no")
        AC_CHECK_HEADER(lualib.h,ac_found_liblua_header="yes",ac_found_liblua_header="no")
        if test "x$ac_found_lua_header" = "xyes" -a "x$ac_found_liblua_header" = "xyes"; then
            LUA_CFLAGS="$CFLAGS"
        fi
        for l in "$lua_suffix" "" "51" "5.1" "52" "5.2" ; do
            AC_CHECK_LIB(lua$l,lua_call,ac_found_lua_lib="yes",ac_found_lua_lib="no")
            if test "x$ac_found_lua_lib" = "xyes" ; then
                LUA_VERSION=">=5.1.0"

                LUA_LFLAGS="$LDFLAGS -llua$l -lm"

                ac2_save_LDFLAGS="$LDFLAGS"
                LDFLAGS="$LDFLAGS -llua$l -lm"

                if echo "$host_os" | grep "^linux" 1>/dev/null 2>&1 ; then
                    LDFLAGS="$LDFLAGS -ldl"
                fi

                AC_CHECK_LIB(lualib$l,luaL_openlib,ac_found_liblua_lib="yes",ac_found_liblua_lib="no")
                if test "x$ac_found_liblua_lib" = "xyes" ; then
                    LUA_VERSION="5.0.x"

                    LUA_LFLAGS="$LUA_LFLAGS -llualib$l"

                    if echo "$host_os" | grep "^linux" 1>/dev/null 2>&1 ; then
                        LUA_LFLAGS="$LUA_LFLAGS -ldl"
                    fi
                fi

                LDFLAGS="$ac2_save_LDFLAGS"
                break
            fi
        done
    fi

    AC_MSG_CHECKING(for Lua compiling and linking)
    LUA_TEST=`LT=luatest.c ; echo "#include <lua.h>" > $LT;  echo "#include <lualib.h>" >> $LT; echo "int main() { luaopen_base((lua_State *)lua_open()); return 0; }" >> $LT ; $CC -Wall $LT -o $LT.out $LUA_CFLAGS $LUA_LFLAGS $CFLAGS $LDFLAGS 1>/dev/null 2>&1 ; echo $?; rm -f $LT $LT.out 1>/dev/null 2>&1`
    
    if test "x$LUA_TEST" != "x0" ; then
        AC_MSG_RESULT(no)
        AC_MSG_WARN([
*** Lua (>=5.0) headers and/or librairies couldn't be found on your system.
*** Try to install liblua, liblualib and liblua-dev with your software package manager.
*** proxenet will be built without Lua support.])
        enable_lua="no"
        not_found="$not_found lua"
    else
        AC_MSG_RESULT(yes)
    fi

    CFLAGS="$ac_save_CFLAGS"
    CPPFLAGS="$ac_save_CPPFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
else
    not_asked="$not_asked lua"
fi

if test "x$enable_lua" = "xyes" ; then
    AC_SUBST(LUA_CFLAGS)
    AC_SUBST(LUA_LFLAGS)
    AC_DEFINE(_LUA_PLUGIN)
fi


# ------------------------------[ Ruby ]------------------------------

RUBY_VERSION=

if test "x$enable_ruby" = "xyes" ; then
    AC_PATH_PROGS(RUBY, ruby1.9.3 ruby1.9.2 ruby1.9.1 ruby1.9 ruby1.8 ruby)
    if test -z $RUBY ; then
        AC_MSG_WARN([
*** Ruby must be installed on your system but ruby interpreter couldn't be found in path.
*** Please check that ruby is in path, or install it with your software package manager.
***  will be built without Ruby support.])
        enable_ruby="no"
        not_found="$not_found ruby"
    else
        RUBY_VERSION=`$RUBY -rrbconfig -e "puts RbConfig::CONFIG[['ruby_version']]"`
        if test "$RUBY_VERSION" = "1.9.0"; then
            AC_MSG_WARN([
*** Ruby header files have been found, but they're of the version 1.9.0.
*** Ruby 1.9.0 is an unstable release and should not be used in production.
*** Please install Ruby 1.8.x (>= 1.8.6) or >= 1.9.1.
*** proxenet will be built without Ruby support.])
            enable_ruby="no"
            not_found="$not_found ruby"
        else
            RUBY_INCLUDE=`$RUBY -rrbconfig -e "puts RbConfig::CONFIG[['rubyhdrdir']] || RbConfig::CONFIG[['archdir']]"`
            RUBY_ARCH=`$RUBY -rrbconfig -e 'print RbConfig::CONFIG[["arch"]]'`
            AC_MSG_CHECKING(for Ruby header files)
            if test -d "$RUBY_INCLUDE/"; then
                M_RUBY_VERSION=`$RUBY -rrbconfig -e "puts RbConfig::CONFIG[['ruby_version']].gsub(/\./, '')[[0,3]]"`
                RUBY_CFLAGS="-I$RUBY_INCLUDE/ -I$RUBY_INCLUDE/$RUBY_ARCH -DRUBY_VERSION=$M_RUBY_VERSION"
            else
                AC_MSG_WARN([
*** Ruby header files couldn't be found on your system.
*** Try to install them with your software package manager.
*** proxenet will be built without Ruby support.])
                enable_ruby="no"
                not_found="$not_found ruby"
            fi
            AC_MSG_RESULT(found)
            RUBY_LFLAGS=`$RUBY -rrbconfig -e "puts RbConfig::CONFIG[['LIBRUBYARG_SHARED']]"`
        fi
    fi
else
    not_asked="$not_asked ruby"
fi

if test "x$enable_ruby" = "xyes" ; then
    AC_SUBST(RUBY_CFLAGS)
    AC_SUBST(RUBY_LFLAGS)
    AC_DEFINE(_RUBY_PLUGIN)
fi


# ------------------------------[ Perl ]------------------------------  

PERL_VERSION=

if test "x$enable_perl" = "xyes" ; then
    AC_PATH_PROGS(PERL, perl perl5)
    if test -z $PERL ; then
        AC_MSG_WARN([
*** Perl must be installed on your system but perl interpreter couldn't be found in path.
*** Please check that perl is in path, or install it with your software package manager.
*** proxenet will be built without Perl support.])
        enable_perl="no"
        not_found="$not_found perl"
    else
        PERL_VERSION=`perl -V:version | sed "s/version='\(.*\)';/\1/"`
        AC_MSG_CHECKING(for Perl headers files)

        PERL_HEADER_TEST=`PT=perltest.c ; echo "#include <EXTERN.h>" > $PT;  echo "#include <perl.h>" >> $PT;  echo "#include <XSUB.h>" >> $PT ;  echo "int main() { return 0; }" >> $PT ; $CC -Wall $PT -o $PT.out $($PERL -MExtUtils::Embed -e ccopts) 1>/dev/null 2>&1; echo $?; rm -f $PT $PT.out 1>/dev/null 2>&1`

        if test "x$PERL_HEADER_TEST" = "x0" ; then
            PERL_CFLAGS=`$PERL -MExtUtils::Embed -e ccopts`
            AC_MSG_RESULT(found)
            AC_MSG_CHECKING(for Perl library)
            PERL_LIB_TEST=`PT=perltest.c ; echo "int main() { return 0; }" > $PT ; $CC -Wall $PT -o $PT.out $($PERL -MExtUtils::Embed -e ldopts) 1>/dev/null 2>&1; echo $?; rm -f $PT $PT.out 1>/dev/null 2>&1`
            if test "x$PERL_LIB_TEST" = "x0" ; then
                PERL_LFLAGS=`$PERL -MExtUtils::Embed -e ldopts`
                AC_MSG_RESULT(found)
            else
                AC_MSG_WARN([
*** Perl library couldn't be found on your system.
*** Try to install it with your software package manager.
*** proxenet will be built without Perl support.])
                enable_perl="no"
                not_found="$not_found perl"
            fi
        else
            AC_MSG_WARN([
*** Perl headers couldn't be found on your system.
*** Try to install it with your software package manager.
*** proxenet will be built without Perl support.])
            enable_perl="no"
            not_found="$not_found perl"
        fi
    fi
else
    not_asked="$not_asked perl"
fi

if test "x$enable_perl" = "xyes" ; then
    AC_SUBST(PERL_CFLAGS)
    AC_SUBST(PERL_LFLAGS)
    AC_DEFINE(_PERL_PLUGIN)
fi

# ------------------------------[ Generating Makefiles ]------------------------------  

AC_OUTPUT([Makefile src/Makefile man/Makefile])
