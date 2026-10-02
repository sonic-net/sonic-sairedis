#!/bin/bash

EXIT_VALUE=0
saiasiccmp=${1:-./saiasiccmp}
dump1=${2:-dump1.json}
dump2=${3:-dump2.json}
dump3=${4:-dump3.json}

# When run under Bazel, args are rlocation paths; prepend TEST_SRCDIR to resolve them.
if [ -n "${TEST_SRCDIR:-}" ] && [ "$#" -gt 0 ]; then
    saiasiccmp="$TEST_SRCDIR/$saiasiccmp"
    dump1="$TEST_SRCDIR/$dump1"
    dump2="$TEST_SRCDIR/$dump2"
    dump3="$TEST_SRCDIR/$dump3"
fi

function test_positive()
{
    "${saiasiccmp}" "${dump1}" "${dump2}"

    if [ $? != 0 ]; then
        echo "${FUNCNAME[0]} ERROR: expected dumps to be equal"
        EXIT_VALUE=1
    fi
}

function test_negative()
{
    "${saiasiccmp}" "${dump1}" "${dump3}"

    if [ $? == 0 ]; then
        echo "${FUNCNAME[0]} ERROR: expected dumps to be not equal"
        EXIT_VALUE=1
    fi
}

test_positive;
test_negative;

exit $EXIT_VALUE
