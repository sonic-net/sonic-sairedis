#!/usr/bin/env bash
#
# Script for sai failure dump
#

# Source the platform specific dump file

SAI_MAX_FAILURE_DUMPS=10

DUMPDIR=/var/log/sai_failure_dump

if [ -f /usr/bin/platform_syncd_dump.sh ]; then
    . ./usr/bin/platform_syncd_dump.sh
fi

if [ -z "$(ls -A $DUMPDIR/)" ]; then
    exit 0
fi

# Perform rotation

ls -1td $DUMPDIR/* | tail -n +$(($SAI_MAX_FAILURE_DUMPS+1)) | xargs rm -rf
