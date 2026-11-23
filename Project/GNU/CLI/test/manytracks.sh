#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

# More than 127 tracks in one Matroska file: the blocks of the tracks 128 and more have a track number on 2 bytes
test="manytracks"
file="d001/img.tif"
count=130

if ! command -v ffmpeg >/dev/null 2>&1 ; then
    echo "SKIP: ${test}, ffmpeg not found" >&${fd}
    exit 0
fi

work_path="${temp_path}/${test}"
mkdir -p "${work_path}"
pushd "${work_path}" >/dev/null 2>&1
    files=()
    for i in $(seq -w 1 ${count}) ; do
        mkdir "d${i}"
        if ! ffmpeg -nostdin -v error -f lavfi -i "testsrc=size=16x8:rate=1,hue=h=${i#0}" -frames:v 1 -pix_fmt rgb24 -compression_algo raw "d${i}/img.tif" ; then
            fatal "${test}" "test file generation failed"
        fi
        files[${#files[@]}]="d${i}/img.tif"
    done

    run_rawcooked --file -d ${files[@]}
    if ! check_success "files rejected at input" "files accepted at input" ; then
        popd >/dev/null 2>&1
        exit ${status}
    fi

    run_ffmpeg "${cmd_stdout}"
    rm -f "${file}.rawcooked_reversibility_data"
    if [ ! -e "${file}.mkv" ] ; then
        echo "NOK: ${test}/${file}, mkv not generated, ${ffmpeg_stderr}" >&${fd}
        status=1
        popd >/dev/null 2>&1
        exit ${status}
    fi

    run_rawcooked --check "${file}.mkv"
    check_success "mkv checking failed" "mkv checked"

    run_rawcooked "${file}.mkv"
    if check_success "mkv decoding failed" "mkv decoded" ; then
        local_status=0
        for f in ${files[@]} ; do
            if ! cmp -s "${f}" "${file}.mkv.RAWcooked/${f}" ; then
                echo "NOK: ${test}/${f}, file is missing or different after decoding" >&${fd}
                status=1
                local_status=1
            fi
        done
        if [ "${local_status}" -eq "0" ] ; then
            echo "OK: ${test}, ${count} tracks decoded" >&${fd}
        fi
    fi
    rm -fr "${file}.mkv.RAWcooked"

    # Blocks are mapped to the tracks by the TrackNumber of the track entries, not by the position of the track entries:
    # the track numbers 1 to 127 are reversed (TrackNumber elements and block headers), the decoded files must not change
    if python3 -c "" >/dev/null 2>&1 ; then # Not only found (e.g. Microsoft Store alias on Windows) but working
        cat >"${temp_path}/tracknumbers.py" <<'EOF'
import sys
MASTERS = {0x18538067, 0x1654AE6B, 0xAE, 0x1F43B675, 0xA0}
def vint(b, p, keep_marker):
    first = b[p]; n = 1
    while n <= 8 and not first & (0x80 >> (n - 1)): n += 1
    v = first if keep_marker else first & (0xFF >> n)
    for i in range(1, n): v = (v << 8) | b[p + i]
    return v, n
def walk(b, p, end):
    while p < end:
        eid, n1 = vint(b, p, True); size, n2 = vint(b, p + n1, False)
        d = p + n1 + n2
        if size == (1 << (7 * n2)) - 1: size = end - d
        if eid in MASTERS: walk(b, d, d + size)
        elif eid == 0xD7 and size == 1 and 1 <= b[d] <= 127: b[d] = 128 - b[d]
        elif eid in (0xA3, 0xA1) and b[d] & 0x80: b[d] = 0x80 | (128 - (b[d] & 0x7F))
        p = d + size
b = bytearray(open(sys.argv[1], 'rb').read())
walk(b, 0, len(b))
open(sys.argv[2], 'wb').write(b)
EOF
        if python3 "${temp_path}/tracknumbers.py" "${file}.mkv" "${file}.reordered.mkv" ; then
            run_rawcooked "${file}.reordered.mkv"
            if check_success "mkv with reordered track numbers decoding failed" "mkv with reordered track numbers decoded" ; then
                local_status=0
                for f in ${files[@]} ; do
                    if ! cmp -s "${f}" "${file}.reordered.mkv.RAWcooked/${f}" ; then
                        echo "NOK: ${test}/${f}, file is missing or different after decoding the mkv with reordered track numbers" >&${fd}
                        status=1
                        local_status=1
                    fi
                done
                if [ "${local_status}" -eq "0" ] ; then
                    echo "OK: ${test}, reordered track numbers decoded" >&${fd}
                fi
            fi
        else
            echo "NOK: ${test}, track number rewriting failed" >&${fd}
            status=1
        fi
    else
        echo "SKIP: ${test}, reordered track numbers, python3 not found or not working" >&${fd}
    fi
popd >/dev/null 2>&1

exit ${status}
