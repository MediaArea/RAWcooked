#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

# test disabled on Windows (chmod operation not permitted)
if [[ "${OSTYPE}" == "darwin"* ]] || [ -n "${WSL}" ] ; then
    exit 77
fi

test="overwrite"

# local helper functions
modify_value() {
    printf "%02x" "$((0x${1}^255))"
}

swap_byte() {
    local pos="${1}"
    local file="${2}"
    local buffer="$(xxd -u -p -l 1 -s ${pos} ${file})" || return 1
    buffer="$(printf %02x $((0x${buffer}^255)))" || return 1
    echo ${buffer} | xxd -r -p -l 1 -s ${pos} - ${file} || return 1
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

# case_1 remove one file for each file type
case_1() {

    local dpx_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/1_??.dpx | wc -l)" || fatal "internal" "unable to list dpx files"
    if [ "${dpx_count}" -gt "1" ] ; then
        local last_dpx="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/01_??.dpx | tail -n1)" || fatal "internal" "unable to list dpx files"
        rm -f ${last_dpx} || fatal "internal" " rm command failed"
    fi

    local waves_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.wav | wc -l)" || fatal "internal" "unable to list waves files"
    if [ "${waves_count}" -gt "1" ] ; then
        local last_wav="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.wav | tail -n1)" || fatal "internal" "unable to list waves files"
        rm -f ${last_wav} || fatal "internal" " rm command failed"
    fi

    local flac_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.flac | wc -l)" || fatal "internal" "unable to list flac files"
    if [ "${flac_count}" -gt "1" ] ; then
        local last_flac="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.flac | tail -n1)" || fatal "internal" "unable to list flac files"
        rm -f ${last_flac} || fatal "internal" " rm command failed"
    fi

    local attachments_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.txt | wc -l)" || fatal "internal" "unable to list attachments files"
    if [ "${attachments_count}" -gt "1" ] ; then
        local last_attachment="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.txt | tail -n1)" || fatal "internal" "unable to list attachments files"
        rm -f ${last_attachment} || fatal "internal" "rm command failed"
    fi
}

# case_2 add one file for each file type
case_2() {
    local dpx_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/1_??.dpx | wc -l)" || fatal "internal" "unable to list dpx files"
    if [ "${dpx_count}" -gt "0" ] ; then
        ffmpeg -nostdin -f lavfi -i testsrc=rate=1:duration=1 ${file}.mkv.RAWcooked/${file}/1_$((dpx_count+1)).dpx >/dev/null 2>&1|| fatal "internal" "ffmpeg command failed"
    fi

    local waves_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.wav | wc -l)" || fatal "internal" "unable to list waves files"
    if [ "${waves_count}" -gt "0" ] ; then
        ffmpeg -nostdin -f lavfi -i anoisesrc=duration=2 ${file}.mkv.RAWcooked/${file}/$((waves_count+1)).wav >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    fi

    local flac_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.flac | wc -l)" || fatal "internal" "unable to list flac files"
    if [ "${flac_count}" -gt "0" ] ; then
        ffmpeg -nostdin  -f lavfi -i anoisesrc=duration=2 ${file}.mkv.RAWcooked/${file}/$((flac_count+1)).flac >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    fi

    local attachments_count="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/*.txt | wc -l)" || fatal "internal" "unable to list attachments files"
    if [ "${attachments_count}" -gt "0" ] ; then
        echo "attachment $((attachments_count+1))" > ${file}.mkv.RAWcooked/${file}/$((attachments_count+1)).txt || fatal "internal" "unable to create file"
    fi
}

# case_3: corrupt some files
case_3() {
    local buffer
    local size

    if [ "$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/1_??.dpx | wc -l)" -ge "4" ] ; then
        # swap the first byte of the first file of the first dpx stream
        swap_byte 0 "${file}.mkv.RAWcooked/${file}/1_01.dpx" || fatal "internal" " unable to modify file"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_01.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_01.dpx" || fatal "internal" "file not modified"

        # swap the last byte of the second file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_02.dpx)" || fatal "internal" "stat command failed"
        swap_byte $((size-1)) "${file}.mkv.RAWcooked/${file}/1_02.dpx" || fatal "internal" " unable to modify file"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_02.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_02.dpx" || fatal "internal" "file not modified"

        # swap 10 bytes in the middle of the third file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_03.dpx)" || fatal "internal" "stat command failed"
        for pos in $(seq $((size/2-5)) $((size/2+4))) ; do
            swap_byte ${pos} "${file}.mkv.RAWcooked/${file}/1_03.dpx" || fatal "internal" " unable to modify file"
        done
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_03.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_03.dpx" || fatal "internal" "file not modified"

        # swap the first byte, 10 bytes in the middle and last the byte of the fourth file of the first dpx stream
        swap_byte 0 "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" " unable to modify file"

        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_04.dpx)" || fatal "internal" "stat command failed"
        swap_byte $((size-1)) "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" " unable to modify file"

        for pos in $(seq $((size/2-5)) $((size/2+4))) ; do
            swap_byte ${pos} "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" " unable to modify file"
        done
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_04.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_04.dpx" || fatal "internal" "file not modified"
    fi

    for stream in "wav" "flac" ; do
        if [ -e "${file}.mkv.RAWcooked/${file}/1.${stream}" ] ; then
            # swap 10 bytes in the middle of the first wave stream
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1.${stream})" || fatal "internal" "stat command failed"
            for pos in $(seq $((size/2-5)) $((size/2+4))) ; do
                swap_byte ${pos} "${file}.mkv.RAWcooked/${file}/1.${stream}" || fatal "internal" " unable to modify file"
            done
            ! cmp -s "${file}.mkv.RAWcooked/${file}/1.${stream}" "${file}.mkv.RAWcooked.orig/${file}/1.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/2.${stream}" ] ; then
            # swap the 128th byte of the second wave stream
            swap_byte 127 "${file}.mkv.RAWcooked/${file}/2.${stream}" || fatal "internal" " unable to modify file"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/2.${stream}" "${file}.mkv.RAWcooked.orig/${file}/2.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/3.${stream}" ] ; then
            # swap the first byte of the third wave stream
            swap_byte 0 "${file}.mkv.RAWcooked/${file}/3.${stream}" || fatal "internal" " unable to modify file"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/3.${stream}" "${file}.mkv.RAWcooked.orig/${file}/3.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/4.${stream}" ] ; then
            # swap the last byte of the fourth wave stream
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/4.${stream})" || fatal "internal" "stat command failed"
            swap_byte $((size-1)) "${file}.mkv.RAWcooked/${file}/4.${stream}" || fatal "internal" " unable to modify file"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/4.${stream}" "${file}.mkv.RAWcooked.orig/${file}/4.${stream}" || fatal "internal" "file not modified"
        fi
    done
}

# case_4: extra or missing bytes in some files
case_4() {
    local size

    if [ "$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/1_??.dpx | wc -l)" -ge "4" ] ; then
        # truncate the last file of the first dpx stream to 0 byte
        local last_dpx="$(ls 2>/dev/null -Ubad1 -- ${file}.mkv.RAWcooked/${file}/1_??.dpx | tail -n1)" || fatal "internal" "unable to list dpx files"
        > "${last_dpx}" || fatal "internal" "truncate failed"

        # remove the last byte of the first file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_01.dpx)" || fatal "internal" "stat command failed"
        dd if="${file}.mkv.RAWcooked/${file}/1_01.dpx" of="${file}.mkv.RAWcooked/${file}/1_01.dpx.new" bs=1 count=$((size-1)) >/dev/null 2>&1 || fatal "internal" "dd command failed"
        mv -f "${file}.mkv.RAWcooked/${file}/1_01.dpx.new" "${file}.mkv.RAWcooked/${file}/1_01.dpx" || fatal "internal" "mv command failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_01.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_01.dpx" || fatal "internal" "file not modified"

        # truncate the second file of the first dpx stream to half his size
        dd if="${file}.mkv.RAWcooked/${file}/1_02.dpx" of="${file}.mkv.RAWcooked/${file}/1_02.dpx.new" bs=1 count=$((size/2)) >/dev/null 2>&1 || fatal "internal" "dd command failed"
        mv -f "${file}.mkv.RAWcooked/${file}/1_02.dpx.new" "${file}.mkv.RAWcooked/${file}/1_02.dpx" || fatal "internal" "mv command failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_02.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_02.dpx" || fatal "internal" "file not modified"

        # add one byte at the end of the third file of the first dpx stream
        echo 1 >> "${file}.mkv.RAWcooked/${file}/1_03.dpx" || fatal "internal" "append failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_03.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_03.dpx" || fatal "internal" "file not modified"

        # duplicate the content of the fourth file of the first dpx stream
        size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1_04.dpx)" || fatal "internal" "stat command failed"
        dd if="${file}.mkv.RAWcooked/${file}/1_04.dpx" of="${file}.mkv.RAWcooked/${file}/1_04.dpx.new" bs=1 count=${size} >/dev/null 2>&1 || fatal "internal" "dd command failed"
        dd if="${file}.mkv.RAWcooked/${file}/1_04.dpx" of="${file}.mkv.RAWcooked/${file}/1_04.dpx.new" bs=1 count=${size} seek=${size} >/dev/null 2>&1 || fatal "internal" "dd command failed"
        mv -f "${file}.mkv.RAWcooked/${file}/1_04.dpx.new" "${file}.mkv.RAWcooked/${file}/1_04.dpx" || fatal "internal" "mv command failed"
        ! cmp -s "${file}.mkv.RAWcooked/${file}/1_04.dpx" "${file}.mkv.RAWcooked.orig/${file}/1_04.dpx" || fatal "internal" "file not modified"
    fi

    for stream in "wav" "flac" ; do
        if [ -e "${file}.mkv.RAWcooked/${file}/1.${stream}" ] ; then
            # remove the last byte of the first wave file
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/1.${stream})" || fatal "internal" "stat command failed"
            dd if="${file}.mkv.RAWcooked/${file}/1.${stream}" of="${file}.mkv.RAWcooked/${file}/1.${stream}.new" bs=1 count=$((size-1)) >/dev/null 2>&1 || fatal "internal" "dd command failed"
            mv -f "${file}.mkv.RAWcooked/${file}/1.${stream}.new" "${file}.mkv.RAWcooked/${file}/1.${stream}" || fatal "internal" "mv command failed"
           ! cmp -s "${file}.mkv.RAWcooked/${file}/1.${stream}" "${file}.mkv.RAWcooked.orig/${file}/1.${stream}" || fatal "internal" "file not modified"
        fi

        if [ -e "${file}.mkv.RAWcooked/${file}/2.${stream}" ] ; then
            # truncate the second wave file to half his size
            size="$(${fsize} ${file}.mkv.RAWcooked/${file}/2.${stream})" || fatal "internal" "stat command failed"
            dd if="${file}.mkv.RAWcooked/${file}/2.${stream}" of="${file}.mkv.RAWcooked/${file}/2.${stream}.new" bs=1 count=$((size/2)) >/dev/null 2>&1 || fatal "internal" "dd command failed"
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
            dd if="${file}.mkv.RAWcooked/${file}/4.${stream}" of="${file}.mkv.RAWcooked/${file}/4.${stream}.new" bs=1 count=${size} >/dev/null 2>&1 || fatal "internal" "dd command failed"
            dd if="${file}.mkv.RAWcooked/${file}/4.${stream}" of="${file}.mkv.RAWcooked/${file}/4.${stream}.new" bs=1 count=${size} seek=${size} >/dev/null 2>&1 || fatal "internal" "dd command failed"
            mv -f "${file}.mkv.RAWcooked/${file}/4.${stream}.new" "${file}.mkv.RAWcooked/${file}/4.${stream}" || fatal "internal" "mv command failed"
            ! cmp -s "${file}.mkv.RAWcooked/${file}/4.${stream}" "${file}.mkv.RAWcooked.orig/${file}/4.${stream}" || fatal "internal" "file not modified"
        fi
    done
}

access_readonly() {
    chmod -R 0440 "${file}.mkv.RAWcooked/${file}/"* || fatal "internal" "chmod command failed"
}

access_none() {
    chmod -R 0000 "${file}.mkv.RAWcooked/${file}/"* || fatal "internal" "chmod command failed"
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
    files="1 2 3 4"
    cases="normal 1 2 3 4"
    accesses="normal readonly none"
    commands="n y"

    for file in ${files} ; do
        file_${file}
        for case in ${cases} ; do
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
