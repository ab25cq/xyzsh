#!/bin/bash

if [ -e Makefile ]
then
    make distclean
fi

if [ ! -e Makefile ]
then
    (cd ..; tar cvfz xyzsh.tgz xyzsh)
    (cd ..; scp xyzsh.tgz ab25cq@clover-lang.org:)
    ssh ab25cq@clover-lang.org bash -c '"cp -f xyzsh.tgz repo/; cd repo; rm -rf xyzsh; tar xvfz xyzsh.tgz"'
fi

