#!/bin/bash
cd /srv5/git/LO/lo-gerrit-clang-release/external/libcmis
echo "diff.."
diff -aru cmis-create-patch/old-src cmis-create-patch/src > libcmis-add-lo_compat-debug_logs.patch
echo "compile..."
/srv5/git/LO/make-lo-gerrit-clang-release-emacs-guid.sh libcmis ucb

