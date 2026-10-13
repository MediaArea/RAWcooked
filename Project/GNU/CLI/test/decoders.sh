#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="decoders"

# many small frames (DPX RGB 10-bit) checked with our decoder (reference duration) and each combination of decoders, with the default count of threads:
# FFmpeg libraries are loaded and used, decoding is not slow (e.g. an initialization or a wait for each frame)
# lot of frames, so FFmpeg decoders decode several frames after their initialization (e.g. 0.5 s for the creation of the Vulkan device with a GPU)
frames=3000
file=decoders

# fewer frames for each FFmpeg decoder alone: FFmpeg decoder is slower than our decoder with small frames,
# slices of small frames are decoded by few GPU threads with the vulkan decoder, and a CPU implementation of Vulkan (e.g. lavapipe on CI) is slower
alone_frames=100
alone_file=decoders_alone

# test skipped if FFmpeg libraries are not available, except if an FFmpeg package is next to rawcooked (ffmpeg directory, e.g. CI)
run_rawcooked --decoder ffmpeg --version
if [ "${cmd_status}" -ne "0" ] ; then
    rawcooked_path="$(type -p rawcooked)"
    if [ -d "${rawcooked_path%/*}/ffmpeg" ] ; then
        echo "NOK: ${test}, FFmpeg libraries of ${rawcooked_path%/*}/ffmpeg not loaded: ${cmd_stderr}" >&${fd}
        exit 1
    fi
    echo "SKIP: ${test}, ${cmd_stderr}" >&${fd}
    exit 77
fi

# duration in milliseconds in duration_ms, from the output of time in the file ${1}
TIMEFORMAT=%3R
read_duration() {
    local duration
    read_file duration "${1}"
    duration="${duration##*$'\n'}" # Last line, after possible messages of the shell
    duration="${duration//[.,]/}" # Decimal point of the locale
    duration_ms=$((10#${duration}))
}

# ${2} frames in the directory ${1}, encoded with the hashes of the source files in the reversibility data, compared to the hashes of the decoded files during the check (no file written)
encode() {
    mkdir -p "${1}" || fatal "internal" "mkdir command failed"
    ffmpeg -nostdin -f lavfi -i testsrc2=size=131x97:rate=24 -frames:v ${2} -pix_fmt gbrp10le "${1}/%04d.dpx" >/dev/null 2>&1 || fatal "internal" "ffmpeg command failed"
    run_rawcooked -y -threads 0 --no-check-padding --hash --encode "${1}"
    check_success "file rejected at input" "file accepted at input" || fatal "${test}" "encoding failed"
    if [ ! -e "${1}.mkv" ] ; then
        fatal "${test}" "mkv not generated"
    fi
}

pushd "${files_path}" >/dev/null 2>&1
    encode "${file}" "${frames}"
    encode "${alone_file}" "${alone_frames}"

    internal_us=0 # Duration of a frame with our decoder alone, in microseconds
    vulkan_ms=0 # Duration with the vulkan decoder alone, including its initialization
    for decoder in internal ffmpeg vulkan internal+vulkan internal+ffmpeg ffmpeg+vulkan internal+ffmpeg+vulkan auto ; do
        test="decoders (${decoder})"
        input="${file}"
        count="${frames}"
        if [ "${decoder}" == "ffmpeg" ] || [ "${decoder}" == "vulkan" ] ; then
            input="${alone_file}"
            count="${alone_frames}"
        fi
        export RAWCOOKED_TRACE="${input}.trace.json" # Summary of the stages in stderr, with the count of frames decoded by each FFmpeg decoder (transfer stages)
        { time run_rawcooked -y -threads 0 --decoder ${decoder} --check "${input}.mkv" ; } 2>"${temp_path}/time"
        unset RAWCOOKED_TRACE
        read_duration "${temp_path}/time"

        # maximum duration: less than 3 ms per frame with our decoder alone (less than the timeout), other decoders less than 5 times our decoder alone per frame plus 5 s (one-time costs, slower FFmpeg decoder),
        # plus with the vulkan decoder in parallel with other decoders the duration of the vulkan decoder alone (the end waits for the first frame given to the vulkan decoder during its initialization, long e.g. with lavapipe)
        # except the vulkan decoder alone, its duration with small frames is not representative
        if [ "${decoder}" == "internal" ] ; then
            internal_us=$((duration_ms * 1000 / count))
            max_ms=$((count * 3))
        elif [ "${decoder}" == "vulkan" ] ; then
            vulkan_ms="${duration_ms}"
            max_ms=
        else
            max_ms=$((count * internal_us * 5 / 1000 + 5000))
            if contains "vulkan" "${decoder}" || [ "${decoder}" == "auto" ] ; then
                max_ms=$((max_ms + vulkan_ms))
            fi
        fi

        check_success "mkv check failed" "mkv checked" || continue
        if ! contains "Reversibility was checked, no issue detected." "${cmd_stdout}" ; then
            echo "NOK: ${test}/${input}, reversibility not checked: ${cmd_stdout}" >&${fd}
            status=1
            continue
        fi

        # frames decoded by each FFmpeg decoder (without the first frame for a decoder if it failed, decoded again by another decoder): all frames by the FFmpeg decoders without our decoder (Vulkan device needed for the vulkan decoder),
        # at least the first frame given to each requested FFmpeg decoder (during its initialization if all frames are keyframes, other decoders may decode most other frames before the end of this initialization),
        # none by the FFmpeg decoders not requested, maybe none by the vulkan decoder with auto (Vulkan device not a GPU)
        decoded_ffmpeg=0
        decoded_vulkan=0
        grep_lines transfer_lines " transfer " "${cmd_stderr}"
        while read -r name stage value dummy ; do
            if [ "${stage}" == "transfer" ] && [[ "${value}" =~ ^[0-9]+$ ]] ; then # Not the lines of the transfer threads
                if [ "${name}" == "FFmpeg" ] ; then
                    decoded_ffmpeg="${value}"
                elif [ "${name}" == "Vulkan" ] ; then
                    decoded_vulkan="${value}"
                fi
            fi
        done <<< "${transfer_lines}"
        min_ffmpeg=0
        max_ffmpeg=0
        if contains "ffmpeg" "${decoder}" ; then
            min_ffmpeg=1
            max_ffmpeg="${count}"
        fi
        min_vulkan=0
        max_vulkan=0
        if contains "vulkan" "${decoder}" ; then
            min_vulkan=1
            max_vulkan="${count}"
        elif [ "${decoder}" == "auto" ] ; then
            max_vulkan="${count}"
        fi
        min_decoded=0
        if ! contains "internal" "${decoder}" && [ "${decoder}" != "auto" ] ; then
            min_decoded="${count}"
        fi
        decoded="$((count - decoded_ffmpeg - decoded_vulkan))/${decoded_ffmpeg}/${decoded_vulkan}" # Frames decoded by our decoder, by FFmpeg decoder, by Vulkan decoder
        if [ "${decoded_ffmpeg}" -lt "${min_ffmpeg}" ] || [ "${decoded_ffmpeg}" -gt "${max_ffmpeg}" ] || [ "${decoded_vulkan}" -lt "${min_vulkan}" ] || [ "${decoded_vulkan}" -gt "${max_vulkan}" ] || [ $((decoded_ffmpeg + decoded_vulkan)) -lt "${min_decoded}" ] ; then
            echo "NOK: ${test}/${input}, ${decoded} frames decoded by ours/FFmpeg/Vulkan decoders instead of ${min_ffmpeg}-${max_ffmpeg} by FFmpeg, ${min_vulkan}-${max_vulkan} by Vulkan, at least ${min_decoded} by FFmpeg and Vulkan" >&${fd}
            status=1
            continue
        fi

        if [ -z "${max_ms}" ] ; then
            maximum="no max"
        elif [ "${duration_ms}" -gt "${max_ms}" ] ; then
            echo "NOK: ${test}/${input}, slow decoding, ${count} frames in ${duration_ms} ms (max ${max_ms} ms), ${decoded}" >&${fd}
            status=1
            continue
        else
            maximum="max ${max_ms} ms"
        fi
        echo "OK: ${test}/${input}, hashes match, ${count} frames in ${duration_ms} ms (${maximum}), ${decoded}" >&${fd}
    done

    clean
popd >/dev/null 2>&1

exit ${status}
