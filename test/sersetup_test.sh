#!/bin/bash

TEST_PORT=30328
SERVER_OPTIONS="
[serial]
serial1 = modem listenport:$TEST_PORT
"
CLIENT_OPTIONS="
[serial]
serial1 = modem
"

set -eu

. test/common.sh

dosbox_with_conf "$SERVER_OPTIONS" <<END
sersetup -answer fakedoom -out t:ANSWER.TXT -secret 1000
END

sleep 1

dosbox_with_conf "$CLIENT_OPTIONS" <<END
sersetup -dial 127.0.0.1:$TEST_PORT fakedoom -out t:DIAL.TXT -secret 2000
END

wait_dosboxes

diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
grep -q 1000 $TEST_DIR/ANSWER.TXT
grep -q 2000 $TEST_DIR/ANSWER.TXT

