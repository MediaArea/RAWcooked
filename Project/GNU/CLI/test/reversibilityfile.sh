#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="reversibility_data"

while read line ; do
    path="$(echo "${line}" | cut -d' ' -f1)"
    file="$(echo "${line}" | cut -s -d' ' -f2)"

    if [ -z "${file}" ] ; then
        file="../$(basename ${path})"
    fi

    if [ ! -e "${files_path}/${path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        status=1
        continue
    fi

    pushd "${files_path}/${path}" >/dev/null 2>&1
        # check presence of reversibility data file with --display-command
        run_rawcooked --file -d "${file}/"
        check_success "file rejected at input" "file accepted at input"

        if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, reversibility data file missing with --display-command" >&${fd}
            status=1
            clean
            continue
        fi
        mv -f "${file}.rawcooked_reversibility_data" "${file}.rawcooked_reversibility_data.save"

        # check if reversibility data file is cleaned when using internal encoder
        run_rawcooked --file "${file}/"
        check_success "file rejected at input" "file accepted at input"
        if [ -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, reversibility data file not cleaned after encoding" >&${fd}
            status=1
            clean
            continue
        fi

        # check if reversibility data file is cleaned with -n option and an existing mkv
        run_rawcooked -n "${file}/"
        if [ -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, reversibility data file not cleaned with -n option and an existing mkv" >&${fd}
            status=1
            clean
            continue
        fi
        rm -f "${file}.mkv"

        # check if stale reversibility data file is preserved with -n option
        cp -f "${file}.rawcooked_reversibility_data.save" "${file}.rawcooked_reversibility_data"
        run_rawcooked -n "${file}/"
        check_failure "file rejected due to -n option and stale reversibility data file" "file accepted despite -n option and stale reversibility data file"
        if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, stale reversibility data file removed despite -n option" >&${fd}
            status=1
            clean
            continue
        fi

        # check for error with -y option and an read only stale reversibility data file
        chmod 0444 "${file}.rawcooked_reversibility_data"
        run_rawcooked -y "${file}/"
        check_failure "file rejected due to read only stale reversibility data file" "file accepted despite read only stale reversibility data file"
        if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, read only stale reversibility data file removed" >&${fd}
            status=1
            clean
            continue
        fi

        # check for error with -y option and an stale reversibility data file with no rights
        chmod 0000 "${file}.rawcooked_reversibility_data"
        run_rawcooked -y "${file}/"
        check_failure "file rejected due to insufficient rights on stale reversibility data file" "file accepted despite insufficient rights on stale reversibility data file"
        if [ ! -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, reversibility data file removed despite insufficient rights" >&${fd}
            status=1
            clean
            continue
        fi
        chmod 0644 "${file}.rawcooked_reversibility_data"

        # check if stale reversibility data file is deleted with -y option
        run_rawcooked -y "${file}/"
        check_success "file rejected due to stale reversibility data file, despite -y" "file accepted with -y option and an stale reversibility data file"
        if [ -e "${file}.rawcooked_reversibility_data" ] ; then
            echo "NOK: ${test}/${file}, stale reversibility data file not removed despite -y option" >&${fd}
            status=1
            clean
            continue
        fi
        clean
    popd >/dev/null 2>&1
done < "${script_path}/reversibilityfile.txt"

exit ${status}
