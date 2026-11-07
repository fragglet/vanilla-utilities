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

rm -rf dist
mkdir dist

cp -R bld/*.EXE modemcfg/ vutils.ico dist/

cp COPYING.GPL1 dist/COPYING1.txt
unix2dos dist/COPYING1.txt

md_to_txt COPYING.md dist/COPYING.txt
for md in man/*.md; do
    fn=$(basename $md)
    md_to_txt $md dist/${fn/.md/.txt}
done

cd dist
zip -X -r ../vanilla-utilities-${1:-unknown-version}.zip *

