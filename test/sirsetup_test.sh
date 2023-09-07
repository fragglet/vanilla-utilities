#!/bin/bash

TEST_PORT1=30328

set -eu

. test/common.sh

start_node1() {
    start_dosbox <<END
      config -set serial serial2 nullmodem port:$TEST_PORT1
      bld\\sirsetup -com2 $@ test\\fakedoom -out t:node1.txt -secret 1000
END
    sleep 1
}

start_node2() {
    start_dosbox <<END
      config -set serial serial2 nullmodem server:localhost port:$TEST_PORT1
      bld\\sirsetup -com2 $@ test\\fakedoom -out t:node2.txt -secret 2000
END
}

check_player_secrets() {
    if ! diff -u $TEST_DIR/NODE1.TXT $TEST_DIR/NODE2.TXT; then
        echo "$1: fakedoom output from node1 and node2 does not match."
        exit 1
    fi
    if ! grep -q "$2" $TEST_DIR/NODE1.TXT; then
        echo "$1: fakedoom output does not contain '$2':"
        diff -u /dev/null $TEST_DIR/NODE1.TXT
        exit 1
    fi
    if ! grep -q "$3" $TEST_DIR/NODE1.TXT; then
        echo "$1: fakedoom output does not contain '$3':"
        diff -u /dev/null $TEST_DIR/NODE1.TXT
        exit 1
    fi
}

run_player_tests() {
    test_descr="run_player_tests '$1' '$2'"

    # Simple dial and answer.
    start_node1 $1
    start_node2 $2
    wait_dosboxes

    check_player_secrets "$test_descr, simple" \
        "secret=1000" "secret=2000"

    # Force player
    start_node1 $1 -player 2
    start_node2 $2
    wait_dosboxes

    check_player_secrets "$test_descr, force player" \
        "Player 1: secret=2000" "Player 2: secret=1000"

    # Force player at both works as long as they're consistent.
    start_node1 $1 -player 2
    start_node2 $2 -player 1
    wait_dosboxes

    check_player_secrets "$test_descr, force player at both" \
        "Player 1: secret=2000" "Player 2: secret=1000"
}

# For testing null modem connections we just use COM2.
run_player_tests "" ""

# Run tests again forcing 8250 UART (uses different code paths).
run_player_tests "-8250" "-8250"

