#!/usr/bin/env bash
# FFmpeg packages of MediaArea, as GitHub Actions outputs:
# linux: matrix entries (package file, runner, container image, package manager), windows and mac: package files
# A package with an unknown name is an error, so a new package is not silently ignored
# Usage: ffmpeg_packages.sh [URL of the directory of the packages]

set -e
Base=${1:-https://mediaarea.net/download/snapshots/binary/ffmpeg/latest}
Files=$(curl -fsSL "${Base}/" | grep -o 'href="[^"?/]*"' | sed 's/^href="//;s/"$//' | sort -u)
[ -n "${Files}" ]

Linux=
Windows=
Mac=
for File in ${Files} ; do
    case "${File}" in
        FFmpeg_Dev_*) continue ;; # Headers of the libraries (for building rawcooked)
        FFmpeg_Bin_*_Windows_*.zip) Windows="${Windows} ${File}" ; continue ;;
        FFmpeg_Bin_*_Mac_*.zip|ffmpeg-ma_*_Mac.dmg) Mac="${Mac} ${File}" ; continue ;;
        *_amd64.*.deb) Arch=x64 Distro=$(echo "${File}" | sed 's/.*_amd64\.\(.*\)\.deb$/\1/') ;;
        *_arm64.*.deb) Arch=arm64 Distro=$(echo "${File}" | sed 's/.*_arm64\.\(.*\)\.deb$/\1/') ;;
        *.x86_64.*.rpm) Arch=x64 Distro=$(echo "${File}" | sed 's/.*\.x86_64\.\(.*\)\.rpm$/\1/') ;;
        *.aarch64.*.rpm) Arch=arm64 Distro=$(echo "${File}" | sed 's/.*\.aarch64\.\(.*\)\.rpm$/\1/') ;;
        *-x86_64.*.pkg.tar.zst) Arch=x64 Distro=$(echo "${File}" | sed 's/.*-x86_64\.\(.*\)\.pkg\.tar\.zst$/\1/') ;;
        *) echo "Unknown package ${File}" >&2 ; exit 1 ;;
    esac
    case "${Distro}" in
        Ubuntu_*) Image=ubuntu:${Distro#Ubuntu_} Manager=apt ;;
        Debian_*) Image=debian:${Distro#Debian_} Manager=apt ;;
        Raspbian_*) Image=debian:${Distro#Raspbian_} Manager=apt ;; # Raspberry Pi OS (64-bit) is based on Debian arm64
        Fedora_*) Image=fedora:${Distro#Fedora_} Manager=dnf ;;
        AlmaLinux_*) Image=almalinux:${Distro#AlmaLinux_} Manager=dnf ;;
        Mageia_*) Image=mageia:${Distro#Mageia_} Manager=urpmi ;;
        openSUSE_Leap_*) Image=opensuse/leap:${Distro#openSUSE_Leap_} Manager=zypper ;;
        openSUSE_Tumbleweed) Image=opensuse/tumbleweed Manager=zypper ;;
        SLE_*) Image=registry.suse.com/bci/bci-base:${Distro#SLE_}.0 Manager=zypper ;;
        Arch_Extra) Image=archlinux:latest Manager=pacman ;;
        *) echo "Unknown distribution ${Distro} of package ${File}" >&2 ; exit 1 ;;
    esac
    case "${Arch}" in
        x64) Runner=ubuntu-latest ;;
        arm64) Runner=ubuntu-24.04-arm ;;
    esac
    Linux="${Linux}${Linux:+,}{\"file\":\"${File}\",\"arch\":\"${Arch}\",\"runner\":\"${Runner}\",\"image\":\"${Image}\",\"manager\":\"${Manager}\"}"
done

echo "base=${Base}"
echo "linux=[${Linux}]"
echo "windows=${Windows# }"
echo "mac=${Mac# }"
