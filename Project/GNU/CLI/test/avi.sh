#!/usr/bin/env bash
script_path="${PWD}/test"
. ${script_path}/helpers.sh

while read line ; do
    read -r entry want dummy <<< "${line}"
    file="${entry##*/}"
    path="${entry%/*}"
    test="${path##*/}"

    pushd "${files_path}/${path}" >/dev/null 2>&1
        run_rawcooked -y --conch --encode --check --file "${file}"

        # check expected result
        if [ "${want}" == "fail" ] ; then
            if check_failure "file rejected at input" "file accepted at input" ; then
                echo "OK: ${test}, file rejected at input" >&${fd}
            fi
            clean
            continue
        elif ! check_success "file rejected at input" "file accepted at input" ; then
            clean
            continue
        fi

        grep_lines source_warnings "Warning: " "${cmd_stderr}"

        # check encoding
        run_rawcooked -y --check "${file}.mkv"
        if ! check_success "mkv check failed" "mkv checked" ; then
            clean
            continue
        fi

        # check decoding
        run_rawcooked -y --conch --decode "${file}.mkv"
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

        check_files "${file}" "${file}.mkv.RAWcooked/${file}"

        # check overwrite
        rm "${file}.mkv.RAWcooked/${file}"
        touch "${file}.mkv.RAWcooked/${file}"
        run_rawcooked -y --conch --decode "${file}.mkv"
        if ! check_failure "mkv overwrite failed" "mkv overwrited" ; then
            clean
            continue
        fi

        clean
    popd >/dev/null 2>&1
done < "${script_path}/avi.txt"

exit ${status}
