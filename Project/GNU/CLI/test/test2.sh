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
    path="$(echo "${line}" | cut -d' ' -f1)"
    file="$(echo "${line}" | cut -s -d' ' -f2)"
    files="$(echo "${line}" | cut -s -d' ' -f2-)"
    test=$(basename "${path}")

    if [ -z "${file}" ] ; then
        file="../$(basename ${path})"
        files="../$(basename ${path})"
    fi

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        rcode=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
            cmdline=$(${valgrind} rawcooked ${files} 2>/dev/null)
            result=$?

            # check valgrind
            if [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
                cat valgrind.log >&${fd}
                rcode=1
            fi

            # check result
            if [ "${result}" -ne "0" ] ; then
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
            rm -f "${file}.rawcooked_reversibility_data"

            ${valgrind} rawcooked "${file}.mkv" >/dev/null 2>&1
            result=$?
            echo $result
            # check valgrind
            if  [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
                cat valgrind.log >&${fd}
                   rcode=1
            fi
            rm -f "${file}.mkv"

            # check result files
            if [ -d "${file}" ] ; then
                files=$(find "${file}" -type f -print)
            fi

            for f in ${files} ; do
                if [ ! -e  "${file}.mkv.RAWcooked/${f}" ] ; then
                    echo "NOK: ${test}/${f} is missing" >&${fd}
                    rcode=1
                    continue
                fi

                md5_a="$(${md5cmd} "${f}" | cut -d' ' -f1)" 2>/dev/null
                md5_b="$(${md5cmd} "${file}.mkv.RAWcooked/${f}" | cut -d' ' -f1)" 2>/dev/null

                if [ -n "${md5_a}" ] && [ -n "${md5_b}" ] && [ "${md5_a}" == "${md5_b}" ] ; then
                    echo "OK: ${test}/${f}, checksums match" >&${fd}
                    continue
                else
                    echo "NOK: ${path}/${f}, checksums mismatch" >&${fd}
                    rcode=1
                fi
            done
            rm -fr "${file}.mkv.RAWcooked"

    popd >/dev/null 2>&1
done < "${script_path}/test2.txt"

exit ${rcode}

