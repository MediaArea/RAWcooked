#!/usr/bin/env bash

PATH="${PWD}:$PATH"
script_path="${PWD}/test"
files_path="${script_path}/TestingFiles"

rcode=0

fd=1
if command exec >&9 ; then
    fd=9
fi >/dev/null 2>&1

valgrind=""
#if command -v valgrind ; then
#    valgrind="valgrind --quiet --log-file=valgrind.log"
#fi >/dev/null 2>&1

while read line ; do
    file="$(basename "$(echo "${line}" | cut -d' ' -f1)")"
    path="$(dirname "$(echo "${line}" | cut -d' ' -f1)")"
    want="$(echo "${line}" | cut -d' ' -f2)"
    test=$(basename "${path}")

    pushd "${files_path}/${path}" >/dev/null 2>&1
        cmdline=$(${valgrind} rawcooked "${file}" 2>stderr)
        result=$?
        stderr="$(<stderr)"
        rm -f stderr

        # check valgrind
        if [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
            cat valgrind.log >&${fd}
            rcode=1
        fi

        # check expected result
        if [ "${want}" == "fail" ] && [ "${result}" -ne "0" ] ; then
            if [ -n "${stderr}" ] ; then
                echo "OK: ${test}/${file}, file rejected at input" >&${fd}
            else
                echo "NOK: ${test}/${file}, file rejected at input without message" >&${fd}
                rcode=1
            fi
            continue
        elif [ "${want}" == "fail" ] || [ "${result}" -ne "0" ] ; then
            echo "NOK: ${test}/${file}, file rejected at input" >&${fd}
            rcode=1
            continue
        fi

        echo "OK: ${test}/${file}, command succeeded" >&${fd}
    popd >/dev/null 2>&1
done < "${script_path}/test3.txt"

exit ${rcode}

