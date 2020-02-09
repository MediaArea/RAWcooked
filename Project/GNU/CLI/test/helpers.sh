#!/usr/bin/env bash

if type -p md5sum ; then
    md5cmd="md5sum"
elif type -p md5 ; then
    md5cmd="md5 -r"
else
    fatal "internal" "command not found for md5sum" >&${fd}
fi >/dev/null 2>&1

WSL=
if grep -q Microsoft /proc/version; then
  WSL=1
fi

case "${OSTYPE}" in
    linux*)
        fsize="stat -c %s"
        ;;
    darwin*|bsd*)
        fsize="stat -f %z"
        ;;
    *)
        fatal "internal" "unsupported platform"
        ;;
esac

fd=1
if (command exec >&9) ; then
    fd=9
fi >/dev/null 2>&1

export PATH="${PWD}:$PATH"

files_path="${PWD}/test/TestingFiles"

status=0

timeout=480

fatal() {
    local test="${1}"
    local message="${2}"

    echo "NOK: ${test}, ${message}" >&${fd}
    status=1
    clean

    exit ${status}
}

clean() {
     pushd "${files_path}" >/dev/null 2>&1
        local output="$(git rev-parse --show-cdup)"
        if [ "${?}" -ne "0" ] || [ -n "${output}" ] ; then
            echo "NOK: internal, ${files_path} is not a git repository" >&${fd}
            status=1
            return ${status}
        fi
        git clean -fdx >/dev/null 2>&1
    popd >/dev/null 2>&1
}

contains() {
    echo "${2}" | grep -q "${1}"
}

check_success() {
    local rejected_message="${1}"
    local accepted_message="${2}"
    local local_status=0

    if [ "${cmd_status}" -ne "0" ] ; then
        if [ -n "${cmd_stderr}" ] ; then
            echo "NOK: ${test}/${file}, ${rejected_message}, ${cmd_stderr}" >&${fd}
        else
            echo "NOK: ${test}/${file}, ${rejected_message} without message" >&${fd}
        fi
        status=1
        local_status=1
    elif contains "Error:" "${cmd_stderr}" ; then
        echo "NOK: ${test}/${file}, "${accepted_message}" with error message: ${cmd_stderr}" >&${fd}
        status=1
        local_status=1
    fi

    return ${local_status}
}

check_failure() {
    local rejected_message="${1}"
    local accepted_message="${2}"
    local local_status=0

    if [ "${cmd_status}" -eq "0" ] ; then
        echo "NOK: ${test}/${file}, ${accepted_message}" >&${fd}
        status=1
        local_status=1
    else
        if [ ! -n "${cmd_stderr}" ] ; then
            echo "NOK: ${test}/${file}, ${rejected_message} without message" >&${fd}
            status=1
            local_status=1
        fi
    fi

    return ${local_status}
}

check_framemd5() {
    local raw="${1}"
    local mkv="${2}"
    local local_status=0

    local media=$(ffmpeg -hide_banner -i "${raw}" 2>&1 </dev/null | tr -d ' ' | grep -m1 '^Stream#.\+:.\+' | cut -d ':' -f 3)
    if [ "${media}" == "Video" ] ; then
        pixfmt=$(ffmpeg -hide_banner -i "${raw}" 2>&1 </dev/null | tr -d ' ' | grep -m1 'Stream#.\+:.\+:Video:.\+,' | cut -d, -f2)
        ffmpeg -i "${raw}" -f framemd5 "${raw}.framemd5" </dev/null >/dev/null 2>&1
        ffmpeg -i "${mkv}" -pix_fmt ${pixfmt} -f framemd5 "${mkv}.framemd5" </dev/null >/dev/null 2>&1
    elif [ "${media}" == "Audio" ] ; then
        pcmfmt=$(ffmpeg -hide_banner -i "${raw}" 2>&1 </dev/null | tr -d ' ' | grep -m1 'Stream#.\+:.\+:Audio:' | grep -o 'pcm_[[:alnum:]]\+')
        ffmpeg -i "${raw}" -c:a ${pcmfmt} -f framemd5 "${raw}.framemd5" </dev/null >/dev/null 2>&1
        ffmpeg -i "${mkv}" -c:a ${pcmfmt} -f framemd5 "${mkv}.framemd5" </dev/null >/dev/null 2>&1
    else
        echo "NOK: ${test}/${file}, stream format not recognized" >&${fd}
        rm -f "${raw}.framemd5" "${mkv}.framemd5"
        status=1
        local_status=1
        return ${local_status}
    fi

    framemd5_a="$(grep -m1 -o '[0-9a-f]\{32\}' "${raw}.framemd5")"
    framemd5_b="$(grep -m1 -o '[0-9a-f]\{32\}' "${mkv}.framemd5")"
    rm -f "${raw}.framemd5" "${mkv}.framemd5"

    if [ -z "${framemd5_a}" ] || [ -z "${framemd5_b}" ] || [ "${framemd5_a}" != "${framemd5_b}" ] ; then
        echo "NOK: ${test}/${file}, framemd5 mismatch" >&${fd}
        status=1
        local_status=1
    fi

    return ${local_status}
}

check_files() {
    local original="${1}"
    local decoded="${2}"
    local local_status=0

     md5_a="$(${md5cmd} "${original}" | cut -d' ' -f1)" 2>/dev/null
     md5_b="$(${md5cmd} "${decoded}" | cut -d' ' -f1)" 2>/dev/null

     if [ -n "${md5_a}" ] && [ -n "${md5_b}" ] && [ "${md5_a}" == "${md5_b}" ] ; then
        if [ "${3}" != "-n" ] ; then
            echo "OK: ${test}/${original}, checksums match" >&${fd}
        fi
    else
        echo "NOK: ${test}/${original}, checksums mismatch" >&${fd}
        status=1
        local_status=1
    fi

    return ${local_status}
}

check_directories() {
    local local_status=0
    local directory1="${1}"
    local directory2="${2}"
    local files

    files=$(find "${directory1}" -type f  ! -empty -print)

    for f in ${files} ; do
        if [ ! -e  "${directory2}/${f}" ] ; then
            echo "NOK: ${test}/${f} is missing" >&${fd}
            local_status=1
            status=1
            continue
        fi
        if ! check_files "${f}" "${directory2}/${f}" "${3}"; then
            local_status=1
        fi
    done

    if [ "$(find ${directory2} -type f | wc -l)" -gt "$(echo ${files} | wc -w)" ] ; then
        echo "NOK: ${test}/${file}, unwanted files in ${directory2}" >&${fd}
        local_status=1
        status=1
    fi

    return ${local_status}
}

run_rawcooked() {
    unset cmd_status
    unset cmd_stdout
    unset cmd_stderr

    local temp="$(mktemp -d -t 'rawcooked_testsuite.XXXXXX')"

    local valgrind=""
    if command -v valgrind && test -z "${WSL}" && test -n "${VALGRIND}" ; then
        valgrind="valgrind --quiet --track-origins=yes --log-file=${temp}/valgrind"
    fi >/dev/null 2>&1

    if test -z "${WSL}" ; then
        ${valgrind} rawcooked $@ >"${temp}/stdout" 2>"${temp}/stderr" & kill -STOP ${!}; local pid=${!}
        sleep ${timeout} && (kill -HUP ${pid} ; fatal "command timeout: rawcooked $@") & local watcher=${!}
        kill -CONT ${pid} ; wait ${pid}
        cmd_status="${?}"
        pkill -P ${watcher}
        cmd_stdout="$(<${temp}/stdout)"
        cmd_stderr="$(<${temp}/stderr)"
    else
        rawcooked ${@////\\} >"${temp}/stdout" 2>"${temp}/stderr"
        cmd_status="${?}"
        cmd_stdout="$(tr -d '\r' <${temp}/stdout)"
        cmd_stderr="$(tr -d '\r' <${temp}/stderr)"
    fi

    # check valgrind
    if [ -n "${valgrind}" ] && [ -s "${temp}/valgrind" ] ; then
        cat "${temp}/valgrind" >&${fd}
        status=1
    fi

    rm -fr "${temp}"

    return ${cmd_status}
}

run_ffmpeg() {
    cmdline=$(echo -e "$@" | grep ffmpeg) 2>/dev/null
    if [ -n "${cmdline}" ] ; then
        eval "${cmdline} </dev/null >/dev/null 2>&1"
    fi
}
