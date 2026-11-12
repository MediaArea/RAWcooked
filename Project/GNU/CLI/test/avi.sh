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

# ffv1_vulkan: 4:2:2 video width not a multiple of 128 rejected before encoding (no Vulkan device needed)
test="v210_48000_24_2_S_LE (ffv1_vulkan)"
file="v210_48000_24_2_S_LE.avi"
pushd "${files_path}/Formats/AVI/Flavors/v210_48000_24_2_S_LE" >/dev/null 2>&1
    run_rawcooked -y --encode --file -c:v ffv1_vulkan "${file}"
    if check_failure "file rejected at input" "file accepted at input" ; then
        if contains "multiple" "${cmd_stderr}" && contains "of 128" "${cmd_stderr}" ; then
            echo "OK: ${test}, file rejected at input" >&${fd}
        else
            echo "NOK: ${test}, file rejected at input without the width message: ${cmd_stderr}" >&${fd}
            status=1
        fi
    fi
    if [ -e "${file}.mkv" ] ; then
        echo "NOK: ${test}, mkv created" >&${fd}
        status=1
    fi
    clean
popd >/dev/null 2>&1

exit ${status}
