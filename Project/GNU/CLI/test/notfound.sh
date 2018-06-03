#!/usr/bin/env bash

PATH="${PWD}:$PATH"

rcode=0

stderr=$(${valgrind} rawcooked -d DoesNotExist/NotFound.dpx 2>&1 >/dev/null)
result=$?

# check valgrind
if [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
    cat valgrind.log >&${fd}
    rcode=1
fi

# check expected result
if [ "${result}" -eq "0" ] && [ -z "${stderr}" ] ; then
    rcode=1
fi

exit ${rcode}

