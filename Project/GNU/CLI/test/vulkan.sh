#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

# test disabled in WSL (Windows build of RAWcooked tested from WSL), enabled in Git Bash on Windows
if [ -n "${WSL}" ] ; then
    exit 77
fi

# CPU, a CPU implementation of Vulkan (e.g. lavapipe) generates code for this CPU
if [[ "${OSTYPE}" == "darwin"* ]] ; then
    cpu="$(sysctl -n machdep.cpu.brand_string 2>/dev/null)"
    [ "$(sysctl -n hw.optional.avx512f 2>/dev/null)" == "1" ] && avx512=yes || avx512=no
else
    cpu="$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2-)"
    grep -qw avx512f /proc/cpuinfo 2>/dev/null && avx512=yes || avx512=no
fi
echo "CPU: ${cpu# }, AVX-512: ${avx512}" >&${fd}

# Vulkan devices seen by the system, shown once if FFmpeg Vulkan encoding fails
show_vulkan_devices() {
    if [ -z "${vulkan_devices_shown}" ] ; then
        vulkan_devices_shown=1
        local devices="$(vulkaninfo --summary 2>&1 | grep -E 'deviceName|deviceType|driverInfo|apiVersion|ERROR|rror' | head -n 10 | tr -s ' \t' ' ' | tr '\n' ' ')"
        echo "Vulkan devices (vulkaninfo --summary): ${devices:-none}" >&${fd}
    fi
}

while read line ; do
    read -r entry want dummy <<< "${line}"
    file="${entry##*/}"
    path="${entry%/*}"
    test="${path##*/}"

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
        grep_lines source_warnings "Warning: " "${cmd_stderr}"

        # check result file generation
        encode_cmd="${cmd_stdout}" # cmd_stdout is overwritten by the decoding
        for variant in swdpx hwdpx ; do
            test="${path##*/} (${variant})"

            cmd="${encode_cmd/-xerror/-xerror -init_hw_device vulkan=vk:0,debug=0}"
            if [ "${variant}" == "hwdpx" ] ; then
               cmd="${cmd/-xerror/-xerror -hwaccel vulkan -hwaccel_output_format vulkan}"
            fi
            cmd="${cmd/-c:v ffv1/-vf hwupload -c:v ffv1_vulkan}"
            cmd="${cmd/-context 1/}"
            rm -f "${file}.mkv" # Not the file of the previous variant
            if ! run_ffmpeg "${cmd}" ; then
                exit_code="exit code ${ffmpeg_status}"
                if [ "${ffmpeg_status}" -gt 128 ] ; then
                    exit_code="${exit_code}, signal $(kill -l $((ffmpeg_status - 128)) 2>/dev/null)" # e.g. crash
                fi
                echo "NOK: ${test}/${file}, FFmpeg Vulkan encoding failed (${exit_code}): $(echo "${ffmpeg_stderr}" | tr '\n' ' ')" >&${fd}
                show_vulkan_devices
                status=1
                rm -f "${file}.mkv"
                continue
            fi

            if [ ! -e "${file}.mkv" ] ; then
                echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
                status=1
                continue
            fi

            # check decoding, by our decoder then by the decoder automatically selected with the Vulkan encoder
            for decoder in internal auto ; do
                test="${path##*/} (${variant}, ${decoder})"
                rm -fr "${file}.mkv.RAWcooked" # Not the files of the previous decoding
                run_rawcooked -y --conch --decoder ${decoder} -c:v ffv1_vulkan --decode "${file}.mkv"
                if ! check_success "mkv decoding failed" "mkv decoded" ; then
                    continue
                fi

                grep_lines decoded_warnings "Warning: " "${cmd_stderr}"
                if [ "${source_warnings}" != "${decoded_warnings}" ] ; then
                    echo "NOK: ${test}/${file}, warnings differs between the source and the decoded files" >&${fd}
                    status=1
                    continue
                fi
                check_files "${file}" "${file}.mkv.RAWcooked/${file}"
            done
            rm -fr "${file}.mkv" "${file}.mkv.RAWcooked"
        done
        clean
    popd >/dev/null 2>&1
done < "${script_path}/vulkan.txt"

exit ${status}
