#!/usr/bin/env bash

PATH="${PWD}:$PATH"

rcode=0

output=$(${valgrind} rawcooked --version)

# check valgrind
if [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
    cat valgrind.log >&${fd}
    rcode=1
fi

# check expected result
if [[ ! "${output}" =~ ^RAWcooked\ ([0-9]+\.)+[0-9]+$ ]] ; then
    rcode=1
fi

exit ${rcode}

