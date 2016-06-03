#!/bin/bash

set -e

orig=$1
dest=$2
progname="proxenet"

KEYS_DIR="${dest}/keys"
AUTOLOAD_DIR="${dest}/proxenet-plugins/autoload"

echo "* Setup ${progname} environment"

if [ ! -d ${AUTOLOAD_DIR} ]; then
    echo "!! Would you like to git-clone the proxenet-plugins from GitHub (y/N)? "
    read i
    if [ ${i} == "y" ] || [ ${i} == "Y"]; then
        echo "** Cloning repository in '${dest}'"
        git clone https://github.com/hugsy/proxenet-plugins.git ${dest}/proxenet-plugins
    fi

    echo "** Building proxenet-plugins tree"
    mkdir -p ${AUTOLOAD_DIR}

else
    echo "** Plugin tree valid is already created"
fi

if [ ! -d ${KEYS_DIR} ]; then
    echo "** Building SSL CA in '${KEYS_DIR}'"
    cp -r ${orig}/keys ${dest}
    chmod a+rx ${KEYS_DIR}
    make -C ${KEYS_DIR} clean keys
else
    echo "** CA already created"
fi

echo "* Success"
echo "! Run ${dest}/${progname} to start"
echo "! The plugin directory is at '${dest}/proxenet-plugins'"

exit 0
