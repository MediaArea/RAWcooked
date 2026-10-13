#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    read -r entry filetomodify want options <<< "${line}"
    file="${entry##*/}"
    path="${entry%/*}"
    test="${file} $filetomodify $options"

    pushd "${files_path}/${path}" >/dev/null 2>&1
        if [ "${filetomodify}" != "X" ] ; then
          filenametomodify=$(ls ${file} | sed -n ${filetomodify}p)
          chmod u+w ${file}/${filenametomodify} || fatal "${test}" "internal: chmod issue"
          truncate -s -1 ${file}/${filenametomodify} 2> $fd || fatal "${test}" "internal: truncate issue"
          printf "\x5f" >> ${file}/${filenametomodify} || echo fatal "${test}" "internal: printf issue"
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

