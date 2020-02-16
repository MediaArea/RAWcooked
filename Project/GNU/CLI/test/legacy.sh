#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    file="$(basename "$(echo "${line}")")"
    path="$(dirname "$(echo "${line}")")"
    test=$(basename "${path}")

    pushd "${files_path}/${path}" >/dev/null 2>&1
        run_rawcooked ${file}

        # check expected result
        if check_success "command rejected" "command accepted" ; then
           echo "OK: ${test}/${file}, command accepted at input" >&${fd}
        fi

        clean
    popd >/dev/null 2>&1
done < "${script_path}/legacy.txt"

exit ${status}
