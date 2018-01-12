#!/bin/bash

# find all source files that can use SWSS_LOG_ENTER macro
# and check if macro is in expected place

set -e

DIR=$(dirname "${BASH_SOURCE[0]}")

find "$DIR/../" -name "*.cpp" -o -name "*.h" | xargs ./swsslogentercheck.pl
