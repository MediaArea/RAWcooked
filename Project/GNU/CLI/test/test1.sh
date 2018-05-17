#!/usr/bin/env bash

PATH="${PWD}:$PATH"
script_path="${PWD}/test"
files_path="${script_path}/TestingFiles"

rcode=0

if type -p md5sum ; then
    md5cmd="md5sum"
elif type -p md5 ; then
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
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        rcode=1
        continue
    fi

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

        # check framemd5
        pixfmt=$(ffmpeg -hide_banner -i "${file}" -f framemd5 "${file}.framemd5" 2>&1 </dev/null | tr -d ' ' | grep -m1 'Stream#.\+:.\+:Video:dpx,' | cut -d, -f2)
        ffmpeg -i "${file}.mkv" -pix_fmt ${pixfmt} -f framemd5 "${file}.mkv.framemd5" </dev/null

        framemd5_a="$(grep -m1 -o '[0-9a-f]\{32\}' "${file}.framemd5")"
        framemd5_b="$(grep -m1 -o '[0-9a-f]\{32\}' "${file}.mkv.framemd5")"
        rm -f "${file}.framemd5" "${file}.mkv.framemd5"

        if [ -z "${framemd5_a}" ] || [ -z "${framemd5_b}" ] || [ "${framemd5_a}" != "${framemd5_b}" ] ; then
            echo "NOK: ${test}/${file}, framemd5 mismatch" >&${fd}
            rcode=1
            rm -f "${file}.mkv"
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
        rm -fr "${file}.mkv.RAWcooked"

        # check result file
        if [ -n "${md5_a}" ] && [ -n "${md5_b}" ] && [ "${md5_a}" == "${md5_b}" ] ; then
            echo "OK: ${test}/${file}, checksums match" >&${fd}
        else
            echo "NOK: ${path}/${file}, checksums mismatch" >&${fd}
            rcode=1
        fi
    popd >/dev/null 2>&1
done < "${script_path}/test1.txt"

exit ${rcode}

