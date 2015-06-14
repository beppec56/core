#!/bin/bash

# configuration for windows, to generate packaging as the daily build, simplyfied for gerrit

#get the branch point
BRANCH_POINT=`diff -u <(git rev-list -200 --first-parent HEAD)   <(git rev-list -200 --first-parent libreoffice-5-0) | sed -ne 's/^ //p' | head -1`
BRANCH_POINT2=`git log -n1 --format=%h $BRANCH_POINT`

./autogen.sh \
--with-lang='en-US it' \
\
--with-external-tar=/cygdrive/e/sources/lo-externalsrc \
--with-junit=/cygdrive/c/sources/junit-4.10.jar \
--with-ant-home=/cygdrive/c/sources/apache-ant-1.9.5 \
--with-package-format=msi \
--enable-pch \
--disable-ccache \
--with-visual-studio=2013 \
\
--enable-extension-integration \
--disable-gtk \
--enable-scripting-beanshell \
--enable-scripting-javascript \
--enable-ext-wiki-publisher \
--enable-ext-nlpsolver \
--enable-win-mozab-driver \
--with-help \
--with-myspell-dicts \
--disable-dependency-tracking \
--enable-mergelibs \
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g')$(echo :$(git log -n1 --format=%h)) based on libreoffice-5-0:$BRANCH_POINT2" \

c:/cygwin/opt/lo/bin/make scp2.clean




exit

come in daily build:

'--without-junit' '--without-helppack-integration'
 '--enable-extension-integration' '--disable-gtk' '--enable-scripting-beanshell' '--enable-scripting-javascript' '--enable-ext-wiki-publisher' '--enable-ext-nlpsolver' '--enable-online-update' '--enable-win-mozab-driver' '--with-help' '--with-myspell-dicts' '--with-package-format=msi' '--with-vendor=The Document Foundation' '--with-lang=de ar ja ru' '--disable-dependency-tracking' '--enable-pch' '--with-junit=/home/buildslave/source/junit-4.10.jar' '--with-ant-home=/home/buildslave/source/apache-ant-1.9.4' '--with-external-tar=/home/buildslave/source/lo-externalsrc' '--with-windows-sdk=7.1A' '--enable-mergelibs' '--with-parallelism=10' '--srcdir=/home/buildslave/source/libo-core' '--enable-option-checking=fatal'


