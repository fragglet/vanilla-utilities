#!/bin/bash

TEST_PORT=31234

set -eu

. test/common.sh

check_outputs_match() {
	cnt=$1

	# Check the log files look as expected. All players generate an
	# identical log file and all secrets are included.
	for i in $(seq $cnt); do
		diff -u $TEST_DIR/SERVER.TXT $TEST_DIR/CLIENT$i.TXT
	done
}

# Simple test that we can spin up an 8-player netgame.
test_8players() {
	start_dosbox <<END
	  config -set ipx true
	  ipxnet startserver $TEST_PORT
	  bld\\ipxsetup -player 5 -dup 3 -nodes 8 test\\fakedoom \
	    -out t:SERVER.TXT -secret 1000
END

	sleep 1

	for i in $(seq 7); do
		start_dosbox <<END
		  config -set ipx true
		  ipxnet connect localhost $TEST_PORT
		  bld\\ipxsetup -nodes 8 test\\fakedoom \
		    -out t:CLIENT$i.TXT -secret $((1000 + i))
END
	done

	wait_dosboxes
	check_outputs_match 7

	# All secrets appear in the output at least once.
	for i in $(seq 0 $cnt); do
		grep -qw secret=$((1000 + i)) $TEST_DIR/SERVER.TXT
	done

	# We check the -dup and -player parameters work correctly.
	grep -q "dup=3" $TEST_DIR/SERVER.TXT
	grep -q "Player 5: secret=1000" $TEST_DIR/SERVER.TXT
}

test_orig_compatibility() {
	rm -f $TEST_DIR/*.TXT

	start_dosbox <<END
	  config -set ipx true
	  ipxnet startserver $TEST_PORT
	  bld\\ipxsetup test\\fakedoom -out t:SERVER.TXT -secret 1000
END

	sleep 1

	start_dosbox <<END
	  config -set ipx true
	  ipxnet connect localhost $TEST_PORT
	  $1 test\\fakedoom -out t:CLIENT1.TXT -secret 1001
END

	wait_dosboxes
	check_outputs_match 1

	# We check the -dup and -player parameters work correctly.
	grep -q "secret=1000" $TEST_DIR/SERVER.TXT
	grep -q "secret=1001" $TEST_DIR/SERVER.TXT
}

# Check that our IPXSETUP is compatible with xttl's modified IPXSETUP.
# To check this we spin up a four player netgame where two players are
# of each executable.
test_xttl_compatibility() {
	rm -f $TEST_DIR/*.TXT

	start_dosbox <<END
	  config -set ipx true
	  ipxnet startserver $TEST_PORT
	  bld\\ipxsetup -dup 3 -player 4 -nodes 4 test\\fakedoom \
	    -out t:SERVER.TXT -secret 1000
END

	sleep 1

	start_dosbox <<END
	  config -set ipx true
	  ipxnet connect localhost $TEST_PORT
	  bld\\ipxsetup -dup 3 -player 3 -nodes 4 test\\fakedoom \
	    -out t:CLIENT1.TXT -secret 1001
END

	# A bit of jumping through hoops here. We run the ipxttl binary
	# from different directories otherwise both will generate a response
	# file named IPX$$$.TMP and one will overwrite the other.
	# The "foobar" argument is a hack to make the ipxsetup args be
	# ignored by fakedoom.exe.

	start_dosbox <<END
	  config -set ipx true
	  ipxnet connect localhost $TEST_PORT
	  t:
	  mkdir 3
	  cd 3
	  c:\\test\\exes\\ipxttl \
	    -out t:\\CLIENT2.TXT -secret 1002 \
	    -exec c:\\test\\fakedoom.exe -extratic -nodes 4 -dup 3 -player 2
END

	start_dosbox <<END
	  config -set ipx true
	  ipxnet connect localhost $TEST_PORT
	  t:
	  mkdir 4
	  cd 4
	  c:\\test\\exes\\ipxttl \
	    -out t:\\CLIENT3.TXT -secret 1003 \
	    -exec c:\\test\\fakedoom.exe -extratic -nodes 4 -player 1 -dup 3
END

	wait_dosboxes
	check_outputs_match 3

	# We check the -dup and -player parameters work correctly.
	grep -q "dup=3" $TEST_DIR/SERVER.TXT
	grep -q "Player 1: secret=1003" $TEST_DIR/SERVER.TXT
	grep -q "Player 2: secret=1002" $TEST_DIR/SERVER.TXT
	grep -q "Player 3: secret=1001" $TEST_DIR/SERVER.TXT
	grep -q "Player 4: secret=1000" $TEST_DIR/SERVER.TXT
}

test_8players
test_orig_compatibility "test\\exes\\ipx1"
test_orig_compatibility "test\\exes\\ipx2"

test_xttl_compatibility
