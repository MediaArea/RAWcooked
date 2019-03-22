#!/usr/bin/env bash

PATH="${PWD}:$PATH"
script_path="${PWD}/test"
files_path="${script_path}/TestingFiles"
. /${script_path}/helpers.sh

rcode=0

while read line ; do
    file="$(basename "$(echo "${line}" | cut -d' ' -f1)")"
    path="$(dirname "$(echo "${line}" | cut -d' ' -f1)")"
    want="$(echo "${line}" | cut -d' ' -f2)"
    test=$(basename "${path}")

    pushd "${files_path}/${path}" >/dev/null 2>&1
        cmdline=$(${valgrind} rawcooked --file -d "${file}" 2>${script_path}/stderr)
        result=$?
        stderr="$(<${script_path}/stderr)"
        rm -f ${script_path}/stderr

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
        elif [ "${result}" -ne "0" ] ; then
            if [ -n "${stderr}" ] ; then
                echo "NOK: ${test}/${file}, file rejected at input, ${stderr}" >&${fd}
            else
                echo "NOK: ${test}/${file}, file rejected at input without message" >&${fd}
            fi
            rcode=1
            continue
        elif [ "${want}" == "fail" ] ; then
            echo "NOK: ${test}/${file}, file accepted at input" >&${fd}
            rcode=1
            continue
        elif ["${want}" == "pass" ] && [[ "${stderr}" == *"Error:"* ]] ; then
            echo "NOK: ${test}/${file}, file accepted at input with error message: ${stderr}" >&${fd}
            rcode=1
            continue
        fi

        echo "OK: ${test}/${file}, command succeeded" >&${fd}
    popd >/dev/null 2>&1
done < "${script_path}/test3.txt"

exit ${rcode}

