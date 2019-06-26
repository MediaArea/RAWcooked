#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    path="$(echo "${line}" | cut -d' ' -f1)"
    file="$(echo "${line}" | cut -s -d' ' -f2)"
    test=$(basename "${path}")

    if [ -z "${file}" ] ; then
        file="../$(basename ${path})"
    fi

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
        run_rawcooked -y --conch --file -d ${file}
        check_success "file rejected at input" "file accepted at input" || continue

        if [ "${file: -1}" == "/" ] ; then
            # bash-3.2 compatible substitution, needed for macOSX
            file="${file:: $((${#file}-1))}"
        fi

        if [ "${file}" == "." ] ; then
            file="../$(basename $(pwd))"
        fi

        # check result file generation
        if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, reversibility_data is missing" >&${fd}
            status=1
            clean
            continue
        fi
        source_warnings=$(echo "${cmd_stderr}" | grep "Warning: ")

        run_ffmpeg "${cmd_stdout}"

        rm -f "${file}.rawcooked_reversibility_data"

        if [ ! -e "${file}.mkv" ] ; then
            echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
            status=1
            clean
            continue
        fi

        # check decoding
        run_rawcooked --check "${file}.mkv"
        check_success "decoding check failed" "decoding checked"
        if ! contains "Reversability was checked, no issue detected." "${cmd_stdout}" ; then
            echo "NOK: ${test}/${file}, wrong decoding check output, ${cmd_stdout}" >&${fd}
            status=1
            clean
            continue
        fi

        run_rawcooked "${file}.mkv" --check -o "${file}/.."
        check_success "encoded files check failed" "encoded files checked"
        if ! contains "Reversability was checked, no issue detected." "${cmd_stdout}" ; then
            echo "NOK: ${test}/${file}, wrong encoded files check output, ${cmd_stdout}" >&${fd}
            status=1
            clean
            continue
        fi

        run_rawcooked --conch "${file}.mkv"
        check_success "mkv decoding failed" "mkv decoded"
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

        check_directories "${file}" "${file}.mkv.RAWcooked"

        clean
    popd >/dev/null 2>&1
done < "${script_path}/test2.txt"

exit ${status}
