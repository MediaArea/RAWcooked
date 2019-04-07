#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    file="$(basename "$(echo "${line}" | cut -d' ' -f1)")"
    path="$(dirname "$(echo "${line}" | cut -d' ' -f1)")"
    want="$(echo "${line}" | cut -d' ' -f2)"
    test=$(basename "${path}")

    pushd "${files_path}/${path}" >/dev/null 2>&1
        run_rawcooked --file -d "${file}"

        # check expected result
        if [ "${want}" == "fail" ] ; then
            if check_failure "file rejected at input" "file accepted at input" ; then
                 echo "OK: ${test}/${file}, file rejected at input" >&${fd}
            fi
            continue
        else
            if check_success "file rejected at input" "file accepted at input" ; then
               echo "OK: ${test}/${file}, file accepted at input" >&${fd}
           fi
        fi

        clean
    popd >/dev/null 2>&1
done < "${script_path}/test3.txt"

exit ${status}

