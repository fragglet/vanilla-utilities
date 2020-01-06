#!/bin/bash

TEST_PORT=30328

set -eu

. test/common.sh

dosbox_with_conf <<END
[serial]
serial1 = modem listenport:$TEST_PORT

[autoexec]
$AUTOEXEC_BOILERPLATE
sersetup -answer fakedoom -out t:answer.txt -secret 1000
exit
END

sleep 1

dosbox_with_conf <<END
[serial]
serial1 = modem

[autoexec]
$AUTOEXEC_BOILERPLATE
sersetup -dial 127.0.0.1:$TEST_PORT fakedoom -out t:dial.txt -secret 2000
exit
END

wait_dosboxes

diff -u $TEST_DIR/answer.txt $TEST_DIR/dial.txt
grep -q 1000 $TEST_DIR/answer.txt
grep -q 2000 $TEST_DIR/answer.txt

