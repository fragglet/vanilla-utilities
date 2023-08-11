#!/bin/sh

tests="
  ipxsetup_test
  sersetup_test
  sirsetup_test
  metanet_test
"

result=0

for t in $tests; do
	if ./test/${t}.sh; then
		echo "PASS: $t"
	else
		echo "FAIL: $t"
		result=1
	fi
done

exit $result

