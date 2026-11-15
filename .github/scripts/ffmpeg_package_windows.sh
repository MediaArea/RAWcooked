#!/usr/bin/env bash
# Check of the FFmpeg packages of MediaArea for Windows (ZIP files with a bin directory), in the directories supported by rawcooked
# Usage: ffmpeg_package_windows.sh <rawcooked.exe> <URL of the directory of the packages> <package file>...

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
    cp "${RAWcooked}" "${Directory}/rawcooked.exe"
}

for File in "${@}" ; do
    curl -fsSL -o "${Work}/${File}" "${Base}/${File}" || { echo "FAIL: ${File}: not downloaded" ; Status=1 ; continue ; }
    Package="${Work}/${File%.zip}"
    7z x -y -o"${Package}" "${Work}/${File}" > /dev/null || { echo "FAIL: ${File}: not extracted" ; Status=1 ; continue ; }
    case "${File}" in
        *_Shared_*) Libraries=yes ;;
        *) Libraries=no ;;
    esac

    # Package extracted in the ffmpeg directory next to rawcooked (ffmpeg/bin)
    new_directory
    7z x -y -o"${Directory}/ffmpeg" "${Work}/${File}" > /dev/null
    bash "${Check}" "${File}, ffmpeg/bin" "${Directory}/rawcooked.exe" "${Directory}/ffmpeg/bin/ffmpeg.exe" ${Libraries} || Status=1

    # Binaries next to rawcooked
    new_directory
    cp "${Package}"/bin/* "${Directory}/"
    bash "${Check}" "${File}, ./" "${Directory}/rawcooked.exe" "${Directory}/ffmpeg.exe" ${Libraries} || Status=1

    # Binaries in the ffmpeg directory next to rawcooked
    new_directory
    mkdir "${Directory}/ffmpeg"
    cp "${Package}"/bin/* "${Directory}/ffmpeg/"
    bash "${Check}" "${File}, ffmpeg" "${Directory}/rawcooked.exe" "${Directory}/ffmpeg/ffmpeg.exe" ${Libraries} || Status=1

    # Binaries in PATH (ffmpeg is found by the system)
    new_directory
    PATH="${Package}/bin:${PATH}" bash "${Check}" "${File}, PATH" "${Directory}/rawcooked.exe" ffmpeg ${Libraries} || Status=1

    # Binaries elsewhere, provided with -b
    new_directory
    Options=(-b "\"$(cygpath -w "${Package}/bin/ffmpeg.exe")\"")
    if [ "${Libraries}" = "yes" ] ; then
        Library=$(ls "${Package}"/bin/avcodec-*.dll | head -n 1)
        Options+=(-b "$(cygpath -w "${Library}")")
    fi
    bash "${Check}" "${File}, -b" "${Directory}/rawcooked.exe" "${Package}/bin/ffmpeg.exe" ${Libraries} "${Options[@]}" || Status=1
done

rm -rf "${Work}"
exit ${Status}
