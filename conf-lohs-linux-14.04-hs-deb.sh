#!/bin/bash

#get the branch point
. ../conf-get-branch-point-lo52.sh

#google credentials
. ../gdrive-lohs-credential.shinc

./autogen.sh \
--with-distro=LibreOfficeLinuxHS \
--with-vendor="Acca Esse, I-10067" \
--with-gdrive-client-id="$GDRIVE_CLIENT_ID" \
--with-gdrive-client-secret="$GDRIVE_CLIENT_SECRET" \
--with-external-tar=/srv5/git/LO/externals \
--disable-kde4 \
--disable-gio \
 \
--with-package-format="deb" \
--with-lang='en-US it fr de pt-BR' \
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g')$(echo :$(git log -n1 --format=%h)) based on $ROOT_BRANCH:$BRANCH_POINT2" \

echo "Cleaning scp2..."
make scp2.clean 2>&1 > /dev/null
cat autogen.lastrun

