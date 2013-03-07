#!/bin/bash

if [ "$1" == "clean" ]; then
    make distclean
    rm -rf aclocal.m4 autom4te.cache config.guess config.sub configure depcomp INSTALL install-sh ltmain.sh missing config.log  config.status libtool
    find . -name 'Makefile.in' -exec rm -f {} \;
    exit;
fi

if [ -x "/usr/bin/libtoolize" ]; then 
    libtoolize --force
else
    glibtoolize --force
fi
aclocal
autoconf
automake --add-missing --force

