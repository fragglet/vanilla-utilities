#!/bin/bash

TEST_PORT1=30328
TEST_PORT2=30329

set -eu

. test/common.sh

start_node1() {
    start_dosbox <<END
      config -set serial serial1 modem listenport:$TEST_PORT1
      config -set serial serial2 nullmodem port:$TEST_PORT2
      bld\\sersetup $@ test\\fakedoom -out t:NODE1.TXT -secret 1000
END
    sleep 1
}

start_node2() {
    start_dosbox <<END
      config -set serial serial1 modem
      config -set serial serial2 nullmodem server:localhost port:$TEST_PORT2
      bld\\sersetup $@ test\\fakedoom -out t:NODE2.TXT -secret 2000
END
}

run_player_tests() {
    # Simple dial and answer.
    start_node1 $1
    start_node2 $2
    wait_dosboxes

    if ! diff -u $TEST_DIR/NODE1.TXT $TEST_DIR/NODE2.TXT ||
       ! grep -q secret=1000 $TEST_DIR/NODE1.TXT ||
       ! grep -q secret=2000 $TEST_DIR/NODE1.TXT; then
        echo "Wrong or missing secrets for '$1' '$2' #1: "
        diff -u /dev/null $TEST_DIR/NODE1.TXT
        exit 1
    fi

    # Force player at answer.
    start_node1 $1 -player2
    start_node2 $2
    wait_dosboxes

    if ! diff -u $TEST_DIR/NODE1.TXT $TEST_DIR/NODE2.TXT ||
       ! grep -q "Player 1: secret=2000" $TEST_DIR/NODE1.TXT ||
       ! grep -q "Player 2: secret=1000" $TEST_DIR/NODE1.TXT; then
        echo "Wrong or missing secrets for '$1' '$2' #2: "
        diff -u /dev/null $TEST_DIR/NODE1.TXT
        exit 1
    fi

    # Force player at both works as long as they're consistent.
    start_node1 $1 -player2
    start_node2 $2 -player1
    wait_dosboxes

    if ! diff -u $TEST_DIR/NODE1.TXT $TEST_DIR/NODE2.TXT ||
       ! grep -q "Player 1: secret=2000" $TEST_DIR/NODE1.TXT ||
       ! grep -q "Player 2: secret=1000" $TEST_DIR/NODE1.TXT; then
        echo "Wrong or missing secrets for '$1' '$2' #3: "
        diff -u /dev/null $TEST_DIR/NODE1.TXT
        exit 1
    fi
}

run_player_tests_non_bg() {
    # Force player at dial.
    start_node1 $1
    start_node2 $2 -player1
    wait_dosboxes

    if ! diff -u $TEST_DIR/NODE1.TXT $TEST_DIR/NODE2.TXT ||
       ! grep -q "Player 1: secret=2000" $TEST_DIR/NODE1.TXT ||
       ! grep -q "Player 2: secret=1000" $TEST_DIR/NODE1.TXT; then
        echo "Wrong or missing secrets for '$1' '$2' #4: "
        diff -u /dev/null $TEST_DIR/NODE1.TXT
        exit 1
    fi
}

run_player_tests "-answer" "-dial localhost:$TEST_PORT1"
run_player_tests_non_bg "-answer" "-dial localhost:$TEST_PORT1"

run_player_tests "-bg -answer" "-dial localhost:$TEST_PORT1"

# For testing null modem connections we just use COM2.
run_player_tests "-com2" "-com2"
run_player_tests_non_bg "-com2" "-com2"

# Run tests again forcing 8250 UART (uses different code paths).
run_player_tests "-8250 -com2" "-8250 -com2"
run_player_tests_non_bg "-8250 -com2" "-8250 -com2"

