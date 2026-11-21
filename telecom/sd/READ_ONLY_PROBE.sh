#!/bin/sh
# AU57x Telecom R03: SD REPORT ONLY.
# No AT commands, /dev/NAD access, mount changes or writes to vehicle app.
set -u
LC_ALL=C; export LC_ALL
umask 077
APP=/mnt/app
SD=/net/mmx/fs/sda0
OUT="$SD/au57x_ct_r03_readonly"
REPORT="$OUT/RESULT_CN.txt"

echo AU57X_CT_R03_SD_ONLY_NO_INSTALL
if [ ! -d "$SD" ] || [ -L "$SD" ]; then
    echo STOP=SD_UNAVAILABLE
    exit 1
fi
if [ -e "$OUT" ] && [ ! -d "$OUT" ]; then
    echo STOP=UNSAFE_OUTPUT_TYPE
    exit 1
fi
if [ -L "$OUT" ] || [ -L "$REPORT" ]; then
    echo STOP=SYMLINK_OUTPUT_REJECTED
    exit 1
fi
if [ ! -d "$OUT" ] && ! mkdir "$OUT"; then
    echo STOP=SD_NOT_WRITABLE
    exit 1
fi
if [ -e "$REPORT" ] && [ ! -f "$REPORT" ]; then
    echo STOP=REPORT_NOT_REGULAR
    exit 1
fi
if ! echo "AU57X_CT_R03_READONLY" > "$REPORT"; then
    echo STOP=SD_NOT_WRITABLE
    exit 1
fi
echo "LIVE_LTE_REGISTRATION=NOT_MEASURED" >> "$REPORT"
echo "NETWORK_MODE=NOT_MEASURED" >> "$REPORT"
echo "SIM_IDENTIFIERS=NOT_READ" >> "$REPORT"
echo "SOURCE_BINARY_HASH=NOT_COMPUTED_ON_HEADUNIT" >> "$REPORT"
echo "TARGET_MUTATIONS=NONE" >> "$REPORT"

check_file() {
    LABEL=$1
    FILE=$2
    EXPECT=$3
    if [ -L "$FILE" ]; then
        echo "$LABEL=SYMLINK_UNVERIFIED" >> "$REPORT"
    elif [ ! -f "$FILE" ] || [ ! -r "$FILE" ]; then
        echo "$LABEL=NOT_FOUND_OR_UNREADABLE" >> "$REPORT"
    else
        set -- $(wc -c < "$FILE")
        SIZE=$1
        if [ "$SIZE" = "$EXPECT" ]; then
            echo "$LABEL=SIZE_MATCH_ONLY:$SIZE" >> "$REPORT"
        else
            echo "$LABEL=SIZE_MISMATCH:$SIZE" >> "$REPORT"
        fi
    fi
}
check_file NAD "$APP/eso/bin/apps/nad" 845158
check_file TELEPHONE "$APP/eso/bin/apps/telephone" 1458120
check_file CONNECTIONMANAGER "$APP/eso/bin/apps/connectionmanager" 758371
check_file AUTOCONF "$APP/eso/telephone/autoconf.xml" 92014
# Never read SIM numbers, serial ports, data sessions or credentials.
echo "STATUS=READONLY_REPORT_FINISHED_NOT_REGISTRATION_TESTED" >> "$REPORT"
echo "REPORT=$REPORT"
exit 0
