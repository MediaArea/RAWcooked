#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="slice"

valid_slices="4 6 9 12 15 16 20 24 25 28 30 35 36 40 42 45 48 49 54 56 60 63 64 66 70 72 77 80 81 84 88 90 91 96 99 100 104 108 110 112 117 120 121 126 130 132 135 140 143 144 150 153 154 156 160 165 168 169 170 176 180 182 187 190 192 195 196 198 204 208 209 210 216 220 221 224 225 228 231 234 238 240 247 252 255 256 260 264 266 270 272 273 276 280 285 286 288 289 294 299 300 304 306 308 312 315 320 322 323 324 325 330 336 340 342 345 350 352 357 360 361 364 368 374 375 378 380 384 390 391 396 399 400 405 408 414 416 418 420 425 432 435 437 440 441 442 448 450 456 459 460 462 464 468 475 476 480 483 484 486 493 494 496 500 504 506 510 513 520 522 525 527 528 529 532 540 544 546 550 551 552 558 560 561 567 570 572 575 576 580 588 589 594 598 600 608 609 612 616 620 621 624 625 627 630 638 640 644 646 648 650 651 660 665 667 672 675 676 680 682 684 690 693 696 700 702 703 704 713 714 720 725 726 728 729 735 736 740 744 748 750 754 756 759 760 768 770 775 777 780 782 783 784 792 798 800 805 806 810 812 814 816 819 825 828 832 836 837 840 841 850 851 858 861 864 868 870 874 875 880 884 888 891 896 897 899 900 902 910 912 918 920 924 925 928 930 936 943 945 946 950 952 957 960 961 962 966 972 975 980 984 986 988 989 990 992 999 1000 1008 1012 1014 1015 1020 1023"

is_number() {
    re='^[0-9]+$'
    if [[ "${1}" =~ ${re} ]] ; then
        return 0
    fi

    return 1
}

is_valid() {
    if [[ " ${valid_slices} " =~ " ${1} " ]] ; then
        return 0
    fi

    return 1
}

current=""

while read line ; do
    x="$(echo "${line}" | cut -d':' -f1)"
    y="$(echo "${line}" | cut -d':' -f2)"
    f="$(echo "${line}" | cut -d':' -f3)"
    c="$(echo "${line}" | cut -d':' -f4)"
    file="${x}x${y}@${f}.dpx"

    if [ "${current}" != "${x}" ] ; then
        echo "Slices counts for x = ${x}x..." >&${fd}
    fi

    current="${x}"

    if [ ! -n "${f}" ] || ! is_number "${x}" || ! is_number "${y}" || ! is_number "${c}" ; then
        echo "NOK: ${test}/${file}, invalid data" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}"
        # generate test file
        run_ffmpeg ffmpeg -f lavfi -i color=c=black:s=${x}x${y} -pix_fmt ${f} -vframes 1 ${file}

        if [ ! -e "${file}" ] ; then
            echo "NOK: ${test}/${file}, ffmpeg cannot generate dpx" >&${fd}
            status=1
            clean
            popd
            continue
        fi

        # generate encoding command
        run_rawcooked --file -d "${file}"
        check_success "file rejected at input" "file accepted at input" || continue

        count=$(echo "${cmd_stdout}" | sed -En 's/.* -slices ([0-9]+) .*/\1/p')

        if ! is_number ${count} ; then
            echo "NOK: ${test}/${file}, unable to get slices count" >&${fd}
            status=1
            clean
            popd
            continue
        fi

        # check result
        if [ "${count}" -lt "${c}" ] || [ "${count}" -gt "$((${c}+(${c}/2)))" ] ; then
            echo "NOK: ${test}/${file}, invalid slices count: ${count} out of range ${c}...$((${c}+(${c}/2)))" >&${fd}
            status=1
            clean
            popd
            continue
        fi

        if ! is_valid "${count}" ; then
            echo "NOK: ${test}/${file}, slices count: ${count} not supported by ffmpeg"
            status=1
        fi

        clean
    popd  >/dev/null 2>&1
done < "${script_path}/slices.txt"

exit ${status}
