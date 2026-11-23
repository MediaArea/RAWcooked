#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="noblock"

# Matroska file without any block (e.g. all blocks removed by a remux): the parts of the files in the reversibility data are output and the missing content is an error

pushd "${files_path}" >/dev/null 2>&1
    # audio: header of the file output, audio data missing
    while read -r ext options ; do
        file="noblock_${ext}_${options//[^a-z0-9]/}"

        # create file: 1 second, 192000 bytes of audio data
        mkdir "${file}"
        ffmpeg -nostdin -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=1" -ac 2 "${file}/1.${ext}" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

        # encoding
        run_rawcooked ${options} -y --encode "${file}"
        if ! check_success "encoding failed" "encoded" ; then
            clean
            continue
        fi

        # remux without any block
        ffmpeg -nostdin -i "${file}.mkv" -map 0 -c copy -t 0 "${file}_noblock.mkv" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

        run_rawcooked -y --check "${file}_noblock.mkv"
        if check_failure "missing audio rejected" "missing audio not detected" ; then
            contains "(0 bytes instead of 192000)" "${cmd_stderr}" || { echo "NOK: ${test}/${file}, missing audio, unexpected message: ${cmd_stderr}" >&${fd} ; status=1 ; }
        fi

        run_rawcooked -y -o "${file}_noblock.out" "${file}_noblock.mkv"
        if check_failure "missing audio rejected at decoding" "missing audio not detected at decoding" ; then
            # header of the file (all except the audio data)
            output="${file}_noblock.out/${file}/1.${ext}"
            if [ ! -f "${output}" ] ; then
                echo "NOK: ${test}/${file}, header not output" >&${fd}
                status=1
            else
                header_size=$(( $(${fsize} "${file}/1.${ext}") - 192000 ))
                if [ "$(${fsize} "${output}")" -ne "${header_size}" ] || ! head -c ${header_size} "${file}/1.${ext}" | cmp -s - "${output}" ; then
                    echo "NOK: ${test}/${file}, header not same as in the source" >&${fd}
                    status=1
                fi
            fi
        fi

        clean
    done <<EOF
wav
wav -c:a copy
aiff
EOF

    # video: all frames missing
    file="noblock_dpx"
    mkdir "${file}"
    ffmpeg -nostdin -f lavfi -i "testsrc=size=64x48:rate=24" -frames:v 3 -pix_fmt rgb48be "${file}/%04d.dpx" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    run_rawcooked -y --encode "${file}"
    if check_success "encoding failed" "encoded" ; then
        ffmpeg -nostdin -i "${file}.mkv" -map 0 -c copy -t 0 "${file}_noblock.mkv" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
        run_rawcooked -y --check "${file}_noblock.mkv"
        if check_failure "missing frames rejected" "missing frames not detected" ; then
            contains "missing frame" "${cmd_stderr}" && contains "0001.dpx" "${cmd_stderr}" && contains "0003.dpx" "${cmd_stderr}" || { echo "NOK: ${test}/${file}, missing frames, unexpected message: ${cmd_stderr}" >&${fd} ; status=1 ; }
        fi
    fi
    clean

    # no reversibility data
    file="noblock_noreversibility"
    ffmpeg -nostdin -f lavfi -i "testsrc=size=64x48:rate=24" -frames:v 3 -c:v ffv1 "${file}.mkv" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -i "${file}.mkv" -map 0 -c copy -t 0 "${file}_noblock.mkv" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    run_rawcooked -y --check "${file}_noblock.mkv"
    if check_failure "no reversibility data rejected" "no reversibility data not detected" ; then
        contains "No reversibility data found" "${cmd_stderr}" || { echo "NOK: ${test}/${file}, no reversibility data, unexpected message: ${cmd_stderr}" >&${fd} ; status=1 ; }
    fi
    clean
popd >/dev/null 2>&1

exit ${status}
