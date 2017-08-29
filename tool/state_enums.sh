#!/bin/sh
BTSTACK_ROOT=`dirname $0`'/..'

# SM
echo "Security Manager states"
grep SM_ $BTSTACK_ROOT/src/hci.h | nl -v0

echo

# RFCOMM
echo "RFCOMM Multiplexer states"
grep RFCOMM_MULTIPLEXER_ $BTSTACK_ROOT/src/classic/rfcomm.h | grep -v RFCOMM_MULTIPLEXER_EVENT | grep -v RFCOMM_MULTIPLEXER_STATE | nl
echo "RFCOMM Channel states"
grep RFCOMM_CHANNEL_ $BTSTACK_ROOT/src/classic/rfcomm.h | grep -v RFCOMM_CHANNEL_STATE | grep -v "// state variables" | nl

echo

# HFP
echo "HFP Commands/States"
grep  HFP_CMD_ $BTSTACK_ROOT/src/classic/hfp.h | nl -v 0
echo "HFP Errors"
grep  HFP_CME_ $BTSTACK_ROOT/src/classic/hfp.h | nl -v 0

echo
