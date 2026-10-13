#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="longpath"

# file names longer than MAX_PATH (260 characters), the limit of the Windows API without the \\?\ prefix
# (Windows: the current directory must be shorter, else Git Bash does not start native programs)

# offset between the lengths of the Windows and Git Bash paths (e.g. 0 for /d/a vs D:\a)
offset=0
if test -n "${WINDOWS}" ; then
    windows_path="$(cygpath -w "${files_path}")"
    offset=$(( ${#windows_path} - ${#files_path} ))
fi

# directory ${1} with a subdirectory padded to a path of ${2} characters, in the variable named ${3}
padded_directory() {
    local path="${1}/long_path_"
    while [ "$(( ${#path} + offset ))" -lt "${2}" ] ; do
        path="${path}x"
    done
    printf -v "${3}" '%s' "${path}"
}

# directory ${1} with nested subdirectories up to a path of at least ${2} characters, in the variable named ${3}
nested_directory() {
    local path="${1}" i=0
    while [ "$(( ${#path} + offset ))" -lt "${2}" ] ; do
        i=$((i + 1))
        path="${path}/long_path_nested_directory_${i}_xxxxxxxxxxxxxxxxxxxx"
    done
    printf -v "${3}" '%s' "${path}"
}

pushd "${files_path}" >/dev/null 2>&1
    mkdir -p long_path_frames || fatal "internal" "mkdir command failed"
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -r 1 -t 3 -start_number 1 "long_path_frames/frame_%04d.dpx" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    # file in a directory of 230 characters: reversibility data and decoded file longer than 260 characters
    padded_directory "${files_path}" 230 directory
    mkdir -p "${directory}" || fatal "internal" "mkdir command failed"
    file=long_path_image.dpx
    cp long_path_frames/frame_0001.dpx "${directory}/${file}" || fatal "internal" "cp command failed"
    pushd "${directory}" >/dev/null 2>&1
        run_rawcooked -y --encode --file -d "${file}"
        if check_success "file rejected at input" "file accepted at input" ; then
            if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
                echo "NOK: ${test}/${file}, reversibility_data is missing" >&${fd}
                status=1
            elif ! run_ffmpeg "${cmd_stdout}" || [ ! -e "${file}.mkv" ] ; then
                echo "NOK: ${test}/${file}, mkv not generated, ${ffmpeg_stderr}" >&${fd}
                status=1
            else
                run_rawcooked -y --decode "${file}.mkv"
                if check_success "mkv decoding failed" "mkv decoded" ; then
                    check_files "${file}" "${file}.mkv.RAWcooked/${file}"
                fi
            fi
        fi
    popd >/dev/null 2>&1

    # sequence in a directory of more than 300 characters, decoded in a new directory of more than 300 characters
    # (names relative to the current directory, the full path is built by rawcooked)
    nested_directory "${files_path}" 300 directory
    nested_directory "${files_path}/long_path_output" 300 output
    directory="${directory#${files_path}/}"
    output="${output#${files_path}/}"
    file=sequence
    mkdir -p "${directory}/${file}" || fatal "internal" "mkdir command failed"
    cp long_path_frames/frame_000[123].dpx "${directory}/${file}/" || fatal "internal" "cp command failed"
    run_rawcooked -y --encode --check "${directory}/${file}"
    if check_success "file rejected at input" "file accepted at input" ; then
        if [ ! -e "${directory}/${file}.mkv" ] ; then
            echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
            status=1
        else
            run_rawcooked -y --decode "${directory}/${file}.mkv" -o "${output}"
            if check_success "mkv decoding failed" "mkv decoded" ; then
                frames_status=0
                for frame in frame_0001.dpx frame_0002.dpx frame_0003.dpx ; do
                    check_files "${directory}/${file}/${frame}" "${output}/${file}/${frame}" -n || frames_status=1
                done
                if [ "${frames_status}" -eq "0" ] ; then
                    echo "OK: ${test}/${file}, checksums match" >&${fd}
                fi
            fi
        fi
    fi

    rm -fr long_path_* # long paths, which git clean may not remove on Windows
    clean
popd >/dev/null 2>&1

exit ${status}
