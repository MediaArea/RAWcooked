#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    file="$(basename "$(echo "${line}" | cut -d' ' -f1)")"
    path="$(dirname "$(echo "${line}" | cut -d' ' -f1)")"
    want="$(echo "${line}" | cut -d' ' -f2)"
    test=$(basename "${path}")

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
        run_rawcooked --file -d "${file}"

        # check expected result
        if [ "${want}" == "fail" ] ; then
            if check_failure "file rejected at input" "file accepted at input" ; then
                echo "OK: ${test}/${file}, file rejected at input" >&${fd}
            fi
            continue
        else
            check_success "file rejected at input" "file accepted at input" || continue
        fi

        if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, reversibility_data is missing" >&${fd}
            status=1
            clean
            continue
        fi

        # check result file generation
        run_ffmpeg "${cmd_stdout}"

        if [ ! -e "${file}.mkv" ] ; then
            echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
            status=1
            clean
            continue
        fi

        # check framemd5
        if ! check_framemd5 "${file}" "${file}.mkv" ; then
            clean
            continue
        fi

        # check decoding
        run_rawcooked "${file}.mkv"
        if ! check_success "mkv decoding failed" "mkv decoded" ; then
            clean
            continue
        fi
        check_files "${file}" "${file}.mkv.RAWcooked/${file}"
        clean
    popd >/dev/null 2>&1
done < "${script_path}/test1.txt"

exit ${status}
