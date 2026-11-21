#!/bin/sh
# Do not replace this with a working installer without a confirmed root cause,
# a reviewed carrier-scoped patch, fresh original backup, SHA gates and
# independent recovery tests. This intentionally performs ZERO vehicle writes.
echo "AU57X_CT_INSTALL_BLOCKED"
echo "REASON=NO_VERIFIED_CARRIER_SCOPED_FIX"
echo "ACTION=STOP_NO_VEHICLE_MUTATION"
exit 78
