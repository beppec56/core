#!/bin/bash

# configuration for windows

#get the branch point
BRANCH_POINT=`diff -u <(git rev-list -200 --first-parent HEAD)   <(git rev-list -200 --first-parent libreoffice-5-0) | sed -ne 's/^ //p' | head -1`
BRANCH_POINT2=`git log -n1 --format=%h $BRANCH_POINT`

. ../gdrive-lohs-credential.shinc

./autogen.sh \
--with-vendor="Acca Esse, I-10067" \
--with-gdrive-client-id="$GDRIVE_CLIENT_ID" \
--with-gdrive-client-secret="$GDRIVE_CLIENT_SECRET" \
--with-lang='en-US' \
\
--with-external-tar=/cygdrive/e/sources/lo-externalsrc \
--with-junit=/cygdrive/c/sources/junit-4.10.jar \
--with-ant-home=/cygdrive/c/sources/apache-ant-1.9.5 \
--enable-pch --disable-ccache \
--disable-activex --disable-atl \
\
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g')$(echo :$(git log -n1 --format=%h)) based on libreoffice-5-0:$BRANCH_POINT2" \

c:/cygwin/opt/lo/bin/make scp2.clean
