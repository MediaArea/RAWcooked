#!/usr/bin/env bash
# Check of an installed FFmpeg package with rawcooked: ffmpeg binary used for encoding, libraries used by the ffmpeg decoder
# Usage: ffmpeg_package_check.sh <case name> <rawcooked> <expected ffmpeg> <libraries: yes|no> [rawcooked options, e.g. -b]
# <expected ffmpeg> is the path of the ffmpeg binary rawcooked must use, or a name searched in PATH by the system (e.g. ffmpeg)
# Bash 3.2 compatible (macOS)

Name=${1}
RAWcooked=${2}
Expected=${3}
Libraries=${4}
shift 4

case "${OSTYPE}" in
    msys*|cygwin*) IsWindows=1 ;;
    *) IsWindows= ;;
esac

Work=$(mktemp -d)
fail() {
    echo "FAIL: ${Name}: ${*}"
    if [ -f "${Work}/log" ] ; then
        sed 's/^/      /' "${Work}/log" | tail -n 20
    fi
    exit 1
}

# Path without symbolic links in the directories, in the same form on all platforms (also from a Windows path), a name without directory is unchanged
normalize() {
    local Path=${1}
    if [ -n "${IsWindows}" ] ; then
        case "${Path}" in
            *\\*|*/*) Path=$(cygpath -u "${Path}") ;;
        esac
    fi
    case "${Path}" in
        */*) Path="$(cd "$(dirname "${Path}")" 2>/dev/null && pwd -P)/$(basename "${Path}")" ;;
    esac
    if [ -n "${IsWindows}" ] ; then
        Path=$(echo "${Path}" | tr '[:upper:]' '[:lower:]')
    fi
    echo "${Path}"
}

# Test files, created by the binary of the package
Generator=${Expected}
case "${Generator}" in
    */*) ;;
    *) Generator=$(command -v "${Generator}") || fail "${Expected} is not in PATH" ;;
esac
mkdir "${Work}/in"
"${Generator}" -hide_banner -loglevel error -f lavfi -i testsrc2=size=96x64:rate=24 -frames:v 3 -pix_fmt rgb48be -y "${Work}/in/%06d.dpx" > "${Work}/log" 2>&1 || fail "test files not created by ${Generator}"
Version=$("${Generator}" -hide_banner -version 2>/dev/null | sed -n '1s/^ffmpeg version \([^ ]*\).*/\1/p')

# Binary used by rawcooked, first item of the command
"${RAWcooked}" -y -d "${@}" "${Work}/in" -o "${Work}/d.mkv" > "${Work}/command" 2> "${Work}/log" || fail "command not displayed (exit code ${?})"
Command=$(sed -n 1p "${Work}/command")
case "${Command}" in
    \"*) Binary=${Command#\"} ; Binary=${Binary%%\"*} ;;
    *) Binary=${Command%% *} ;;
esac
if [ "$(normalize "${Binary}")" != "$(normalize "${Expected}")" ] ; then
    echo "${Command}" > "${Work}/log"
    fail "ffmpeg binary is ${Binary}, expected ${Expected}"
fi

if [ "${Libraries}" = "yes" ] ; then
    # Encoding, then check with the ffmpeg decoder
    "${RAWcooked}" -y --all --decoder ffmpeg "${@}" "${Work}/in" -o "${Work}/out.mkv" > "${Work}/log" 2>&1 || fail "encoding failed (exit code ${?})"
    grep -q "Reversibility was checked, no issue detected" "${Work}/log" || fail "no check after encoding"

    # Decoding with the ffmpeg decoder
    "${RAWcooked}" -y --decoder ffmpeg "${@}" "${Work}/out.mkv" -o "${Work}/dec" > "${Work}/log" 2>&1 || fail "decoding failed (exit code ${?})"
    for File in "${Work}"/in/*.dpx ; do
        Decoded="${Work}/dec/in/$(basename "${File}")"
        [ -f "${Decoded}" ] || Decoded="${Work}/dec/$(basename "${File}")"
        [ -f "${Decoded}" ] || fail "$(basename "${File}") not decoded"
        [ "$(cksum < "${File}")" = "$(cksum < "${Decoded}")" ] || fail "$(basename "${File}") decoded with a different content"
    done
else
    # Encoding with our decoder, the ffmpeg decoder is rejected before the encoding
    "${RAWcooked}" -y --all "${@}" "${Work}/in" -o "${Work}/out.mkv" > "${Work}/log" 2>&1 || fail "encoding failed (exit code ${?})"
    grep -q "Reversibility was checked, no issue detected" "${Work}/log" || fail "no check after encoding"
    if "${RAWcooked}" -y --all --decoder ffmpeg "${@}" "${Work}/in" -o "${Work}/out2.mkv" > "${Work}/log" 2>&1 ; then
        fail "ffmpeg decoder accepted without FFmpeg libraries"
    fi
    grep -q "decoder value 'ffmpeg' is not available" "${Work}/log" || fail "no error about the FFmpeg libraries"
    [ ! -e "${Work}/out2.mkv" ] || fail "encoding done before the error about the FFmpeg libraries"
fi

echo "OK: ${Name} (ffmpeg ${Version}: ${Binary}, libraries: ${Libraries})"
rm -rf "${Work}"
