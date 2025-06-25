#!/bin/sh
# original https://github.com/Morganamilo/paru/blob/master/scripts/mkpot
set -e

xgettext \
	-d ulpm \
	--msgid-bugs-address https://github.com/Toni500github/ulpm \
	--package-name=ulpm\
	--default-domain=ulpm\
	--package-version="$(awk -F '= ' '/^VERSION/ {print $2}' Makefile | sed 's/\"//g')" \
	-k_ \
	-o po/ulpm.pot \
	src/*.cpp --c++
