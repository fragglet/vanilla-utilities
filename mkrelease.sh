#!/usr/bin/env bash

md_to_txt() {
    pandoc "$1" \
           -o "$2" \
           -t plain \
           --reference-location=section \
           --lua-filter=man/tweaks.lua \
           --eol=crlf
}

set -euo pipefail

zip_filename="vanilla-utilities-${1:-unknown-version}.zip"

rm -rf dist $zip_filename
mkdir dist

cp -R bld/*.EXE modemcfg/ vutils.ico dist/
for exe in dist/*.EXE; do
    mv $exe $(echo $exe | tr A-Z a-z)
done

md_to_txt COPYING.md dist/COPYING.txt
md_to_txt COPYING1.md dist/COPYING1.txt

for md in man/*.md; do
    fn=$(basename $md)
    md_to_txt $md dist/${fn/.md/.txt}
done

cd dist
zip -X -r ../$zip_filename *
