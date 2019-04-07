#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    path="$(echo "${line}" | cut -d' ' -f1)"
    file="$(echo "${line}" | cut -s -d' ' -f2)"
    files="$(echo "${line}" | cut -s -d' ' -f2-)"
    test=$(basename "${path}")

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
        run_rawcooked --file -d ${files}
        if ! check_success "file rejected at input" "file accepted at input" ; then
            clean
            continue
        fi

        if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, reversibility_data is missing" >&${fd}
            clean
            continue
        fi

        # check result file generation
        cmdline=$(echo -e "${cmd_stdout}" | grep ffmpeg) 2>/dev/null
        if [ -n "${cmdline}" ] ; then
            eval "${cmdline} </dev/null >/dev/null 2>&1"
        fi

        rm -f "${file}.rawcooked_reversibility_data"
        if [ ! -e "${file}.mkv" ] ; then
            echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
            status=1
            continue
        fi

        run_rawcooked "${file}.mkv"
        if ! check_success "mkv decoding failed" "mkv decoded" ; then
            clean
            continue
        fi

        rm -f "${file}.mkv"

        # check result files
        files="$(find * -path ${file}.mkv.RAWcooked -prune -o -type f ! -empty -print)"

        for f in ${files} ; do
            if [ ! -e  "${file}.mkv.RAWcooked/${f}" ] ; then
                echo "NOK: ${test}/${f} is missing" >&${fd}
                status=1
                continue
            fi

            check_files "${f}" "${file}.mkv.RAWcooked/${f}"
        done

        if [ "$(find ${file}.mkv.RAWcooked -type f | wc -l)" -gt "$(echo ${files} | wc -w)" ] ; then
            echo "NOK: ${test} unwanted files in ${file}.mkv.RAWcooked directory" >&${fd}
            status=1
        fi

        clean
    popd >/dev/null 2>&1
done < "${script_path}/multiple.txt"

exit ${status}
