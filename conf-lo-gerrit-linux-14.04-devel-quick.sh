#!/bin/bash

./autogen.sh \
--with-vendor="Acca Esse, I-10067" \
--with-gdrive-client-id="457862564325.apps.googleusercontent.com" \
--with-gdrive-client-secret="GYWrDtzyZQZ0_g5YoBCC6F0I" \
--with-external-tar=/srv5/git/LibO/externals \
--with-lang='en-US' \
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g') $(echo git_$(git log -n1 --format=%h)) based on git_$(git log -n1 --format=%h lo-master-start)" \
\
