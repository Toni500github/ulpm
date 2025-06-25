#!/bin/sh
set -e

for po in po/*.po; do
	msgmerge "$po" "po/ulpm.pot" -o "$po"
done
