#!/usr/bin/env bash

md_to_txt() {
    local md=$1
    local out=$2
    pandoc "$md" > /tmp/tmp.html
    elinks -dump-charset ascii -dump /tmp/tmp.html > "$out"
    unix2dos "$out"
    rm -f /tmp/tmp.html
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
