#!/bin/bash
#
# Test script for testing the METANET networking driver that builds a
# forwarding network based on other Doom networking drivers. The test
# builds a deliberately-complicated forwarding network that looks like
# this:
#              ------------------------
#             | A      (B)           C | - IPX 4000
#              ------------------------
#               ^       ^            ^
#           4001|   4002|        4003|
#               D       E   I ----> (F) <---- J
#               ^            4006    ^   4007
#           4004|                4005|
#               G                    H
#
# There is a three-node IPX LAN and the rest of the connections are
# dial-up serial links (the number next to each arrow is the port
# number).
#
# Things that are tested here:
#  * This is an eight player game (+ one dedicated forwarding node);
#    you could theoretically run a Hexen game over this.
#  * Nodes can use at least four underlying drivers (F).
#  * There are two nodes running in forwarding mode (B), and they
#    quit automatically when there is no more work to be done.
#  * There is at least one four-hop route through the network
#    (G-D-A-C-F-I). 

set -eu

. test/common.sh

# Node A (IPX server, accepts dial-in from D)
start_dosbox <<END
  serial1 modem listenport:4001
  ipx true
  ipxnet startserver 4000
  sersetup -answer ipxsetup -nodes 3 metanet fakedoom -out MNTEST_A.TXT -secret 10001
END

sleep 1

# Node B (Dedicated forwarding node, IPX client, accepts dial-in from E)
start_dosbox <<END
  serial1 modem listenport:4002
  ipx true
  ipxnet connect localhost 4000
  sersetup -answer ipxsetup -nodes 3 metanet -forward
END

sleep 1

# Node C (IPX client, accepts connection from F)
start_dosbox <<END
  serial1 modem listenport:4003
  ipx true
  ipxnet connect localhost 4000
  sersetup -answer ipxsetup -nodes 3 metanet fakedoom -out MNTEST_C.TXT -secret 10003
END

sleep 1

# Node D (dial-in to A, accept call from G)
start_dosbox <<END
  serial1 modem
  serial2 modem listenport:4004
  sersetup -dial localhost:4001 sersetup -com2 -answer metanet fakedoom -out MNTEST_D.TXT -secret 10004
END

sleep 1

# Node E (dial-in to B)
start_dosbox <<END
  serial1 modem
  sersetup -dial localhost:4002 metanet fakedoom -out MNTEST_E.TXT -secret 10005
END

sleep 1

# Node G (dial-in to D)
start_dosbox <<END
  serial1 modem
  sersetup -dial localhost:4004 metanet fakedoom -out MNTEST_G.TXT -secret 10007
END

# Node F (dial-in to C, accept call from H, I and J)
# Note we use the SERSETUP -bg argument and check we can accept
# three incoming calls "backwards".
start_dosbox <<END
  serial1 modem
  serial2 modem listenport:4005
  serial3 modem listenport:4006
  serial4 modem listenport:4007
  sersetup -dial localhost:4003 sersetup -com2 -bg -answer sersetup -com3 -bg -answer sersetup -com4 -bg -answer metanet -forward
END

sleep 5

# Node J (dial-in to J)
start_dosbox <<END
  serial1 modem
  sersetup -dial localhost:4007 metanet fakedoom -out MNTEST_J.TXT -secret 10010
END

# Node I (dial-in to F)
start_dosbox <<END
  serial1 modem
  sersetup -dial localhost:4006 metanet fakedoom -out MNTEST_I.TXT -secret 10009
END

# Node H (dial-in to F)
start_dosbox <<END
  serial1 modem
  sersetup -dial localhost:4005 metanet fakedoom -out MNTEST_H.TXT -secret 10008
END

wait_dosboxes

# Each node has a unique secret. Check the test output for node A to confirm
# that all the secrets are present in it.
result=0

for secret in 10001 10003 10004 10005 10007 10008 10009 10010; do
    if ! grep -q "secret=$secret" MNTEST_A.TXT; then
        echo "Secret $secret missing from test output" 2>&1
        result=1
    fi
done

# Test outputs for all nodes should be identical.
for node in C D E G H I J; do
    filename=MNTEST_${node}.TXT
    if diff MNTEST_A.TXT $filename 2>&1; then
        rm -f $filename
    else
        result=1
    fi
done

if [ $result -eq 0 ]; then
    rm -f MNTEST_A.TXT
fi

exit $result

