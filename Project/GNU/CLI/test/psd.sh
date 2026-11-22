#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

# PSD files with a raw merged image (RGB, gray and duotone, 8- and 16-bit), with image resources and a layer section,
# generated here (no PSD in the testing files)
if ! command -v python3 >/dev/null 2>&1 ; then
    echo "SKIP: psd, python3 not found" >&${fd}
    exit 77
fi

work="${temp_path}/psd"
mkdir "${work}" || fatal "internal" "mkdir failed"
python3 - "${work}" <<'EOF' || fatal "internal" "PSD generation failed"
import os, random, struct, sys

def pascal(name, align):
    data = bytes([len(name)]) + name
    while len(data) % align:
        data += b'\0'
    return data

def plane(width, height, depth, index, rnd):
    out = bytearray()
    for y in range(height):
        for x in range(width):
            v = (x * 7 + y * 13 + index * 61) + rnd.randrange(4)
            if depth == 8:
                out.append(v & 0xFF)
            else:
                out += struct.pack('>H', (v * 257 + rnd.randrange(256)) & 0xFFFF)
    return bytes(out)

def make_psd(mode, depth, width, height, rle=False, extra_channel=False, seed=0):
    rnd = random.Random(seed)
    color_mode, channels = {'gray': (1, 1), 'rgb': (3, 3), 'duotone': (8, 1)}[mode]
    total = channels + (1 if extra_channel else 0)
    out = bytearray(b'8BPS' + struct.pack('>H', 1) + b'\0' * 6)
    out += struct.pack('>HIIHH', total, height, width, depth, color_mode)
    color_data = b'\0\1' + bytes(range(64)) if mode == 'duotone' else b''
    out += struct.pack('>I', len(color_data)) + color_data
    res = b'8BIM' + struct.pack('>H', 0x03ED) + pascal(b'', 2) + struct.pack('>I', 16) + struct.pack('>IHHIHH', 72 << 16, 1, 1, 72 << 16, 1, 1)
    res += b'8BIM' + struct.pack('>H', 0x0428) + pascal(b'n', 2) + struct.pack('>I', 19) + b'RAWcooked PSD sampl\0'
    out += struct.pack('>I', len(res)) + res
    lw, lh = max(1, width // 2), max(1, height // 2)
    ids = list(range(channels)) + [-1]
    chan_data = [struct.pack('>H', 0) + plane(lw, lh, depth, c + 5, rnd) for c in ids]
    rec = struct.pack('>iiiiH', 1, 1, 1 + lh, 1 + lw, len(ids))
    for c, d in zip(ids, chan_data):
        rec += struct.pack('>hI', c, len(d))
    extra = struct.pack('>II', 0, 0) + pascal(b'Layer 1', 4)
    rec += b'8BIMnorm' + bytes([255, 0, 0, 0]) + struct.pack('>I', len(extra)) + extra
    info = struct.pack('>h', 1) + rec + b''.join(chan_data)
    while len(info) % 4:
        info += b'\0'
    section = struct.pack('>I', len(info)) + info + struct.pack('>I', 0)
    out += struct.pack('>I', len(section)) + section
    planes = [plane(width, height, depth, c, rnd) for c in range(total)]
    if not rle:
        out += struct.pack('>H', 0) + b''.join(planes)
    else:
        rows = [p[y * width:(y + 1) * width] for p in planes for y in range(height)]
        rows = [bytes([len(r) - 1]) + r for r in rows] # PackBits literal runs (width <= 128)
        out += struct.pack('>H', 1) + b''.join(struct.pack('>H', len(r)) for r in rows) + b''.join(rows)
    return bytes(out)

def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)

work = sys.argv[1]
for name, mode, depth, width, height in (('RGB_8', 'rgb', 8, 37, 23), ('RGB_16', 'rgb', 16, 33, 17), ('Y_8', 'gray', 8, 41, 19), ('Y_16', 'gray', 16, 29, 31), ('Duotone_8', 'duotone', 8, 35, 21), ('Duotone_16', 'duotone', 16, 27, 15)):
    write(os.path.join(work, name, 'image.psd'), make_psd(mode, depth, width, height))
for i in range(3):
    write(os.path.join(work, 'Sequence', 'frame_%04d.psd' % (i + 1)), make_psd('rgb', 8, 37, 23, seed=i + 1))
write(os.path.join(work, 'RLE', 'image.psd'), make_psd('rgb', 8, 37, 23, rle=True))
write(os.path.join(work, 'Extra_Channel', 'image.psd'), make_psd('rgb', 8, 37, 23, extra_channel=True))
EOF

pushd "${work}" >/dev/null 2>&1
    # single files: byte-exact roundtrip, --check forced
    file="image.psd"
    for test in RGB_8 RGB_16 Y_8 Y_16 Duotone_8 Duotone_16 ; do
        pushd "${test}" >/dev/null 2>&1
            run_rawcooked -y --conch --encode --file "${file}"
            if check_success "file rejected at input" "file accepted at input" ; then
                if ! contains "--check is set" "${cmd_stderr}" ; then
                    echo "NOK: ${test}/${file}, --check not forced" >&${fd}
                    status=1
                fi

                # check encoding
                run_rawcooked -y --check "${file}.mkv"
                if check_success "mkv check failed" "mkv checked" ; then
                    # check decoding
                    run_rawcooked -y --conch --decode "${file}.mkv"
                    if check_success "mkv decoding failed" "mkv decoded" ; then
                        check_files "${file}" "${file}.mkv.RAWcooked/${file}"
                    fi
                fi
            fi
        popd >/dev/null 2>&1
    done

    # sequence of numbered files
    test="Sequence"
    file="${test}"
    run_rawcooked -y --conch --encode "${test}"
    if check_success "sequence rejected at input" "sequence accepted at input" ; then
        if ! contains "(0001 --> 0003)" "${cmd_stdout}${cmd_stderr}" ; then
            echo "NOK: ${test}, sequence not detected: ${cmd_stdout}${cmd_stderr}" >&${fd}
            status=1
        fi
        run_rawcooked -y --conch --decode "${test}.mkv"
        if check_success "mkv decoding failed" "mkv decoded" ; then
            check_directories "${test}" "${test}.mkv.RAWcooked"
        fi
    fi

    # refused: RLE merged image, extra channel
    file="image.psd"
    for test in RLE Extra_Channel ; do
        pushd "${test}" >/dev/null 2>&1
            run_rawcooked -y --encode --file "${file}"
            if check_failure "file rejected at input" "file accepted at input" ; then
                if contains "PSD" "${cmd_stderr}" && contains "not supported" "${cmd_stderr}" ; then
                    echo "OK: ${test}/${file}, file rejected at input" >&${fd}
                else
                    echo "NOK: ${test}/${file}, file rejected at input without the PSD message: ${cmd_stderr}" >&${fd}
                    status=1
                fi
            fi
            if [ -e "${file}.mkv" ] ; then
                echo "NOK: ${test}/${file}, mkv created" >&${fd}
                status=1
            fi
        popd >/dev/null 2>&1
    done
popd >/dev/null 2>&1

exit ${status}
