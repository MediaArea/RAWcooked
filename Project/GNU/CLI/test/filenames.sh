#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="filenames"

# names with non-ASCII characters in the directory, the image files, the attachments, a subdirectory and a hash file
# (Windows: characters in the ANSI code page or not, attachment names written in UTF-8 by FFmpeg)

# directory ${file} with 3 image files ${1}0001.dpx to ${1}0003.dpx, a hash file of them and the attachments ${2}..., encoded, checked and decoded
check_names() {
    local prefix="${1}" name count
    shift
    local names=("${prefix}0001.dpx" "${prefix}0002.dpx" "${prefix}0003.dpx" "checksums.md5" "${@}")

    mkdir -p "${file}" || fatal "internal" "mkdir command failed"
    ffmpeg -nostdin -f lavfi -i testsrc=size=16x16 -r 1 -t 3 -start_number 1 "${file}/${prefix}%04d.dpx" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    pushd "${file}" >/dev/null 2>&1
        ${md5cmd} "${prefix}0001.dpx" "${prefix}0002.dpx" "${prefix}0003.dpx" > checksums.md5 || fatal "internal" "md5 command failed"
    popd >/dev/null 2>&1
    for name in "${@}" ; do
        mkdir -p "$(dirname "${file}/${name}")" || fatal "internal" "mkdir command failed"
        printf '%s\n' "${name}" > "${file}/${name}" || fatal "internal" "printf command failed"
    done

    run_rawcooked --check "${file}"
    if check_success "encoding failed" "encoded" ; then
        run_rawcooked --decode "${file}.mkv"
        if check_success "decoding failed" "decoded" ; then
            for name in "${names[@]}" ; do
                if [ -e "${file}.mkv.RAWcooked/${file}/${name}" ] ; then
                    check_files "${file}/${name}" "${file}.mkv.RAWcooked/${file}/${name}"
                else
                    echo "NOK: ${test}/${file}/${name}, missing in the decoded files" >&${fd}
                    status=1
                fi
            done
            count=$(( $(find "${file}.mkv.RAWcooked" -type f | wc -l) ))
            if [ "${count}" -ne "${#names[@]}" ] ; then
                echo "NOK: ${test}/${file}, ${count} decoded files instead of ${#names[@]}" >&${fd}
                status=1
            fi
        fi
    fi
}

pushd "${files_path}" >/dev/null 2>&1
    # Latin characters (in Windows-1252)
    file="filenames_été" # no space, run_rawcooked splits its arguments
    check_names "image_é_" "notes d'opérateur.txt" "sous-dossier_ü/à lire.txt"

    # Cyrillic and Japanese characters (not in Windows-1252)
    file="filenames_ж_日本"
    check_names "кадр_" "заметки.txt" "メモ/説明.txt"

    clean
popd >/dev/null 2>&1

exit ${status}
