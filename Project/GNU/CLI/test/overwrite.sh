#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

# test disabled on macOS and in WSL (Windows build of RAWcooked tested from WSL), enabled in Git Bash on Windows
if [[ "${OSTYPE}" == "darwin"* ]] || [ -n "${WSL}" ] ; then
    exit 77
fi

test="overwrite"

# local helper functions
modify_value() {
    printf "%02x" "$((0x${1}^255))"
}

# invert the bits of ${2} bytes at the position ${1} of the file ${3}, with one read and one write (process creation is slow on Windows)
swap_bytes() {
    local pos="${1}"
    local count="${2}"
    local file="${3}"
    local buffer swapped= byte i
    buffer="$(xxd -u -p -c ${count} -l ${count} -s ${pos} ${file})" || return 1
    for ((i = 0; i < ${#buffer}; i += 2)) ; do
        printf -v byte %02x $((0x${buffer:i:2}^255))
        swapped="${swapped}${byte}"
    done
    xxd -r -p -l ${count} -s ${pos} - ${file} <<< "${swapped}" || return 1
}

# count and last of the files expanded by the shell from a pattern, as $(ls -d pattern | wc -l) and $(ls -d pattern | tail -n1)
list_files() {
    local f
    listed_count=0
    listed_last=
    for f in "${@}" ; do
        if [ -e "${f}" ] ; then
            listed_count=$((listed_count + 1))
            listed_last="${f}"
        fi
    done
}

# file_1: one ffv1
file_1() {
    mkdir 1
    ffmpeg -nostdin -f lavfi -i testsrc=duration=2 1/1_%02d.dpx >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    run_rawcooked 1 || fatal "internal" "rawcooked command failed"
    run_rawcooked 1.mkv -o 1.mkv.RAWcooked.orig || fatal "internal" "rawcooked command failed"
}

# file_2 one pcm
file_2() {
    mkdir 2
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 2/1.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    run_rawcooked -c:a copy 2 || fatal "internal" "rawcooked command failed"
    run_rawcooked 2.mkv -o 2.mkv.RAWcooked.orig || fatal "internal" "rawcooked command failed"
}

# file_3 one flac
file_3() {
    mkdir 3
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 3/1.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    run_rawcooked 3 || fatal "internal" "rawcooked command failed"
    run_rawcooked 3.mkv -o 3.mkv.RAWcooked.orig || fatal "internal" "rawcooked command failed"
}

# file_4: two ffv1, four waves, four flac, two attachments
file_4() {
    mkdir 4
    ffmpeg -nostdin -f lavfi -i testsrc=duration=2 4/1_%02d.dpx >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i testsrc=duration=2 4/2_%02d.dpx >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/1.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/2.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/3.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/4.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/1.flac >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/2.flac >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/3.flac >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 4/4.flac >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    echo "attachment 1" > 4/1.txt || fatal "internal" "unable to create file"
    echo "attachment 2" > 4/2.txt || fatal "internal" "unable to create file"

    run_rawcooked 4 || fatal "internal" "rawcooked command failed"
    run_rawcooked -o 4.mkv.RAWcooked.orig 4.mkv || fatal "internal" "rawcooked command failed"
}

# file_5: four pcm
file_5() {
    mkdir 5
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 5/1.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 5/2.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 5/3.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 5/4.wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"

    run_rawcooked -c:a copy 5 || fatal "internal" "rawcooked command failed"
    run_rawcooked -o 5.mkv.RAWcooked.orig 5.mkv || fatal "internal" "rawcooked command failed"
}

# case_1 remove one file for each file type
case_1() {

    list_files ${file}.mkv.RAWcooked/${file}/1_??.dpx
    local dpx_count="${listed_count}"
    if [ "${dpx_count}" -gt "1" ] ; then
        list_files ${file}.mkv.RAWcooked/${file}/01_??.dpx
        local last_dpx="${listed_last}"
        rm -f ${last_dpx} || fatal "internal" " rm command failed"
    fi

    list_files ${file}.mkv.RAWcooked/${file}/*.wav
    local waves_count="${listed_count}"
    if [ "${waves_count}" -gt "1" ] ; then
        list_files ${file}.mkv.RAWcooked/${file}/*.wav
        local last_wav="${listed_last}"
        rm -f ${last_wav} || fatal "internal" " rm command failed"
    fi

    list_files ${file}.mkv.RAWcooked/${file}/*.flac
    local flac_count="${listed_count}"
    if [ "${flac_count}" -gt "1" ] ; then
        list_files ${file}.mkv.RAWcooked/${file}/*.flac
        local last_flac="${listed_last}"
        rm -f ${last_flac} || fatal "internal" " rm command failed"
    fi

    list_files ${file}.mkv.RAWcooked/${file}/*.txt
    local attachments_count="${listed_count}"
    if [ "${attachments_count}" -gt "1" ] ; then
        list_files ${file}.mkv.RAWcooked/${file}/*.txt
        local last_attachment="${listed_last}"
        rm -f ${last_attachment} || fatal "internal" "rm command failed"
    fi
}

# case_2 add one file for each file type
case_2() {
    list_files ${file}.mkv.RAWcooked/${file}/1_??.dpx
    local dpx_count="${listed_count}"
    if [ "${dpx_count}" -gt "0" ] ; then
        ffmpeg -nostdin -f lavfi -i testsrc=rate=1:duration=1 ${file}.mkv.RAWcooked/${file}/1_$((dpx_count+1)).dpx >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"
    fi

    list_files ${file}.mkv.RAWcooked/${file}/*.wav
    local waves_count="${listed_count}"
    if [ "${waves_count}" -gt "0" ] ; then
        ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 ${file}.mkv.RAWcooked/${file}/$((waves_count+1)).wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    fi

    list_files ${file}.mkv.RAWcooked/${file}/*.flac
    local flac_count="${listed_count}"
    if [ "${flac_count}" -gt "0" ] ; then
        ffmpeg -nostdin  -f lavfi -i anoisesrc=duration=2 ${file}.mkv.RAWcooked/${file}/$((flac_count+1)).flac >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    fi

    list_files ${file}.mkv.RAWcooked/${file}/*.txt
    local attachments_count="${listed_count}"
    if [ "${attachments_count}" -gt "0" ] ; then
        echo "attachment $((attachments_count+1))" > ${file}.mkv.RAWcooked/${file}/$((attachments_count+1)).txt || fatal "internal" "unable to create file"
    fi
}

# case_3: corrupt some files
case_3() {
    local buffer
    local size

    list_files ${file}.mkv.RAWcooked/${file}/1_??.dpx
    if [ "${listed_count}" -ge "4" ] ; then
        # swap the first byte of the first file of the first dpx stream
        swap_bytes 0 1 "${file}.mkv.RAWcooked/${file}/1_01.dpx" || fatal "internal" " unable to modify file"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_01.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_01.dpx" || fatal "internal" "file not modified"

        # swap the last byte of the second file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_02.dpx)" || fatal "internal" "stat command failed"
        swap_bytes $((size-1)) 1 "${file}.mkv.RAWcooked/${file}/1_02.dpx" || fatal "internal" " unable to modify file"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_02.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_02.dpx" || fatal "internal" "file not modified"

        # swap 10 bytes in the middle of the third file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_03.dpx)" || fatal "internal" "stat command failed"
        swap_bytes $((size/2-5)) 10 "${file}.mkv.RAWcooked/${file}/1_03.dpx" || fatal "internal" " unable to modify file"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_03.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_03.dpx" || fatal "internal" "file not modified"

        # swap the first byte, 10 bytes in the middle and last the byte of the fourth file of the first dpx stream
        swap_bytes 0 1 "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" " unable to modify file"

        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_04.dpx)" || fatal "internal" "stat command failed"
        swap_bytes $((size-1)) 1 "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" " unable to modify file"

        swap_bytes $((size/2-5)) 10 "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" " unable to modify file"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_04.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_04.dpx" || fatal "internal" "file not modified"
    fi

    for stream in "wav" "flac" ; do
        if [ -e "${file}.mkv.RAWcooked/${file}/1.${stream}" ] ; then
            # swap 10 bytes in the middle of the first wave stream
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1.${stream})" || fatal "internal" "stat command failed"
            swap_bytes $((size/2-5)) 10 "${file}.mkv.RAWcooked/${file}/1.${stream}" || fatal "internal" " unable to modify file"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/1.${stream}" "${file}.mkv.RAWcooked.orig/${file}/1.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/2.${stream}" ] ; then
            # swap the 128th byte of the second wave stream
            swap_bytes 127 1 "${file}.mkv.RAWcooked/${file}/2.${stream}" || fatal "internal" " unable to modify file"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/2.${stream}" "${file}.mkv.RAWcooked.orig/${file}/2.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/3.${stream}" ] ; then
            # swap the first byte of the third wave stream
            swap_bytes 0 1 "${file}.mkv.RAWcooked/${file}/3.${stream}" || fatal "internal" " unable to modify file"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/3.${stream}" "${file}.mkv.RAWcooked.orig/${file}/3.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/4.${stream}" ] ; then
            # swap the last byte of the fourth wave stream
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/4.${stream})" || fatal "internal" "stat command failed"
            swap_bytes $((size-1)) 1 "${file}.mkv.RAWcooked/${file}/4.${stream}" || fatal "internal" " unable to modify file"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/4.${stream}" "${file}.mkv.RAWcooked.orig/${file}/4.${stream}" || fatal "internal" "file not modified"
        fi
    done
}

# case_4: extra or missing bytes in some files
case_4() {
    local size

    list_files ${file}.mkv.RAWcooked/${file}/1_??.dpx
    if [ "${listed_count}" -ge "4" ] ; then
        # truncate the last file of the first dpx stream to 0 byte
        list_files ${file}.mkv.RAWcooked/${file}/1_??.dpx
        local last_dpx="${listed_last}"
        > "${last_dpx}" || fatal "internal" "truncate failed"

        # remove the last byte of the first file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_01.dpx)" || fatal "internal" "stat command failed"
        head -c $((size-1)) "${file}.mkv.RAWcooked/${file}/1_01.dpx" >"${file}.mkv.RAWcooked/${file}/1_01.dpx.new" || fatal "internal" "head command failed"
        mv -f "${file}.mkv.RAWcooked/${file}/1_01.dpx.new" "${file}.mkv.RAWcooked/${file}/1_01.dpx" || fatal "internal" "mv command failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_01.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_01.dpx" || fatal "internal" "file not modified"

        # truncate the second file of the first dpx stream to half his size
        head -c $((size/2)) "${file}.mkv.RAWcooked/${file}/1_02.dpx" >"${file}.mkv.RAWcooked/${file}/1_02.dpx.new" || fatal "internal" "head command failed"
        mv -f "${file}.mkv.RAWcooked/${file}/1_02.dpx.new" "${file}.mkv.RAWcooked/${file}/1_02.dpx" || fatal "internal" "mv command failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_02.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_02.dpx" || fatal "internal" "file not modified"

        # add one byte at the end of the third file of the first dpx stream
        echo 1 >> "${file}.mkv.RAWcooked/${file}/1_03.dpx" || fatal "internal" "append failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_03.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_03.dpx" || fatal "internal" "file not modified"

        # duplicate the content of the fourth file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_04.dpx)" || fatal "internal" "stat command failed"
        cat "${file}.mkv.RAWcooked/${file}/1_04.dpx" "${file}.mkv.RAWcooked/${file}/1_04.dpx" >"${file}.mkv.RAWcooked/${file}/1_04.dpx.new" || fatal "internal" "cat command failed"
        mv -f "${file}.mkv.RAWcooked/${file}/1_04.dpx.new" "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" "mv command failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_04.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_04.dpx" || fatal "internal" "file not modified"
    fi

    for stream in "wav" "flac" ; do
        if [ -e "${file}.mkv.RAWcooked/${file}/1.${stream}" ] ; then
            # remove the last byte of the first wave file
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1.${stream})" || fatal "internal" "stat command failed"
            head -c $((size-1)) "${file}.mkv.RAWcooked/${file}/1.${stream}" >"${file}.mkv.RAWcooked/${file}/1.${stream}.new" || fatal "internal" "head command failed"
            mv -f "${file}.mkv.RAWcooked/${file}/1.${stream}.new" "${file}.mkv.RAWcooked/${file}/1.${stream}" || fatal "internal" "mv command failed"
           ! cmp -s "${file}.mkv.RAWcooked/${file}/1.${stream}" "${file}.mkv.RAWcooked.orig/${file}/1.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/2.${stream}" ] ; then
            # truncate the second wave file to half his size
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/2.${stream})" || fatal "internal" "stat command failed"
            head -c $((size/2)) "${file}.mkv.RAWcooked/${file}/2.${stream}" >"${file}.mkv.RAWcooked/${file}/2.${stream}.new" || fatal "internal" "head command failed"
            mv -f "${file}.mkv.RAWcooked/${file}/2.${stream}.new" "${file}.mkv.RAWcooked/${file}/2.${stream}" || fatal "internal" "mv command failed"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/2.${stream}" "${file}.mkv.RAWcooked.orig/${file}/2.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/3.${stream}" ] ; then
            # add one byte at the end of the third wave file
            echo 1 >> "${file}.mkv.RAWcooked/${file}/3.${stream}" || fatal "internal" "append failed"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/3.${stream}" "${file}.mkv.RAWcooked.orig/${file}/3.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/4.${stream}" ] ; then
            # duplicate the content of the fourth wave file
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/4.${stream})" || fatal "internal" "stat command failed"
            cat "${file}.mkv.RAWcooked/${file}/4.${stream}" "${file}.mkv.RAWcooked/${file}/4.${stream}" >"${file}.mkv.RAWcooked/${file}/4.${stream}.new" || fatal "internal" "cat command failed"
            mv -f "${file}.mkv.RAWcooked/${file}/4.${stream}.new" "${file}.mkv.RAWcooked/${file}/4.${stream}" || fatal "internal" "mv command failed"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/4.${stream}" "${file}.mkv.RAWcooked.orig/${file}/4.${stream}" || fatal "internal" "file not modified"
        fi
    done
}

case_5() {
    # corrupt last attachment
    list_files ${file}.mkv.RAWcooked/${file}/*.txt
    local attachments_count="${listed_count}"
    if [ "${attachments_count}" -gt "0" ] ; then
        echo "attachment X" > ${file}.mkv.RAWcooked/${file}/$((attachments_count)).txt || fatal "internal" "unable to update file"
    fi
}

access_readonly() {
    chmod -R 0440 "${file}.mkv.RAWcooked/${file}/"* || fatal "internal" "chmod command failed"
}

access_none() {
    deny_access "${file}.mkv.RAWcooked/${file}/"* || fatal "internal" "access rights change failed"
}

command_n() {
    local want="pass"
   if ([ "${access}" == "none" ]) ||
      ([ "${case}" != "normal" ] && [ "${case}" != "1" ] && [ "${case}" != "2" ]) ; then
       want="fail"
   fi

    cp -a "${file}.mkv.RAWcooked.orig" "${file}.mkv.RAWcooked" || fatal "internal" "cp command failed"

    if [ "${case}" != "normal" ] ; then
        case_${case}
    fi

    if [ "${access}" != "normal" ] ; then
        access_${access}
    fi

    run_rawcooked -n "${file}.mkv"
    if [ "${want}" == "pass" ] ; then
        check_success "${case}/${access}/${command}, decoding failed unexpectedly" "${case}/${access}/${command}, decoding succeeded"
        if [ "${case}" != "2" ] ; then
            check_directories "${file}" "${file}.mkv.RAWcooked" -n
        fi
    else
        if check_failure "${case}/${access}/${command}, decoding failed" "${case}/${access}/${command}, decoding succeeded unexpectedly" ; then
            if ! contains "Error: undecodable" "${cmd_stderr}" ; then
                echo "NOK: ${test}/${file}, ${case}/${access}/${command}, invalid error message, ${cmd_stderr}" >&${fd}
                status=1
            fi
        fi
    fi

    rm -fr "${file}.mkv.RAWcooked" || fatal "internal" "rm command failed"
}

command_y() {
    local want="pass"
    if ([ "${access}" == "none" ]) ||
       ([ "${access}" == "readonly" ] && ([ "${case}" != "normal" ] && [ "${case}" != "1" ] && [ "${case}" != "2" ])) ; then
        want="fail"
    fi

    cp -a "${file}.mkv.RAWcooked.orig" "${file}.mkv.RAWcooked" || fatal "internal" "cp command failed"

    if [ "${case}" != "normal" ] ; then
        case_${case}
    fi

    if [ "${access}" != "normal" ] ; then
        access_${access}
    fi

    run_rawcooked -y "${file}.mkv"
    if [ "${want}" == "pass" ] ; then
        check_success "${case}/${access}/${command}, decoding failed unexpectedly" "${case}/${access}/${command}, decoding succeeded"
        if [ "${case}" != "2" ] ; then
            check_directories "${file}" "${file}.mkv.RAWcooked" -n
        fi
    else
        if check_failure "${case}/${access}/${command}, decoding failed" "${case}/${access}/${command}, decoding succeeded unexpectedly" ; then
            if ! contains "Error: undecodable" "${cmd_stderr}" ; then
                echo "NOK: ${test}/${file}, ${case}/${access}/${command}, invalid error message, ${cmd_stderr}" >&${fd}
                status=1
            fi
        fi
    fi

    rm -fr "${file}.mkv.RAWcooked" || fatal "internal" "rm command failed"
}

pushd "${files_path}" >/dev/null 2>&1
    files="1 2 3 4 5"
    cases="normal 1 2 3 4 5"
    accesses="normal readonly none"
    commands="n y"

    for file in ${files} ; do
        file_${file}
        for case in ${cases} ; do
            ([ "${case}" == "5" ] && [ "${file}" != "4" ] ) && continue
            for access in ${accesses} ; do
                for command in ${commands} ; do
                    command_${command}
                done
            done
        done
    done

    clean
popd >/dev/null 2>&1

exit ${status}
