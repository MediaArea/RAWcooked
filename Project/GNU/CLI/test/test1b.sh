#!/usr/bin/env bash

# same as test1 except ' --check-padding -slices 4' option

PATH="${PWD}:$PATH"
script_path="${PWD}/test"
files_path="${script_path}/TestingFiles"
. /${script_path}/helpers.sh

rcode=0

clean() {
    rm -fr "${file}.mkv.RAWcooked"
    rm -f "${file}.rawcooked_reversibility_data"
    rm -f "${file}.mkv"
}

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
        cmdline=$(${valgrind} rawcooked --file --check-padding -slices 4 -d "${file}" 2>${script_path}/stderr)
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

        # check result file generation
        cmdline=$(echo -e "${cmdline}" | grep ffmpeg) 2>/dev/null
        if [ -n "${cmdline}" ] ; then
            eval "${cmdline} </dev/null >/dev/null 2>&1"
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
        media=$(ffmpeg -hide_banner -i "${file}" 2>&1 </dev/null | tr -d ' ' | grep -m1 '^Stream#.\+:.\+' | cut -d ':' -f 3)
        if [ "${media}" == "Video" ] ; then
            pixfmt=$(ffmpeg -hide_banner -i "${file}" -f framemd5 "${file}.framemd5" 2>&1 </dev/null | tr -d ' ' | grep -m1 'Stream#.\+:.\+:Video:.\+,' | cut -d, -f2)
            ffmpeg -i "${file}.mkv" -pix_fmt ${pixfmt} -f framemd5 "${file}.mkv.framemd5" </dev/null
        elif [ "${media}" == "Audio" ] ; then
            pcmfmt=$(ffmpeg -hide_banner -i "${file}" 2>&1 </dev/null | tr -d ' ' | grep -m1 'Stream#.\+:.\+:Audio:' | grep -o 'pcm_[[:alnum:]]\+')
            ffmpeg -i "${file}" -c:a ${pcmfmt} -f framemd5 "${file}.framemd5" </dev/null
            ffmpeg -i "${file}.mkv" -c:a ${pcmfmt} -f framemd5 "${file}.mkv.framemd5" </dev/null
        else
            echo "NOK: ${test}/${file}, stream format not recognized" >&${fd}
            clean
            rcode=1
            continue
        fi

        framemd5_a="$(grep -m1 -o '[0-9a-f]\{32\}' "${file}.framemd5")"
        framemd5_b="$(grep -m1 -o '[0-9a-f]\{32\}' "${file}.mkv.framemd5")"
        rm -f "${file}.framemd5" "${file}.mkv.framemd5"

        if [ -z "${framemd5_a}" ] || [ -z "${framemd5_b}" ] || [ "${framemd5_a}" != "${framemd5_b}" ] ; then
            echo "NOK: ${test}/${file}, framemd5 mismatch" >&${fd}
            clean
            rcode=1
            continue
        fi

        ${valgrind} rawcooked "${file}.mkv" >/dev/null 2>${script_path}/stderr
        result=$?
        stderr="$(<${script_path}/stderr)"
        rm -f ${script_path}/stderr

        # check valgrind
        if  [ -n "${valgrind}" ] && [ -s "valgrind.log" ] ; then
            cat valgrind.log >&${fd}
            rcode=1
        fi

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

        md5_a="$(${md5cmd} "${file}" | cut -d' ' -f1)" 2>/dev/null
        md5_b="$(${md5cmd} "${file}.mkv.RAWcooked/${file}" | cut -d' ' -f1)" 2>/dev/null
        # check result file
        if [ -n "${md5_a}" ] && [ -n "${md5_b}" ] && [ "${md5_a}" == "${md5_b}" ] ; then
            echo "OK: ${test}/${file}, checksums match" >&${fd}
        else
            echo "NOK: ${path}/${file}, checksums mismatch" >&${fd}
            rcode=1
        fi
        clean
    popd >/dev/null 2>&1
done < "${script_path}/test1b.txt"

exit ${rcode}

