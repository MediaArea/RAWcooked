#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    read -r path file dummy <<< "${line}"
    file_input="${file}"
    test="${path##*/}/${file_input}"

    if [ "${file: -1}" == "/" ] ; then
        # bash-3.2 compatible substitution, needed for macOSX
        file="${file:: $((${#file}-1))}"
    fi

    if [ "${file}" == "." ] ; then
        file="../${path##*/}"
    fi

    if [ -z "${file}" ] ; then
        file="../${path##*/}"
        file_input="${file}"
    fi

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}, file not found" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
        # integrated check
        run_rawcooked --check --check-padding ${file_input}
        check_success "integrated check failed" "integrated check checked"
        rm  "${file}.mkv"
        if ! contains "Reversibility was checked, no issue detected." "${cmd_stdout}" ; then
            echo "NOK: ${test}, wrong integrated check output, ${cmd_stdout}" >&${fd}
            status=1
            clean
            continue
        fi

        # run analysis
        run_rawcooked -y --conch --encode --check-padding --file ${file_input}
        check_success "file rejected at input" "file accepted at input" || continue

        grep_lines source_warnings "Warning: " "${cmd_stderr}"

        if [ ! -e "${file}.mkv" ] ; then
            echo "NOK: ${test}, mkv not generated" >&${fd}
            status=1
            clean
            continue
        fi

        # check decoding
        run_rawcooked --check "${file}.mkv"
        check_success "decoding check failed" "decoding checked"
        if ! contains "Decoding was checked, no issue detected." "${cmd_stdout}" ; then
            echo "NOK: ${test}, wrong decoding check output, ${cmd_stdout}" >&${fd}
            status=1
            clean
            continue
        fi

        run_rawcooked "${file}.mkv" --check -o "${file}/.."
        check_success "encoded files check failed" "encoded files checked"
        if ! contains "Reversibility was checked, no issue detected." "${cmd_stdout}" ; then
            echo "NOK: ${test}, wrong encoded files check output, ${cmd_stdout}" >&${fd}
            status=1
            clean
            continue
        fi

        run_rawcooked -y --conch --decode "${file}.mkv"
        check_success "mkv decoding failed" "mkv decoded"
        if ! check_success "mkv decoding failed" "mkv decoded" ; then
            clean
            continue
        fi

        grep_lines decoded_warnings "Warning: " "${cmd_stderr}"
        if [ "${source_warnings}" != "${decoded_warnings}" ] ; then
            echo "NOK: ${test}, warnings differs between the source and the decoded files" >&${fd}
            status=1
            clean
            continue
        fi

        check_directories "${file}" "${file}.mkv.RAWcooked"

        clean
    popd >/dev/null 2>&1
done < "${script_path}/test2.txt"

exit ${status}
