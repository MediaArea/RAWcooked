#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

run_rawcooked -d DoesNotExist/NotFound.dpx

# check expected result
if [ "${cmd_status}" -eq "0" ] && [ -z "${cmd_stderr}" ] ; then
    status=1
fi

clean

exit ${status}
