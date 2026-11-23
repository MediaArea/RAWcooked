#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="flacblocksize"

# FLAC blocks bigger than the 16384 samples of the streamable subset (up to 65535), or bigger than the previous blocks: the decoded file is same as the source

pushd "${files_path}" >/dev/null 2>&1
    file="flacblocksize"

    # create file: 2 seconds
    mkdir "${file}"
    ffmpeg -nostdin -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=2" -ac 2 "${file}/1.wav" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    # encoding, for the reversibility data
    run_rawcooked -y --encode "${file}"
    if check_success "encoding failed" "encoded" ; then
        # same audio with other block sizes
        for blocksize in 32768 65535 ; do
            ffmpeg -nostdin -i "${file}/1.wav" -c:a flac -frame_size ${blocksize} "${file}_${blocksize}.mka" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
        done
        # 1152-sample blocks then 4608-sample blocks
        ffmpeg -nostdin -i "${file}/1.wav" -t 1 -c:a flac -frame_size 1152 "${file}_1.flac" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
        ffmpeg -nostdin -i "${file}/1.wav" -ss 1 -c:a flac -frame_size 4608 "${file}_2.flac" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
        printf "file '%s'\nfile '%s'\n" "${file}_1.flac" "${file}_2.flac" >"${file}_list.txt"
        ffmpeg -nostdin -f concat -i "${file}_list.txt" -c copy "${file}_increasing.mka" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

        for blocksize in 32768 65535 increasing ; do
            # audio with the reversibility data of the source
            ffmpeg -nostdin -i "${file}_${blocksize}.mka" -i "${file}.mkv" -map 0:a -map 1:t -c copy "${file}_${blocksize}.mkv" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

            run_rawcooked -y -o "${file}_${blocksize}.out" "${file}_${blocksize}.mkv"
            if check_success "decoding failed with ${blocksize} blocks" "decoded with ${blocksize} blocks" ; then
                check_files "${file}/1.wav" "${file}_${blocksize}.out/${file}/1.wav"
            fi
        done
    fi

    clean
popd >/dev/null 2>&1

exit ${status}
