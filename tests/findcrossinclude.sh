#!/bin/bash

# ideally we want this dependency:
# - saimetadata - no dependency
# - saimeta - depends on saimetadata
# - sairedis - depends on saimeta
# - saivs - depends on saimeta
# - syncd - depnds on sairedis/saivs

# some headers like sairedis.h and sairediscommon.h are cross used on all projects

echo -- find Makefile.am files with cross directory file include

find .. -name Makefile.am | grep -vP "SAI/|pyext/" | xargs grep -P "\.\./"

echo -- find cross include dependencies between meta/lib/vslib directory

find ../meta ../lib ../vslib -name "*.cpp" -o -name "*.h" | \
         xargs grep -P "#include.+/" | grep -vP "(meta/|swss/)" | grep -vP "include <(linux|sys|net|arpa)"

echo -- find cross include in meta/Makefile.am

grep -P "FLAGS.+(lib|vslib|syncd)" ../meta/Makefile.am

echo -- find cross include in lib/Makefile.am

grep -P "FLAGS.+(vslib|syncd)" ../lib/Makefile.am

echo -- find cross include in vslib/Makefile.am

grep -P "FLAGS.+(lib|syncd)" ../vslib/Makefile.am

echo -- find cross include in meta directory

find ../meta/.deps -name "*.Plo" -o -name "*.Po"|xargs grep -P "[^r]/lib/|vslib/|syncd/"| \
         perl -npe 's!lib/sairedis(common)?.h!!g' | grep -P "[^r]/lib/|vslib/|syncd/" | grep -v deps/test

echo -- find cross include in lib directory

find ../lib/.deps -name "*.Plo" -o -name "*.Po"|xargs grep -P "vslib/|syncd/"

echo -- find cross include in vslib directory

find ../vslib/.deps -name "*.Plo" -o -name "*.Po"|xargs grep -P "[^r]/lib/|syncd/"| \
         perl -npe 's!lib/sairedis.h!!g' | grep -P "[^r]/lib/|syncd/"
