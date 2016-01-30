#!/bin/bash

#get the branch point
. ./conf-lohs-get-branch-point-lo51.sh

. ../gdrive-lohs-credential.shinc

./autogen.sh CC=clang-3.6 CXX=clang++-3.6 CLANGDIR=/usr/lib/llvm-3.6 \
--disable-ccache \
--with-vendor="Acca Esse, I-10067" \
--with-gdrive-client-id="$GDRIVE_CLIENT_ID" \
--with-gdrive-client-secret="$GDRIVE_CLIENT_SECRET" \
--with-external-tar=/srv5/git/LO/externals \
--without-system-dicts \
--with-myspell-dicts \
--with-system-zlib \
--without-system-poppler \
--without-system-openssl \
--without-system-libpng \
--without-system-libxml \
--without-system-jpeg \
--without-system-jars \
--without-system-postgresql \
--without-junit \
--with-help \
--without-helppack-integration \
--with-linker-hash-style=both \
--with-fonts \
--enable-dbus \
--enable-extension-integration \
--enable-odk \
--enable-gstreamer-0-10 \
--disable-gstreamer-1-0 \
\
--enable-extension-integration \
--enable-scripting-beanshell \
--enable-scripting-javascript \
--enable-ext-wiki-publisher \
--enable-ext-nlpsolver \
--enable-epm \
--enable-python=internal \
--disable-online-update  \
--disable-gio \
--disable-randr-link \
--with-package-format="deb" \
--with-lang='en-US it fr de pt-BR' \
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g')$(echo :$(git log -n1 --format=%h)) based on $ROOT_BRANCH:$BRANCH_POINT2" \
\

make scp2.clean > /dev/null 2>&1
cat autogen.lastrun
