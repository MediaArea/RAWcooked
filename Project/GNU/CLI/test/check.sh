#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="check"

# local helper functions
find() {
    local old="${1}" len="$((${#1} / 2))" file="${2}" size="$(${fsize} ${2})" occurence="${3}" chunk_size=256 pos=0 found=0
    while ((pos < size)) ; do
        local chunk="$(xxd -u -p -c ${chunk_size} -l ${chunk_size} -s ${pos} ${file})"
        local prefix="${chunk%%${old}*}"

        if ((${#prefix} < ${#chunk})) ; then
            ((pos += ${#prefix} / 2))
            ((found += 1))
            if ((found == occurence)) ; then
                break
            fi
            ((pos += len))
            continue
        fi
        ((pos += chunk_size - len + 1))
    done

    ((found == occurence)) && return 0

    return 1
}

find_and_replace() {
    local old="${1}" new="${2}" len="$((${#1} / 2))" file="${3}" size="$(${fsize} ${3})" occurence="${4}" chunk_size=256 pos=0 found=0
    while ((pos < size)) ; do
        local chunk="$(xxd -u -p -c ${chunk_size} -l ${chunk_size} -s ${pos} ${file})"
        local prefix="${chunk%%${old}*}"

        if ((${#prefix} < ${#chunk})) ; then
            ((pos += ${#prefix} / 2))
            ((found += 1))
            if ((found == occurence)) ; then
                break
            fi
            ((pos += len))
            continue
        fi
        ((pos += chunk_size - len + 1))
    done

    ((found == occurence)) || fatal "internal" "pattern not found"

    (echo "${new}" | xxd -r -p -l ${len} -s ${pos} - ${file}) || fatal "internal" "file patching failed"
}

find_and_truncate() {
    local pattern="${1}" len="$((${#1} / 2))" file="${2}" size="$(${fsize} ${2})" occurence="${3}" chunk_size=256 pos=0 found=0
    while ((pos < size)) ; do
        local chunk="$(xxd -u -p -c ${chunk_size} -l ${chunk_size} -s ${pos} ${file})"
        local prefix="${chunk%%${pattern}*}"

        if ((${#prefix} < ${#chunk})) ; then
            ((pos += ${#prefix} / 2))
            ((found += 1))
            if ((found == occurence)) ; then
                break
            fi
            ((pos += len))
            continue
        fi
        ((pos += chunk_size - len + 1))
    done

    ((found == occurence)) || fatal "internal" "pattern not found"
    dd if="${file}" of="${file}.new" bs=1 count=${pos} >/dev/null 2>&1 || fatal "internal" "dd command failed"
    mv -f "${file}.new" "${file}" || fatal "internal" "mv command failed"
}

pushd "${files_path}" >/dev/null 2>&1
    directory="${test}"
    file="${test}.mkv"

    mkdir "${directory}"|| fatal "internal" "mkdir command failed"

    # generate video and attachment source directory
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -t 1 -start_number 0 "${directory}/%03d.dpx" >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"
    echo "a" > "${directory}/attachment"

    run_rawcooked -y "${directory}"
    check_success "failed to generate mkv" "mkv generated"

    cp -f "${file}" "${file}.orig" || fatal "internal" "cp command failed"

    # valid mkv
    run_rawcooked --check "${file}" -o ./
    check_success "test failed on valid mkv with output set to source directory" "test succeeded on valid mkv with output set to source directory"
    run_rawcooked --check "${file}"
    check_success "test failed on valid mkv" "test succeeded on valid mkv"

    # missing video frames
    cp -f "${file}.orig" "${file}" || fatal "internal" "mv command failed"

    find_and_truncate "1F43B675" "${file}" 1
    run_rawcooked --check "${file}" -o ./
    check_failure "test failed on mkv with missing video frames (first occurence) and output set to source directory" "test succeeded on mkv with missing video frames (first occurence) and output set to source directory"
    run_rawcooked --check "${file}"
    check_failure "test failed on mkv with missing video frames (first occurence)" "test succeeded on mkv with missing video frames (first occurence)"

    cp -f "${file}.orig" "${file}" || fatal "internal" "mv command failed"

    find_and_truncate "1F43B675" "${file}" 2
    run_rawcooked --check "${file}" -o ./
    check_failure "test failed on mkv with missing video frames (second occurence) and output set to source directory" "test succeeded on mkv with missing video frames (second occurence) and output set to source directory"
    run_rawcooked --check "${file}"
    check_failure "test failed on mkv with missing video frames (second occurence)" "test succeeded on mkv with missing video frames (second occurence)"

    # missing attachment
    cp -f "${file}.orig" "${file}" || fatal "internal" "mv command failed"

    if find "61A70100" "${file}" 1 ; then
        find_and_replace "61A70100" "EC80EC02" "${file}" 1
    else
        find_and_replace "61A7" "42EC" "${file}" 1
    fi
    run_rawcooked --check "${file}" -o ./
    check_failure "test failed on mkv with missing attachment and output set to source directory" "test succeeded on mkv with missing attachment and output set to source directory"
    run_rawcooked --check "${file}"
    check_failure "test failed on mkv mkv with missing attachment" "test missing on mkv with modified attachment"

    # missing reversibility file
    cp -f "${file}.orig" "${file}" || fatal "internal" "mv command failed"

    if find "61A70100" "${file}" 2 ; then
        find_and_replace "61A70100" "EC80EC02" "${file}" 2
    else
        find_and_replace "61A7" "42EC" "${file}" 2
    fi
    run_rawcooked --check "${file}" -o ./
    check_failure "test failed on mkv with missing reversibility file and output set to source directory" "test succeeded on mkv with missing reversibility file and output set to source directory"
    run_rawcooked --check "${file}"
    check_failure "test failed on mkv mkv with missing reversibility file" "test succeeded on mkv with missing reversibility file"

    # missing attachment in source directory
    rm -f "${directory}/attachment" || fatal "internal" "rm command failed"
    run_rawcooked --check "${file}" -o ./
    check_failure "test failed on mkv with missing attachment in source directory" "test succeeded on mkv with missing attachment in source directory"

    # generate audio only source directory
    rm -f "${directory}/*" "${file}" "${file}.orig" || fatal "internal" "rm command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=60 "${directory}/audio.wav" >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"

    run_rawcooked -y "${directory}"
    check_success "failed to generate mkv" "mkv generated"

    cp -f "${file}" "${file}.orig" || fatal "internal" "cp command failed"

    # missing audio frames
    cp -f "${file}.orig" "${file}" || fatal "internal" "mv command failed"

    find_and_truncate "1F43B675" "${file}" 1
    run_rawcooked --check "${file}" -o ./
    check_failure "test failed on mkv with missing audio frames (first occurence) and output set to source directory" "test succeeded on mkv with missing audio frames (first occurence) and output set to source directory"
    run_rawcooked --check "${file}"
    check_failure "test failed on mkv with missing audio frames (first occurence)" "test succeeded on mkv with missing audio frames (first occurence)"

    # cleaning
    clean
popd >/dev/null 2>&1

exit ${status}
