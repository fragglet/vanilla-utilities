#!/bin/bash

TEST_PORT1=30328
TEST_PORT2=30329

set -eu

. test/common.sh

start_node1() {
    start_dosbox <<END
      config -set serial serial1 modem listenport:$TEST_PORT1
      config -set serial serial2 nullmodem port:$TEST_PORT2
      $@ test\\fakedoom -out t:NODE1.TXT -secret 1000
END
    sleep 1
}

start_node2() {
    start_dosbox <<END
      config -set serial serial1 modem
      config -set serial serial2 nullmodem server:localhost port:$TEST_PORT2
      $@ test\\fakedoom -out t:NODE2.TXT -secret 2000
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
    start_node1 "bld\\sersetup" $1
    start_node2 "bld\\sersetup" $2
    wait_dosboxes

    check_player_secrets "$test_descr, simple dial and answer" \
        "secret=1000" "secret=2000"

    # Force player at answer.
    start_node1 "bld\\sersetup" $1 -player2
    start_node2 "bld\\sersetup" $2
    wait_dosboxes

    check_player_secrets "$test_descr, force player at answer" \
        "Player 1: secret=2000" "Player 2: secret=1000"

    # Force player at both works as long as they're consistent.
    start_node1 "bld\\sersetup" $1 -player2
    start_node2 "bld\\sersetup" $2 -player1
    wait_dosboxes

    check_player_secrets "$test_descr, force player at both" \
        "Player 1: secret=2000" "Player 2: secret=1000"
}

run_player_tests_non_bg() {
    test_descr="run_player_tests_non_bg '$1' '$2'"

    # Force player at dial.
    start_node1 "bld\\sersetup" $1
    start_node2 "bld\\sersetup" $2 -player1
    wait_dosboxes

    check_player_secrets "$test_descr, force player at dial" \
        "Player 1: secret=2000" "Player 2: secret=1000"
}

# Checks for compatibility with original versions of SERSETUP.
run_compatibility_tests() {
    test_descr="run_compatibility_tests '$1'"

    # Our sersetup answers call.
    start_node1 "bld\\sersetup" -answer -player2
    start_node2 "$1" -dial localhost:$TEST_PORT1
    wait_dosboxes

    check_player_secrets "$test_descr, our sersetup answers" \
        "Player 1: secret=2000" "Player 2: secret=1000"

    # Our sersetup dials call.
    start_node1 "$1" -answer
    start_node2 "bld\\sersetup" -dial localhost:$TEST_PORT1 -player1
    wait_dosboxes

    check_player_secrets "$test_descr, our sersetup dials" \
        "Player 1: secret=2000" "Player 2: secret=1000"

    # Null modem game
    start_node1 "$1" -com2
    start_node2 "bld\\sersetup" -com2 -player2
    wait_dosboxes

    check_player_secrets "$test_descr, null modem game" \
        "Player 1: secret=1000" "Player 2: secret=2000"
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

run_compatibility_tests "test\\exes\\ser1"
run_compatibility_tests "test\\exes\\ser2"

