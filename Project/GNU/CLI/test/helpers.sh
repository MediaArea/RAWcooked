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

WINDOWS=
if [[ "${OSTYPE}" == "msys"* || "${OSTYPE}" == "cygwin"* ]] ; then
  WINDOWS=1 # Git Bash, msys or cygwin depending on its version
fi

case "${OSTYPE}" in
    linux*|cygwin*|msys*)
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

timeout=30

# directory for the temporary files, created once (process creation is slow on Windows)
temp_path="$(mktemp -d -t 'rawcooked_testsuite.XXXXXX')"
trap 'rm -fr "${temp_path}"' EXIT

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
        if [ -z "${files_path_checked}" ] ; then # checked once
            local output="$(git rev-parse --show-cdup)"
            if [ "${?}" -ne "0" ] || [ -n "${output}" ] ; then
                echo "NOK: internal, ${files_path} is not a git repository" >&${fd}
                status=1
                return ${status}
            fi
            files_path_checked=1
        fi
        git clean -fdx >/dev/null 2>&1
    popd >/dev/null 2>&1
}

# shell features instead of external commands in the functions called for each file, process creation is slow on Windows

contains() {
    [[ "${2}" == *"${1}"* ]]
}

# content of the file ${2} in the variable named ${1}, as $(<file), without the CR of CR LF line endings on Windows
read_file() {
    local content=
    IFS= read -r -d '' content <"${2}"
    if test -n "${WINDOWS}" || test -n "${WSL}" ; then
        content="${content//$'\r'/}"
    fi
    while [[ "${content}" == *$'\n' ]] ; do
        content="${content%$'\n'}"
    done
    printf -v "${1}" '%s' "${content}"
}

# lines of the text ${3} containing ${2} in the variable named ${1}, as $(echo "${3}" | grep -F "${2}")
grep_lines() {
    local line lines= newline=$'\n'
    while IFS= read -r line ; do
        if [[ "${line}" == *"${2}"* ]] ; then
            lines="${lines}${lines:+${newline}}${line}"
        fi
    done <<< "${3}"
    printf -v "${1}" '%s' "${lines}"
}

# checksums of the files in the array checksums, one process for all files, returns 1 if a file can not be read
compute_checksums() {
    local checksum dummy
    checksums=()
    ${md5cmd} "${@}" >"${temp_path}/md5" 2>/dev/null
    local local_status="${?}"
    while read -r checksum dummy ; do
        checksums[${#checksums[@]}]="${checksum}"
    done <"${temp_path}/md5"
    return ${local_status}
}

# no access rights for the files and directories (and their content), as chmod -R 0000
# on Windows, chmod sets only the read-only attribute, so reading and writing are denied to everyone with an ACL
# (Git Bash tools may still access the files with the backup privilege of an administrator, rawcooked does not)
# (icacls /T only for directories, with a file name it is a pattern searched in all the subdirectories)
deny_access() {
    local path recursive
    for path in "${@}" ; do
        if test -n "${WINDOWS}" ; then
            recursive=
            [ -d "${path}" ] && recursive=/T
            MSYS_NO_PATHCONV=1 icacls "${path}" /deny '*S-1-1-0:(RD,WD,AD)' ${recursive} /Q >/dev/null || return 1
        else
            chmod -R 0000 "${path}" || return 1
        fi
    done
}

# access rights after deny_access, as chmod -R 0644
allow_access() {
    local path recursive
    for path in "${@}" ; do
        if test -n "${WINDOWS}" ; then
            recursive=
            [ -d "${path}" ] && recursive=/T
            MSYS_NO_PATHCONV=1 icacls "${path}" /remove:d '*S-1-1-0' ${recursive} /Q >/dev/null || return 1
        fi
        chmod -R 0644 "${path}" || return 1
    done
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

check_files() {
    compute_checksums "${1}" "${2}"
    check_checksums "${1}" "${checksums[0]}" "${checksums[1]}" "${3}"
}

check_checksums() {
    local original="${1}"
    local md5_a="${2}"
    local md5_b="${3}"
    local local_status=0

     if [ -n "${md5_a}" ] && [ -n "${md5_b}" ] && [ "${md5_a}" == "${md5_b}" ] ; then
        if [ "${4}" != "-n" ] ; then
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
    local files count=0 i
    local -a originals decodeds checksums_a

    files=$(find "${directory1}" -type f -print)

    for f in ${files} ; do
        count=$((count + 1))
        if [ ! -e  "${directory2}/${f}" ] ; then
            echo "NOK: ${test}/${f} is missing" >&${fd}
            local_status=1
            status=1
            continue
        fi
        originals[${#originals[@]}]="${f}"
        decodeds[${#decodeds[@]}]="${directory2}/${f}"
    done

    # checksums with one process per directory, or file by file if a file can not be read
    if [ "${#originals[@]}" -gt "0" ] ; then
        if compute_checksums "${originals[@]}" && checksums_a=("${checksums[@]}") && compute_checksums "${decodeds[@]}" ; then
            for ((i = 0; i < ${#originals[@]}; i++)) ; do
                if ! check_checksums "${originals[i]}" "${checksums_a[i]}" "${checksums[i]}" "${3}" ; then
                    local_status=1
                fi
            done
        else
            for ((i = 0; i < ${#originals[@]}; i++)) ; do
                if ! check_files "${originals[i]}" "${decodeds[i]}" "${3}" ; then
                    local_status=1
                fi
            done
        fi
    fi

    if [ "$(find ${directory2} -type f | wc -l)" -gt "${count}" ] ; then
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

    local valgrind=""
    if command -v valgrind && test -z "${WSL}" && test -n "${VALGRIND}" ; then
        valgrind="valgrind --quiet --track-origins=yes --log-file=${temp_path}/valgrind"
        : >"${temp_path}/valgrind"
    fi >/dev/null 2>&1

    if test -n "${WINDOWS}" ; then
        # Windows: no pkill for the watcher, so timeout
        timeout ${timeout} rawcooked -n -threads 1 $@ >"${temp_path}/stdout" 2>"${temp_path}/stderr"
        cmd_status="${?}"
        if [ "${cmd_status}" -eq "124" ] ; then
            fatal "command timeout: rawcooked $@"
        fi
    elif test -z "${WSL}" ; then
        ${valgrind} rawcooked -n -threads 1 $@ >"${temp_path}/stdout" 2>"${temp_path}/stderr" & kill -STOP ${!}; local pid=${!}
        sleep ${timeout} && (kill -HUP ${pid} ; fatal "command timeout: rawcooked $@") & local watcher=${!}
        kill -CONT ${pid} ; wait ${pid}
        cmd_status="${?}"
        # stop the watcher: its sleep, and the watcher itself in case its sleep is not started yet (else it reports a timeout later, maybe during another test)
        pkill -P ${watcher}
        kill ${watcher} 2>/dev/null
        wait ${watcher} 2>/dev/null
    else
        rawcooked ${@////\\} >"${temp_path}/stdout" 2>"${temp_path}/stderr"
        cmd_status="${?}"
    fi
    read_file cmd_stdout "${temp_path}/stdout"
    read_file cmd_stderr "${temp_path}/stderr"

    # check crash (illegal instruction, abort, bus error, floating point exception, segmentation fault), never an expected failure
    case "${cmd_status}" in
        132|134|135|136|138|139)
            echo "NOK: ${test}/${file}, rawcooked crashed (exit code ${cmd_status}): rawcooked $@" >&${fd}
            status=1
            ;;
    esac

    # check valgrind
    if [ -n "${valgrind}" ] && [ -s "${temp_path}/valgrind" ] ; then
        cat "${temp_path}/valgrind" >&${fd}
        status=1
    fi

    return ${cmd_status}
}

# ffmpeg_stderr is set only if ffmpeg fails
run_ffmpeg() {
    unset ffmpeg_status
    unset ffmpeg_stderr

    local cmdline
    grep_lines cmdline ffmpeg "${*}"
    if [ -n "${cmdline}" ] ; then
        eval "${cmdline} </dev/null >/dev/null 2>\"${temp_path}/ffmpeg\""
        ffmpeg_status="${?}"
        if [ "${ffmpeg_status}" -ne "0" ] ; then
            ffmpeg_stderr="$(tr '\r' '\n' < "${temp_path}/ffmpeg" | grep -v -E '^(ffmpeg version |  built with |  configuration: |  lib[a-z]+ +[0-9]|frame=|size=| *$)' | tail -n 5)" # Last lines without the banner and the progress lines
        fi
        return ${ffmpeg_status}
    fi
}
