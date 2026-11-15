#!/usr/bin/env bash
# Check of an ffmpeg-ma package of MediaArea on Linux, in a container of its distribution: installed with the package manager (intended usage)
# Usage: ffmpeg_package_linux.sh <package manager> <package file> <rawcooked> <key file> <key fingerprint>
# Key file: public key of the MediaArea repositories, the signature of RPM packages and of Arch packages (<package file>.sig) is checked with it

Manager=${1}
Package=${2}
RAWcooked=${3}
Key=${4}
Fingerprint=${5}
Check="$(dirname "${0}")/ffmpeg_package_check.sh"
Name=$(basename "${Package}")

# Installation, with the dependencies from the repositories of the distribution
case "${Manager}" in
    dnf|zypper|urpmi) rpm --import "${Key}" ;;
    pacman) pacman-key --init && pacman-key --add "${Key}" && pacman-key --lsign-key "${Fingerprint}" && pacman-key --verify "./${Package}.sig" "./${Package}" ;;
esac > install.log 2>&1 || { cat install.log ; echo "FAIL: ${Name}: signature not checked" ; exit 1 ; }
case "${Manager}" in
    apt) apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "./${Package}" ;;
    dnf) dnf install -y -q "./${Package}" ;;
    zypper) zypper --non-interactive --quiet install "./${Package}" ;;
    urpmi) urpmi.update -a -q && urpmi --auto "./${Package}" ;;
    pacman) pacman -Syu --noconfirm --noprogressbar && pacman -U --noconfirm --noprogressbar "./${Package}" ;;
    *) echo "Unknown package manager ${Manager}" ; exit 1 ;;
esac > install.log 2>&1 || { tail -n 30 install.log ; echo "FAIL: ${Name}: installation failed" ; exit 1 ; }

# rawcooked in a directory with a space, without FFmpeg next to it
Directory="$(mktemp -d)/raw cooked"
mkdir "${Directory}"
cp "${RAWcooked}" "${Directory}/rawcooked"
chmod +x "${Directory}/rawcooked"

# ffmpeg-ma binary in PATH and libraries in the library paths of the system
Binary=$(command -v ffmpeg-ma) || { echo "FAIL: ${Name}: no ffmpeg-ma in PATH" ; exit 1 ; }
for Library in /usr/lib*/libavcodec-ma.so.* /usr/lib/*/libavcodec-ma.so.* ; do
    [ -f "${Library}" ] && break
done
[ -f "${Library}" ] || { echo "FAIL: ${Name}: no libavcodec-ma library" ; exit 1 ; }
Status=0
bash "${Check}" "${Name}, installed" "${Directory}/rawcooked" "${Binary}" yes || Status=1
bash "${Check}" "${Name}, installed, -b" "${Directory}/rawcooked" "${Binary}" yes -b "${Binary}" -b "${Library}" || Status=1
exit ${Status}
