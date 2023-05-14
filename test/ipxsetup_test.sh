#!/bin/bash

TEST_PORT=31234

set -eu

. test/common.sh

start_dosbox <<END
  config -set ipx true
  ipxnet startserver $TEST_PORT
  ipxsetup -player 5 -dup 3 -nodes 8 fakedoom -out t:SERVER.TXT -secret 1000
END

sleep 1

for i in $(seq 7); do
    start_dosbox <<END
      config -set ipx true
      ipxnet connect localhost $TEST_PORT
      ipxsetup -nodes 8 fakedoom -out t:CLIENT$i.TXT -secret $((1000 + i))
END
done

wait_dosboxes

# Check the log files look as expected.
# All players generate an identical log file and all secrets are included.
for i in $(seq 7); do
    diff -u $TEST_DIR/SERVER.TXT $TEST_DIR/CLIENT$i.TXT
done

for i in $(seq 0 7); do
    grep -qw secret=$((1000 + i)) $TEST_DIR/SERVER.TXT
done

# We check the -dup and -player parameters work correctly.
grep -q "dup=3" $TEST_DIR/SERVER.TXT
grep -q "Player 5: secret=1000" $TEST_DIR/SERVER.TXT

