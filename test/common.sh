
TEST_DIR=$(mktemp -d)
TOPLEVEL_DIR=$(dirname $0)/..

DOSBOX_COMMON_OPTIONS="
[sdl]
usescancodes=false

[sblaster]
sbtype=none

[midi]
mpu401=none

[render]
frameskip=10
"

# Don't show a window when DOSbox is running; this should be able to
# run entirely headless.
export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy

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
	local config_options="$1"
	local dosbox_conf=$TEST_DIR/dosbox-$dosbox_index.conf
	local logfile=$TEST_DIR/dosbox-$dosbox_index.log
	local batfile=$TEST_DIR/CMDS_$dosbox_index.BAT

	# Batch file contains commands from stdin
	cat > $batfile

	# Generate config file
	cat >$dosbox_conf <<END
	$DOSBOX_COMMON_OPTIONS
	$config_options
	[autoexec]
	mount c ${TOPLEVEL_DIR}
	mount t ${TEST_DIR}
	c:
	call t:CMDS_${dosbox_index}.BAT
	exit
END

	dosbox_index=$((dosbox_index + 1))
	dosbox -conf $dosbox_conf >$logfile 2>&1 &
	dosbox_pids="$dosbox_pids $!"
}

wait_dosboxes() {
	wait $dosbox_pids
	dosbox_pids=""
}

