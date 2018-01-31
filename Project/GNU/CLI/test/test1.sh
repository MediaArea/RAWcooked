#!/bin/bash

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
    if [ ! -e "${files_path}/${path}/${file}" ] ; then
echo "NOT FOUND: ${files_path}/${path}/${file}"
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
            cmdline=$(${valgrind} rawcooked "${file}" 2>/dev/null)
            result=$?

            # check valgrind
            if [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
                cat valgrind.log >&${fd}
                rcode=1
            fi

            # check expected result
            if [ "${result}" -ne "0" ] ; then
                if [ "${want}" == "fail" ] ; then
                    echo "OK: ${test}/${file}" >&${fd}
                    continue
                else
                    echo "NOK: ${test}/${file}" >&${fd}
                    rcode=1
                    continue
                fi
            fi

            # check result file generation
            cmdline=$(echo -e "${cmdline}" | grep ffmpeg)
            if [ -z "${cmdline}" ] ; then
                echo "NOK: ${test}/${file}" >&${fd}
                rcode=1
                continue
            fi

            eval "${cmdline} </dev/null >/dev/null 2>&1" 

            if [ ! -e "${file}.mkv" ] ; then
                echo "NOK: ${test}/${file}" >&${fd}
                rcode=1
                continue
            fi

            ${valgrind} rawcooked "${file}.mkv" >/dev/null 2>&1
            result=$?
            
            # check valgrind
            if  [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
                cat valgrind.log >&${fd}
                rcode=1
            fi

            md5_a="$(md5sum "${file}" | cut -d' ' -f1 2>/dev/null)"
            md5_b="$(md5sum "${file}.mkv.RAWcooked/${file}" | cut -d' ' -f1 2>/dev/null)"

            # check result file
            if [ "${md5_a}" == "${md5_b}" ] && [ "${want}" == "pass" ] ; then
                echo "OK: ${test}/${file}" >&${fd}
                continue
            fi
            
            if [ "${want}" == "pass" ] ; then
                echo "NOK: ${file}" >&${fd}
                rcode=1
            elif  [ "${want}" == "fail" ] ; then
                echo "OK: ${test}/${file}" >&${fd}
            fi
    popd >/dev/null 2>&1
done < "${script_path}/test1.txt"

exit ${rcode}

