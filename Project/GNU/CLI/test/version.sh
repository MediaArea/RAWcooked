#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

run_rawcooked --version

# check expected result
if [[ ! "${cmd_stdout}" =~ ^RAWcooked\ ([0-9A-Za-z]+\.)+[0-9A-Za-z]+$ ]] ; then
    status=1
fi

exit ${status}
