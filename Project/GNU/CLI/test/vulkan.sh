#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

# test disabled on macOS and Windows
if [[ "${OSTYPE}" == "darwin"* ]] || [ -n "${WSL}" ] ; then
    exit 77
fi

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
        if [[ ${file} == *".exr" ]]; then
            encode_extra="--check"
        fi
        run_rawcooked -y --conch --encode --file ${encode_extra} -d "${file}"

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
        for variant in swdpx hwdpx ; do
            test="$(basename "${path}") (${variant})"

            cmd_stdout="${cmd_stdout/-xerror/-xerror -init_hw_device vulkan=vk:0,debug=0}"
            if [ "${variant}" == "hwdpx" ] ; then
               cmd_stdout="${cmd_stdout/-xerror/-xerror -hwaccel vulkan -hwaccel_output_format vulkan}"
            fi
            cmd_stdout="${cmd_stdout/-c:v ffv1/-vf hwupload -c:v ffv1_vulkan}"
            cmd_stdout="${cmd_stdout/-context 1/}"
            run_ffmpeg "${cmd_stdout}"

            if [ ! -e "${file}.mkv" ] ; then
                echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
                status=1
                continue
            fi

            # check decoding
            run_rawcooked -y --conch --decode "${file}.mkv"
            if ! check_success "mkv decoding failed" "mkv decoded" ; then
                rm -f "${file}.mkv"
                continue
            fi

            decoded_warnings=$(echo "${cmd_stderr}" | grep "Warning: ")
            if [ "${source_warnings}" != "${decoded_warnings}" ] ; then
                echo "NOK: ${test}/${file}, warnings differs between the source and the decoded files" >&${fd}
                status=1
                rm -fr "${file}.mkv" "${file}.mkv.RAWcooked"
                continue
            fi
            check_files "${file}" "${file}.mkv.RAWcooked/${file}"
        done
        clean
    popd >/dev/null 2>&1
done < "${script_path}/vulkan.txt"

exit ${status}
