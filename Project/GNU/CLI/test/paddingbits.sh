#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    file="$(basename "$(echo "${line}" | cut -d' ' -f1)")"
    path="$(dirname "$(echo "${line}" | cut -d' ' -f1)")"
    filetomodify="$(echo "${line}" | cut -d' ' -f2)"
    want="$(echo "${line}" | cut -d' ' -f3)"
    options="$(echo "${line}" | cut -s -d' ' -f4-)"
    test="${file} $filetomodify $options"

    pushd "${files_path}/${path}" >/dev/null 2>&1
        if [ "${filetomodify}" != "X" ] ; then
          filenametomodify=$(ls ${file} | sed -n ${filetomodify}p)
          chmod u+w ${file}/${filenametomodify} || echo "chmod issue"
          truncate --size=-1 ${file}/${filenametomodify} || echo "truncate issue"
          printf "\x5f" >> ${file}/${filenametomodify} || echo "printf issue"
        fi
        
        run_rawcooked -y $options "${file}"

        # check expected result
        if [ "${want}" == "fail" ] ; then
            if check_failure "file rejected at input" "file accepted at input" ; then
                 echo "OK: ${test}, file rejected at input" >&${fd}
            fi
            continue
        else
            if check_success "file rejected at input" "file accepted at input" ; then
               echo "OK: ${test}, file accepted at input" >&${fd}
           fi
        fi

        clean
    popd >/dev/null 2>&1
done < "${script_path}/paddingbits.txt"

exit ${status}

