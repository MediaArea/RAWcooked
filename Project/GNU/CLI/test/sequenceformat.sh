#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="sequenceformat"

# the files of a sequence must have the flavor, the frame size and the orientation of the first file, else FFmpeg converts their frames

# file of a sequence, ${1}: file name, ${2}: frame size, other arguments: ffmpeg options
make_file() {
    local name="${1}"
    local size="${2}"
    local options=
    shift 2
    case "${name}" in
        *.tif) options="-compression_algo raw" ;;
        *.exr) options="-compression none -format half" ;;
    esac
    ffmpeg -nostdin -f lavfi -i testsrc=size=${size} -frames:v 1 ${options} "${@}" "${name}" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
}

# sequence ${file} accepted, with one or several threads for the analysis of the files
check_accepted() {
    local io_threads
    for io_threads in "" "--io-threads 1" ; do
        run_rawcooked ${io_threads} --check "${file}"
        check_success "sequence rejected ${io_threads}" "sequence accepted ${io_threads}"
        rm -f "${file}.mkv"
    done
}

# sequence ${file} rejected with the message ${1}, with one or several threads for the analysis of the files
check_rejected() {
    local message="${1}"
    local io_threads
    for io_threads in "" "--io-threads 1" ; do
        run_rawcooked ${io_threads} --check "${file}"
        if check_failure "sequence rejected ${io_threads}" "sequence accepted despite ${message} ${io_threads}" ; then
            if ! contains "${message}" "${cmd_stderr}" ; then
                echo "NOK: ${test}/${file}, sequence rejected without \"${message}\" ${io_threads}: ${cmd_stderr}" >&${fd}
                status=1
            elif [ -e "${file}.mkv" ] ; then
                echo "NOK: ${test}/${file}, ${file}.mkv created ${io_threads}" >&${fd}
                status=1
            fi
        fi
        rm -f "${file}.mkv"
    done
}

pushd "${files_path}" >/dev/null 2>&1
    # same flavor and frame size (not EXR, its encoding depends on the FFmpeg version)
    for ext in dpx tif ; do
        file=same_${ext}
        mkdir "${file}"
        make_file "${file}/0001.${ext}" 32x16
        make_file "${file}/0002.${ext}" 32x16
        check_accepted
    done

    # frame size not same as the first file
    for ext in dpx tif exr ; do
        file=framesize_${ext}
        mkdir "${file}"
        make_file "${file}/0001.${ext}" 32x16
        make_file "${file}/0002.${ext}" 16x16
        check_rejected "0002.${ext} (16x16 instead of 32x16)"
    done

    # flavor not same as the first file
    file=flavor_dpx
    mkdir "${file}"
    make_file "${file}/0001.dpx" 16x16
    make_file "${file}/0002.dpx" 16x16 -pix_fmt rgb48be
    check_rejected "flavor (not same as the first file of the sequence)"

    file=flavor_tif
    mkdir "${file}"
    make_file "${file}/0001.tif" 16x16
    make_file "${file}/0002.tif" 16x16 -pix_fmt rgb48le
    check_rejected "flavor (not same as the first file of the sequence)"

    # orientation not same as the first file, 1 is right to left (flavor of 8-bit RGB DPX without vertical flip)
    file=orientation_dpx
    mkdir "${file}"
    make_file "${file}/0001.dpx" 16x16
    make_file "${file}/0002.dpx" 16x16
    perl -0777 -pi -e 'substr($_, 768, 1, "\x01")' "${file}/0002.dpx" || fatal "internal" "perl command failed" # Little endian orientation
    check_rejected "0002.dpx (1 instead of 0)"

    # unsupported flavor after a supported one (the flavor of the previous file must not be kept)
    file=unsupported_tif
    mkdir "${file}"
    make_file "${file}/0001.tif" 16x16
    make_file "${file}/0002.tif" 16x16 -pix_fmt yuv444p
    check_rejected "unsupported TIFF flavor"

    clean
popd >/dev/null 2>&1

exit ${status}
