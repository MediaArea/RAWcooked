#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="framerate"

while read line ; do
    path="$(echo "${line}" | cut -d' ' -f1)"
    file="$(echo "${line}" | cut -s -d' ' -f2)"
    rate="$(echo "${line}" | cut -s -d' ' -f3)"

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
        # check framerate in output mkv
        run_rawcooked -framerate ${rate} --file "${file}"
        check_success "file rejected at input" "file accepted at input"

        framerate=$(ffmpeg -hide_banner -i "${file}.mkv" 2>&1 </dev/null | tr -d ' ' | grep -m1 'Stream#.\+:.\+:Video:.\+,' | sed -En 's/.*,([0-9]+)fps,.*/\1/p')

        if [ "${framerate}" != "${rate}" ] ; then
            echo "NOK: ${test}/${file}, wrong output framerate, requested: ${rate}, got: ${framerate}" >&${fd}
            status=1
        fi

        clean
    popd >/dev/null 2>&1
done < "${script_path}/framerate.txt"

exit ${status}
