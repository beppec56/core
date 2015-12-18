#!/bin/bash

# configuration for windows
#get the branch point
#get the branch point
. /cygdrive/e/LO/conf-get-branch-point-lo51.sh

. /cygdrive/e/LO/gdrive-lohs-credential.shinc

./autogen.sh \
--with-vendor="Acca Esse, I-10067" \
--with-gdrive-client-id="$GDRIVE_CLIENT_ID" \
--with-gdrive-client-secret="$GDRIVE_CLIENT_SECRET" \
--with-lang='en-US it fr de' \
\
--with-external-tar=/cygdrive/e/sources/lo-externalsrc \
--with-junit=/cygdrive/c/sources/junit-4.10.jar \
--with-ant-home=/cygdrive/c/sources/apache-ant-1.9.5 \
--with-package-format=msi \
--enable-pch --disable-ccache \
--with-visual-studio=2013 \
\
--enable-extension-integration \
--enable-scripting-beanshell \
--enable-scripting-javascript \
--enable-ext-wiki-publisher \
--enable-ext-nlpsolver \
\
--with-myspell-dicts \
--disable-dependency-tracking \
\
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g')$(echo :$(git log -n1 --format=%h)) based on $ROOT_BRANCH:$BRANCH_POINT2" \


echo "clean scp2 module"
C:/cygwin/opt/lo/bin/make scp2.clean > /dev/null

echo "configuration selected:"
cat autogen.lastrun
