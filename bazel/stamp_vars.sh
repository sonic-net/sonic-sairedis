#!/bin/bash
set -euo pipefail

sairedis_revision=$(git rev-parse --short HEAD 2>/dev/null || printf '0000000')
sai_revision=$(git rev-parse HEAD:SAI 2>/dev/null || printf '0000000')
sai_revision=${sai_revision:0:7}
printf 'STABLE_SAIREDIS_GIT_REVISION %s\n' "$sairedis_revision"
printf 'STABLE_SAI_GIT_REVISION %s\n' "$sai_revision"
