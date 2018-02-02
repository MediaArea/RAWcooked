#!/bin/bash

PATH="${PWD}:$PATH"
script_path="${PWD}/test"
files_path="${script_path}/TestingFiles"

rcode=0

if which md5sum ; then
    md5cmd="md5sum"
elif which md5 ; then
    md5cmd="md5 -r"
else
    exit 1
fi >/dev/null 2>&1

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
            if [ "${want}" == "fail" ] && [ "${result}" -ne "0" ] ; then
                echo "OK: ${test}/${file}, file rejected at input" >&${fd}
                continue
            elif [ "${want}" == "fail" ] || [ "${result}" -ne "0" ] ; then
                echo "NOK: ${test}/${file}, file rejected at input" >&${fd}
                rcode=1
                continue
            fi

            # check result file generation
            cmdline=$(echo -e "${cmdline}" | grep ffmpeg) 2>/dev/null
            if [ -n "${cmdline}" ] ; then
                eval "${cmdline} </dev/null >/dev/null 2>&1"
            fi

            if [ ! -e "${file}.mkv" ] ; then
                echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
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

            md5_a="$(${md5cmd} "${file}" | cut -d' ' -f1)" 2>/dev/null
            md5_b="$(${md5cmd} "${file}.mkv.RAWcooked/${file}" | cut -d' ' -f1)" 2>/dev/null

            # check result file
            if [ -n "${md5_a}" ] && [ -n "${md5_b}" ] && [ "${md5_a}" == "${md5_b}" ] ; then
                echo "OK: ${test}/${file}, checksums match" >&${fd}
                continue
            else
                echo "NOK: ${path}/${file}, checksums mismatch" >&${fd}
                rcode=1
            fi
    popd >/dev/null 2>&1
done < "${script_path}/test1.txt"

exit ${rcode}

