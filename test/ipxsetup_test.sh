#!/bin/bash

TEST_PORT=31234

set -eu

. test/common.sh

dosbox_with_conf <<END
[ipx]
ipx = true

[autoexec]
$AUTOEXEC_BOILERPLATE
ipxnet startserver $TEST_PORT
ipxsetup -nodes 8 fakedoom -out t:server.txt -secret 1000
exit
END

sleep 1

for i in $(seq 7); do
	dosbox_with_conf <<END
	[ipx]
	ipx = true

	[autoexec]
	$AUTOEXEC_BOILERPLATE
	ipxnet connect 127.0.0.1 $TEST_PORT
	ipxsetup -nodes 8 fakedoom -out t:client$i.txt -secret $((1000 + i))
	exit
END
done

wait_dosboxes

# Check the log files look as expected.
# All players generate an identical log file and all secrets are included.
for i in $(seq 7); do
	diff -u $TEST_DIR/server.txt $TEST_DIR/client$i.txt
done

for i in $(seq 0 7); do
	grep -qw $((1000 + i)) $TEST_DIR/server.txt
done

