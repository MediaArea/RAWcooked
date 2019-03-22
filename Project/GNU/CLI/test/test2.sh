#!/usr/bin/env bash

PATH="${PWD}:$PATH"
script_path="${PWD}/test"
files_path="${script_path}/TestingFiles"
. /${script_path}/helpers.sh

rcode=0

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
        cmdline=$(${valgrind} rawcooked --file -d ${files} 2>${script_path}/stderr)
        result=$?
        stderr="$(${script_path}/<stderr)"
        rm -f ${script_path}/stderr

        if [ "${file: -1}" == "/" ] ; then
            file="${file:: -1}"
        fi

        if [ "${file}" == "." ] ; then
            file="../$(basename $(pwd))"
        fi

        # check valgrind
        if [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
            cat valgrind.log >&${fd}
            rcode=1
        fi

        # check result
        if [ "${result}" -ne "0" ] ; then
            if [ -n "${stderr}" ] ; then
                echo "NOK: ${test}/${file}, file rejected at input, ${stderr}" >&${fd}
            else
                echo "NOK: ${test}/${file}, file rejected at input without message" >&${fd}
            fi
            rcode=1
            continue
        elif [[ "${stderr}" == *"Error:"* ]] ; then
            echo "NOK: ${test}/${file}, file accepted at input with error message: ${stderr}" >&${fd}
            rcode=1
            continue
        fi

        # check result file generation
        cmdline=$(echo -e "${cmdline}" | grep ffmpeg) 2>/dev/null
        if [ -n "${cmdline}" ] ; then
            eval "${cmdline} </dev/null >/dev/null 2>&1"
        fi

        rm -f "${file}.rawcooked_reversibility_data"
        if [ ! -e "${file}.mkv" ] ; then
            echo "NOK: ${test}/${file}, mkv not generated" >&${fd}
            rcode=1
            continue
        fi

        ${valgrind} rawcooked "${file}.mkv" >/dev/null 2>${script_path}/stderr
        result=$?
        stderr="$(${script_path}/<stderr)"
        rm -f ${script_path}/stderr

        # check valgrind
        if  [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
             cat valgrind.log >&${fd}
             rcode=1
        fi
        rm -f "${file}.mkv"

        # check command result
        if [ "${result}" -eq "0" ] ; then
            if [[ "${stderr}" == *"Error:"* ]] ; then
                echo "NOK: ${test}/${file}, mkv decoded with error message" >&${fd}
                clean
                rcode=1
                continue
            fi
        else
            if [[ "${stderr}" == *"Error:"* ]] ; then
                echo "NOK: ${test}/${file}, mkv decoding failed, ${stderr}" >&${fd}
            else
                echo "NOK: ${test}/${file}, mkv decoding failed without message" >&${fd}
            fi
            clean
            rcode=1
            continue
        fi

        # check result files
        if [ -d "${file}" ] ; then
            files=$(find "${file}" -type f  ! -empty -print)
        else
            files="$(find * -path ${file}.mkv.RAWcooked -prune -o -type f ! -empty -print)"
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

        if [ "$(find ${file}.mkv.RAWcooked -type f | wc -l)" -gt "$(echo ${files} | wc -w)" ] ; then
            echo "NOK: ${test} unwanted files in ${file}.mkv.RAWcooked directory" >&${fd}
            rcode=1
        fi

        rm -fr "${file}.mkv.RAWcooked"

    popd >/dev/null 2>&1
done < "${script_path}/test2.txt"

exit ${rcode}

