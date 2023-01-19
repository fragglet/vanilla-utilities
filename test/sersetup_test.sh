#!/bin/bash

TEST_PORT1=30328
TEST_PORT2=30329

set -eu

. test/common.sh

start_answerer() {
    start_dosbox <<END
      serial1 modem listenport:$TEST_PORT1
      serial2 nullmodem port:$TEST_PORT2
      sersetup $@ fakedoom -out t:ANSWER.TXT -secret 1000
END
    sleep 1
}

start_dialer() {
    start_dosbox <<END
      serial1 modem
      serial2 nullmodem server:localhost port:$TEST_PORT2
      sersetup $@ fakedoom -out t:DIAL.TXT -secret 2000
END
}

run_player_tests() {
    # Simple dial and answer.
    start_answerer $1
    start_dialer $2
    wait_dosboxes

    diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
    grep -q secret=1000 $TEST_DIR/ANSWER.TXT
    grep -q secret=2000 $TEST_DIR/ANSWER.TXT

    # Force player at answer.
    start_answerer $1 -player1
    start_dialer $2
    wait_dosboxes

    diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
    grep -q "Player 1: secret=1000" $TEST_DIR/ANSWER.TXT
    grep -q "Player 2: secret=2000" $TEST_DIR/ANSWER.TXT

    # Force player at dial.
    start_answerer $1
    start_dialer $2 -player2
    wait_dosboxes

    diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
    grep -q "Player 1: secret=1000" $TEST_DIR/ANSWER.TXT
    grep -q "Player 2: secret=2000" $TEST_DIR/ANSWER.TXT

    # Force player at both works as long as they're consistent.
    start_answerer $1 -player2
    start_dialer $2 -player1
    wait_dosboxes

    diff -u $TEST_DIR/ANSWER.TXT $TEST_DIR/DIAL.TXT
    grep -q "Player 1: secret=2000" $TEST_DIR/ANSWER.TXT
    grep -q "Player 2: secret=1000" $TEST_DIR/ANSWER.TXT
}

run_player_tests "-answer" "-dial localhost:$TEST_PORT1"
run_player_tests "-bg -answer" "-bg -dial localhost:$TEST_PORT1"

# For testing null modem connections we just use COM2.
run_player_tests "-com2" "-com2"

