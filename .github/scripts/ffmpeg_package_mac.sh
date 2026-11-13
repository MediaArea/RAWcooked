#!/usr/bin/env bash
# Check of the FFmpeg packages of MediaArea for macOS, as intended: ZIP files (bin and lib directories) in the ffmpeg directory next to rawcooked,
# DMG file installed in the system (ffmpeg-ma in /usr/local), installed last as ffmpeg-ma in PATH is used before ffmpeg next to rawcooked
# Usage: ffmpeg_package_mac.sh <rawcooked> <URL of the directory of the packages> <package file>...
# Bash 3.2 compatible

RAWcooked=${1}
Base=${2}
shift 2
Check="$(cd "$(dirname "${0}")" && pwd)/ffmpeg_package_check.sh"
Work=$(mktemp -d)
Status=0
Count=0

# New directory with a space, with rawcooked and without FFmpeg
new_directory() {
    Count=$((Count + 1))
    Directory="${Work}/raw cooked ${Count}"
    mkdir -p "${Directory}"
    cp "${RAWcooked}" "${Directory}/rawcooked"
}

for File in $(printf '%s\n' "${@}" | grep -v '\.dmg$') $(printf '%s\n' "${@}" | grep '\.dmg$') ; do
    curl -fsSL -o "${Work}/${File}" "${Base}/${File}" || { echo "FAIL: ${File}: not downloaded" ; Status=1 ; continue ; }
    case "${File}" in
        *.zip)
            case "${File}" in
                *_Shared_*) Libraries=yes ;;
                *) Libraries=no ;;
            esac

            # Package extracted in the ffmpeg directory next to rawcooked
            new_directory
            unzip -q "${Work}/${File}" -d "${Directory}/ffmpeg" || { echo "FAIL: ${File}: not extracted" ; Status=1 ; continue ; }
            bash "${Check}" "${File}, ffmpeg" "${Directory}/rawcooked" "${Directory}/ffmpeg/bin/ffmpeg" ${Libraries} || Status=1

            # Package extracted elsewhere, provided with -b
            new_directory
            Package="${Work}/${File%.zip}"
            unzip -q "${Work}/${File}" -d "${Package}"
            Options="-b ${Package}/bin/ffmpeg"
            if [ "${Libraries}" = "yes" ] ; then
                Options="${Options} -b $(ls "${Package}"/lib/libavcodec.[0-9]*.dylib | head -n 1)"
            fi
            bash "${Check}" "${File}, -b" "${Directory}/rawcooked" "${Package}/bin/ffmpeg" ${Libraries} ${Options} || Status=1
            ;;
        *.dmg)
            # Installation of the package in the DMG file
            Mount="${Work}/mount"
            mkdir "${Mount}"
            hdiutil attach -quiet -nobrowse -readonly -mountpoint "${Mount}" "${Work}/${File}" || { echo "FAIL: ${File}: not mounted" ; Status=1 ; continue ; }
            Installer=$(ls "${Mount}"/*.pkg | head -n 1)
            sudo installer -pkg "${Installer}" -target / > "${Work}/install.log" 2>&1 || { cat "${Work}/install.log" ; echo "FAIL: ${File}: not installed" ; Status=1 ; continue ; }
            hdiutil detach -quiet "${Mount}"

            # ffmpeg-ma in PATH, libraries in /usr/local/lib
            new_directory
            Binary=$(command -v ffmpeg-ma) || { echo "FAIL: ${File}: no ffmpeg-ma in PATH" ; Status=1 ; continue ; }
            bash "${Check}" "${File}, installed" "${Directory}/rawcooked" "${Binary}" yes || Status=1
            Library=$(ls /usr/local/lib/libavcodec-ma.[0-9]*.dylib | head -n 1)
            bash "${Check}" "${File}, installed, -b" "${Directory}/rawcooked" "${Binary}" yes -b "${Binary}" -b "${Library}" || Status=1
            ;;
    esac
done

rm -rf "${Work}"
exit ${Status}
