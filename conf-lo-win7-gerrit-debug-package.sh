#!/bin/bash

# configuration for windows, to generate packaging, simplyfied for gerrit with debug symbols

#get the branch point
BRANCH_POINT=`diff -u <(git rev-list -200 --first-parent HEAD)   <(git rev-list -200 --first-parent libreoffice-5-0) | sed -ne 's/^ //p' | head -1`
BRANCH_POINT2=`git log -n1 --format=%h $BRANCH_POINT`

./autogen.sh \
--with-lang='en-US' \
\
--with-external-tar=/cygdrive/e/sources/lo-externalsrc \
--with-junit=/cygdrive/c/sources/junit-4.10.jar \
--with-ant-home=/cygdrive/c/sources/apache-ant-1.9.5 \
--with-package-format=msi \
--enable-pch \
--disable-ccache \
--with-visual-studio=2013 \
\
--enable-sal-log \
--enable-dbgutil \
--enable-debug \
--disable-odk \
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g')$(echo :$(git log -n1 --format=%h)) based on libreoffice-5-0:$BRANCH_POINT2" \

c:/cygwin/opt/lo/bin/make scp2.clean

