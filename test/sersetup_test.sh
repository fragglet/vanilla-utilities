#!/bin/bash

TEST_PORT=30328

set -eu

. test/common.sh

dosbox_with_conf <<END
[serial]
serial1 = modem listenport:$TEST_PORT

[autoexec]
$AUTOEXEC_BOILERPLATE
sersetup -answer fakedoom -out t:ANSWER.TXT -secret 1000
exit
END

sleep 1

dosbox_with_conf <<END
[serial]
serial1 = modem

[autoexec]
$AUTOEXEC_BOILERPLATE
sersetup -dial 127.0.0.1:$TEST_PORT fakedoom -out t:DIAL.TXT -secret 2000
exit
END

wait_dosboxes

diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
grep -q 1000 $TEST_DIR/ANSWER.TXT
grep -q 2000 $TEST_DIR/ANSWER.TXT

