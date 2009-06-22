#!/bin/sh
[ ! -d build-aux ] && mkdir build-aux 
aclocal || exit
autoheader || exit
autoconf || exit
libtoolize -f || exit
automake -a -c || exit
