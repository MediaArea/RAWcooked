#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="valgrind"

# test disabled on macOS due to this bug: https://bugs.kde.org/show_bug.cgi?id=349128
if [[ "${OSTYPE}" == "darwin"* ]] || [ -n "${WSL}" ] ; then
    exit 77
fi

type -p valgrind >/dev/null 2>&1 || fatal "${test}" "valgrind command not found"

while read line ; do
    path="$(echo "${line}" | cut -d' ' -f1)"
    file="$(echo "${line}" | cut -s -d' ' -f2)"
    opts="$(echo "${line}" | cut -s -d' ' -f3-)"

    if [ -z "${file}" ] ; then
        file="../$(basename ${path})"
    fi

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
        # check encoding
        VALGRIND=1 run_rawcooked ${opts} --file "${files_path}/${path}/${file}"

        # check decoding
        VALGRIND=1 run_rawcooked "${files_path}/${path}/${file}.mkv"

        clean
    popd >/dev/null 2>&1
done < "${script_path}/valgrind.txt"

exit ${status}
