#!/bin/bash

# configuration for windows

#get the branch point
. ../conf-get-branch-point-lo52.sh

. ../gdrive-lohs-credential.shinc

./autogen.sh \
--with-distro=LibreOfficeWin64 \
--enable-release-build \
--with-vendor="Acca Esse, I-10067" \
--with-gdrive-client-id="$GDRIVE_CLIENT_ID" \
--with-gdrive-client-secret="$GDRIVE_CLIENT_SECRET" \
--with-lang='en-US it fr de pt-BR' \
\
--with-external-tar=/cygdrive/d/lode/ext_tar \
--enable-pch \
--disable-ccache \
--with-visual-studio=2013 \
\
--disable-online-update \
--with-help \
--with-myspell-dicts \
--disable-dependency-tracking \
--disable-mergelibs \
\
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g')$(echo :$(git log -n1 --format=%h)) based on $ROOT_BRANCH:$BRANCH_POINT2" \

echo "Cleaning scp2..."
d:/lode/opt/bin/make scp2.clean 2>&1 > /dev/null
cat autogen.lastrun

exit

--with-junit=/cygdrive/c/sources/junit-4.10.jar \
--with-ant-home=/cygdrive/c/sources/apache-ant-1.9.5 \
--with-package-format=msi \
