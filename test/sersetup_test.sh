#!/bin/bash

TEST_PORT=30328

set -eu

. test/common.sh

start_answerer() {
    start_dosbox <<END
      serial1 modem listenport:$TEST_PORT
      sersetup -answer $@ fakedoom -out t:ANSWER.TXT -secret 1000
END
    sleep 1
}

start_dialer() {
    start_dosbox <<END
      serial1 modem
      sersetup $@ -dial localhost:$TEST_PORT fakedoom -out t:DIAL.TXT -secret 2000
END
}

# Simple dial and answer.
start_answerer
start_dialer
wait_dosboxes

diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
grep -q secret=1000 $TEST_DIR/ANSWER.TXT
grep -q secret=2000 $TEST_DIR/ANSWER.TXT

# Force player at answer.
start_answerer -player1
start_dialer
wait_dosboxes

diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
grep -q "Player 1: secret=1000" $TEST_DIR/ANSWER.TXT
grep -q "Player 2: secret=2000" $TEST_DIR/ANSWER.TXT

# Force player at dial.
start_answerer
start_dialer -player2
wait_dosboxes

diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
grep -q "Player 1: secret=1000" $TEST_DIR/ANSWER.TXT
grep -q "Player 2: secret=2000" $TEST_DIR/ANSWER.TXT

# Force player at both works as long as they're consistent.
start_answerer -player2
start_dialer -player1
wait_dosboxes

diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
grep -q "Player 1: secret=2000" $TEST_DIR/ANSWER.TXT
grep -q "Player 2: secret=1000" $TEST_DIR/ANSWER.TXT

