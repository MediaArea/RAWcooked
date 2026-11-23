#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="audiosize"

# The decoded audio data must have the size declared in the header of the file, else audio blocks are missing or extra in the compressed file

pushd "${files_path}" >/dev/null 2>&1
    while read -r ext options ; do
        file="audiosize_${ext}_${options//[^a-z0-9]/}"

        # create files: 1 second and 2 seconds
        mkdir "${file}" "${file}_long"
        ffmpeg -nostdin -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=1" -ac 2 "${file}/1.${ext}" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
        ffmpeg -nostdin -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=2" -ac 2 "${file}_long/1.${ext}" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

        # encoding
        run_rawcooked ${options} -y --encode "${file}"
        if ! check_success "encoding failed" "encoded" ; then
            clean
            continue
        fi
        run_rawcooked ${options} -y --encode "${file}_long"
        if ! check_success "encoding failed" "encoded" ; then
            clean
            continue
        fi

        # decoding of the original mkv, no error
        run_rawcooked -y --check "${file}.mkv"
        check_success "check failed" "checked"

        # missing audio blocks (audio cut in a remux)
        ffmpeg -nostdin -i "${file}.mkv" -map 0 -c copy -t 0.5 "${file}_short.mkv" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
        run_rawcooked -y --check "${file}_short.mkv"
        if check_failure "missing audio rejected" "missing audio not detected" ; then
            contains "audio size" "${cmd_stderr}" || { echo "NOK: ${test}/${file}, missing audio, unexpected message: ${cmd_stderr}" >&${fd} ; status=1 ; }
        fi
        run_rawcooked -y -o "${file}_short.out" "${file}_short.mkv"
        check_failure "missing audio rejected at decoding" "missing audio not detected at decoding"

        # extra audio blocks (audio of the 2-second file with the reversibility data of the 1-second file)
        ffmpeg -nostdin -i "${file}_long.mkv" -i "${file}.mkv" -map 0:a -map 1:t -c copy "${file}_extra.mkv" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
        run_rawcooked -y --check "${file}_extra.mkv"
        if check_failure "extra audio rejected" "extra audio not detected" ; then
            contains "audio size" "${cmd_stderr}" || { echo "NOK: ${test}/${file}, extra audio, unexpected message: ${cmd_stderr}" >&${fd} ; status=1 ; }
        fi

        clean
    done <<EOF
wav
wav -c:a copy
aiff
EOF
popd >/dev/null 2>&1

exit ${status}
