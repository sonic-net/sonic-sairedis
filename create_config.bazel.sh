#!/bin/bash
set -euo pipefail

output=${1:?output path is required}
status_file=${2:?stable status file is required}
asic_platform=${3:-generic}

read_revision() {
    local key=$1
    local name value=
    while read -r name value; do
        if [[ $name == "$key" ]]; then
            break
        fi
        value=
    done < "$status_file"
    if [[ ! $value =~ ^[0-9a-fA-F]+$ ]]; then
        value=0000000
    fi
    printf '%s' "$value"
}

sairedis_revision=$(read_revision STABLE_SAIREDIS_GIT_REVISION)
sai_revision=$(read_revision STABLE_SAI_GIT_REVISION)

{
    printf '#pragma once\n'
    printf '#define SAIREDIS_GIT_REVISION "%s"\n' "$sairedis_revision"
    printf '#define SAI_GIT_REVISION "%s"\n' "$sai_revision"
    printf '#define HAVE_SAI_BULK_OBJECT_CLEAR_STATS 1\n'
    printf '#define HAVE_SAI_BULK_OBJECT_GET_STATS 1\n'
    printf '#define HAVE_SAI_QUERY_STATS_ST_CAPABILITY 1\n'
    printf '#define HAVE_SAI_TAM_TELEMETRY_GET_DATA 1\n'
    if [[ $asic_platform == mellanox ]]; then
        printf '#define MELLANOX 1\n'
    fi
} > "$output"
