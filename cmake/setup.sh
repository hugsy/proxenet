#!/bin/bash

orig=$1
dest=$2
mandir=$3
progname="proxenet"

echo "* Setup ${progname} environment"
echo "** Install man page"
gzip --to-stdout ${orig}/${progname}.1 > ${mandir}/${progname}.1.gz

echo "** Building proxenet-plugins tree"
mkdir -p ${dest}/proxenet-plugins/autoload

echo "** Building SSL CA"
cp -r ${orig}/keys ${dest}
chmod a+rx ${dest}/keys
make -C ${dest}/keys

echo "** Copying control clients"
cp control-web.py control-client.py ${dest}/bin

echo '#!/bin/bash' > ${dest}/${progname}
echo "cd ${dest};./bin/proxenet \$@;cd -" >> ${dest}/${progname}
chmod a+x ${dest}/${progname}
chmod a+r -R ${dest}

echo "* Success"
echo "! Run ${dest}/${progname} to start"
echo "! The plugin directory is at '${dest}/proxenet-plugins'"

exit 0
