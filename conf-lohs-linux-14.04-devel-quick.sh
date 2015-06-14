#!/bin/bash

. ../gdrive-lohs-credential.shinc

./autogen.sh \
--with-vendor="Acca Esse, I-10067" \
--with-gdrive-client-id="$GDRIVE_CLIENT_ID" \
--with-gdrive-client-secret="$GDRIVE_CLIENT_SECRET" \
--with-external-tar=/srv5/git/LO/externals \
--with-lang='en-US' \
--with-build-version="$(date +"%Y-%m-%d %H:%M:%S") - Rev. $(git branch |grep "*" | sed 's/* //g') $(echo git_$(git log -n1 --format=%h)) based on git_$(git log -n1 --format=%h lohs-5-0-start)" \
\

