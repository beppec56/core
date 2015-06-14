#!/bin/bash

# configuration for windows, to generate packaging, simplyfied for gerrit with debug symbols

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

