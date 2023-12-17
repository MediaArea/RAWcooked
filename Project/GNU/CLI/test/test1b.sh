#!/usr/bin/env bash

# same as test1 except ' --check-padding -slices 4' option

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
        run_rawcooked -y --conch --encode --file --check-padding -slices 4 -d "${file}"

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
        source_warnings=$(echo "${cmd_stderr}" | grep "Warning: ")

        # check result file generation
        run_ffmpeg "${cmd_stdout}"

        if [ ! -e "${file}.mkv" ] ; then
            echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
            status=1
            clean
            continue
        fi

        # check decoding
        run_rawcooked -y --conch --decode "${file}.mkv"
        if ! check_success "mkv decoding failed" "mkv decoded" ; then
            clean
            continue
        fi

        decoded_warnings=$(echo "${cmd_stderr}" | grep "Warning: ")
        if [ "${source_warnings}" != "${decoded_warnings}" ] ; then
            echo "NOK: ${test}/${file}, warnings differs between the source and the decoded files" >&${fd}
            status=1
            clean
            continue
        fi

        if [ -d "${file}" ] ; then
            check_directories "${file}" "${file}.mkv.RAWcooked/"
        else
            check_files "${file}" "${file}.mkv.RAWcooked/${file}"
        fi
        clean
    popd >/dev/null 2>&1
done < "${script_path}/test1b.txt"

exit ${status}
