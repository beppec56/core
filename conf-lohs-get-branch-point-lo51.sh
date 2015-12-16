#!/bin/bash

#get the branch point
ROOT_BRANCH=libreoffice-5-1
BRANCH_POINT=`diff -u <(git rev-list -200 --first-parent HEAD)   <(git rev-list -200 --first-parent $ROOT_BRANCH) | sed -ne 's/^ //p' | head -1`
BRANCH_POINT2=`git log -n1 --format=%h $BRANCH_POINT`
