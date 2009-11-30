#!/bin/sh
[ ! -d build-aux ] && mkdir build-aux 
aclocal -I m4/ || exit
autoheader || exit
autoconf || exit
libtoolize -f || exit
automake -a -c || exit
