#!/bin/bash

rm -rf dist
mkdir dist

cp -R *.EXE modemcfg/ vutils.ico dist/

cp COPYING.GPL1 dist/COPYING1.txt
cp COPYING.md dist/COPYING.txt
cp METANET-HOWTO.md dist/METANET.txt
unix2dos dist/COPYING1.txt dist/COPYING.txt dist/METANET.txt

cd dist
zip -X -r ../vanilla-utilities-${1:-unknown-version}.zip *

