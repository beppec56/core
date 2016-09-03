#!/bin/bash

#get the branch point
ROOT_BRANCH=libreoffice-5.1.3
export BRANCH_POINT=`diff -u <(git rev-list -2000 --first-parent HEAD)   <(git rev-list -200 --first-parent $ROOT_BRANCH) | sed -ne 's/^ //p' | head -1`
export BRANCH_POINT2=`git log -n1 --format=%h $BRANCH_POINT`
echo $BRANCH_POINT
echo $BRANCH_POINT2
