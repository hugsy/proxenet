#!/bin/sh

branch="`git rev-parse --abbrev-ref HEAD`"
ver="`git log -n 1 --pretty=format:'git-%t-%at'`"

if [ "$branch" = "dev" ]; then
    echo "Building for '$branch'"
    make clean && make VERSION_REL=$(ver) DEBUG=1 CC=cc NO_PYTHON=0 NO_PERL=1 NO_LUA=1 NO_RUBY=1 NO_C=0

elif [ "$branch" = "master" ]; then
    echo "Building for '$branch'"
    make clean && make DEBUG=1 CC=cc VERSION_REL=$ver

else
    echo "Unknown branch"
fi
