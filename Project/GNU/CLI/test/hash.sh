#!/usr/bin/env bash

script_path="${PWD}/test"
. ${script_path}/helpers.sh

test="hash"

while read file ; do
    if [ ! -e "${files_path}/${file}" ] ; then
        echo "NOK: ${test}/${file}, file not found" >&${fd}
        status=1
        continue
    fi

    output="${files_path}/${file}.mkv"

    # --output-version 1
    run_rawcooked --file --all --compute-output-hash "${files_path}/${file}"
    check_success "file rejected at input" "file accepted at input"
    
    computed_md5=$(echo "${cmd_stdout}" | sed -nE 's/.*MD5 is ([a-f0-9]{32}).*/\1/p')
    output_md5=$(${md5cmd} "${output}" | cut -d' ' -f1)
    
    # check result
    if [ "${computed_md5}" != "${output_md5}" ] ; then
        echo "NOK: ${test}/${file}, computed and output md5 differs, computed: ${computed_md5}, output: ${output_md5} " >&${fd}
        status=1
        clean
        continue
    fi

    clean

    # --output-version 2
    run_rawcooked --file --all --compute-output-hash --output-version 2 "${files_path}/${file}"
    check_success "file rejected at input" "file accepted at input"
    
    computed_md5=$(echo "${cmd_stdout}" | sed -nE 's/.*MD5 is ([a-f0-9]{32}).*/\1/p')
    output_md5=$(${md5cmd} "${output}" | cut -d' ' -f1)
    
    # check result
    if [ "${computed_md5}" != "${output_md5}" ] ; then
        echo "NOK: ${test}/${file}, computed and output md5 differs, computed: ${computed_md5}, output: ${output_md5} " >&${fd}
        status=1
        clean
        continue
    fi

    clean
done < "${script_path}/hash.txt"

exit ${status}
