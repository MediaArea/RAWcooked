#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="gaps"

pushd "${files_path}" >/dev/null 2>&1
    file=gaps1
    mkdir -p "${file}/image1"
    mkdir -p "${file}/image2"
    mkdir -p "${file}/image3"
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -r 1 -t 1 -start_number 0 "${file}/image1/%06d.dpx" >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -r 1 -t 1 -start_number 6 "${file}/image1/%06d.dpx" >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -r 1 -t 2 -start_number 0 "${file}/image2/%06d.dpx" >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -r 1 -t 2 -start_number 0 "${file}/image3/%06d.dpx" >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -r 1 -t 2 -start_number 5 "${file}/image3/%06d.dpx" >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"

    run_rawcooked --accept-gaps --check "${file}"
    check_success "check failed on dpx gaps" "check succeded on dpx gaps"
    rm -f "${file}.mkv"

    run_rawcooked -y --check "${file}"
    check_success "check failed on dpx gaps with -y option" "check succeded on dpx gaps with -y option"
    rm -f "${file}.mkv"

    run_rawcooked -n --check "${file}"
    check_failure "check failed on dpx gaps with -n option" "check succeded on dpx gaps despite -n option"
    rm -f "${file}.mkv"

    run_rawcooked -n --accept-gaps --check "${file}"
    check_success "check failed on dpx gaps with -n and --accepts-gaps options" "check succeded on dpx gaps with -n and --accept-gaps option"
    rm -f "${file}.mkv"

    outdir=gaps1output
    mkdir -p "${outfile}"
    run_rawcooked -n --accept-gaps --check "${file}" -o "${outdir}/${file}.mkv"
    check_success "check failed on dpx gaps with -n and --accepts-gaps and -o options" "check succeded on dpx gaps with -n and --accept-gaps and -o options"
    rm -f "${outdir}/${file}.mkv"

    clean
popd >/dev/null 2>&1

exit ${status}
