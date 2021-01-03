#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="pcm"

pushd "${files_path}" >/dev/null 2>&1
    # create file
    file=pcm1
    mkdir "${file}"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 "${file}"/1.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    # encoding
    run_rawcooked -c:a copy -y --conch --encode "${file}"
    check_success "check failed with one pcm" "check succeded with one pcm"

    # check decoding
    run_rawcooked -y --conch --decode "${file}.mkv"
    if ! check_success "mkv decoding failed" "mkv decoded" ; then
        clean
        continue
    fi

    check_directories "${file}" "${file}.mkv.RAWcooked"

    clean
popd >/dev/null 2>&1

exit ${status}
