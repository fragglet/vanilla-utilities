
TEST_DIR=$(mktemp -d)
TOPLEVEL_DIR=$(dirname $0)/..

AUTOEXEC_BOILERPLATE="
mount c ${TOPLEVEL_DIR}
mount t ${TEST_DIR}
c:
"

# Don't show a window when DOSbox is running; this should be able to
# run entirely headless.
export SDL_VIDEODRIVER=dummy

dosbox_index=0
dosbox_pids=""

dosbox_shutdown() {
	if [ "$dosbox_pids" != "" ]; then
		kill -HUP $dosbox_pids
	fi
	rm -rf "$TEST_DIR"
}

trap dosbox_shutdown INT EXIT

dosbox_with_conf() {
	local dosbox_conf=$TEST_DIR/dosbox-$dosbox_index.conf
	local logfile=$TEST_DIR/dosbox-$dosbox_index.log
	cat > $dosbox_conf
	dosbox_index=$((dosbox_index + 1))
	dosbox -conf $dosbox_conf >$logfile 2>&1 &
	dosbox_pids="$dosbox_pids $!"
}

wait_dosboxes() {
	wait $dosbox_pids
	dosbox_pids=""
}

