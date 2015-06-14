#!/bin/bash
date > duration-of-make.txt
C:/cygwin/opt/lo/bin/make
date >> duration-of-make.txt
cat duration-of-make.txt
