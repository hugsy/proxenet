#!/bin/sh

DEBUG=0  # 1:proxenet debug output   //   2:proxenet+ssl debug output

branch="`git rev-parse --abbrev-ref HEAD`"
ver="`git log -n 1 --pretty=format:'git-%t-%at'`"

export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:`pwd`/pc

if [ "$branch" = "dev" ]; then
    echo "Building for '$branch'"
    make clean && make VERSION_REL=$(ver) DEBUG=1 CC=cc NO_PYTHON=0 NO_PERL=1 NO_LUA=1 NO_RUBY=1 NO_C=0 NO_TCL=1

elif [ "$branch" = "master" ]; then
    echo "Building for '$branch'"
    make clean && make DEBUG=${DEBUG} CC=cc VERSION_REL=$ver

else
    echo "Unknown branch"
fi
