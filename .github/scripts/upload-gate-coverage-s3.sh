#!/usr/bin/env bash
# Upload gate coverage to S3. One upload per repo/branch/UTC-day; skip if already present.
set -euo pipefail

REPO_SLUG="${1:?usage: upload-gate-coverage-s3.sh <repo-slug> [subpath]}"
SUBPATH="${2:-}"
: "${S3_COVERAGE_BUCKET:?S3_COVERAGE_BUCKET required}"
: "${GATE_BRANCH_NAME:?GATE_BRANCH_NAME required}"

HTML_DIR="${HTML_DIR:-html}"
COVERAGE_INFO="${COVERAGE_INFO:-coverage.info}"
COVERAGE_XML="${COVERAGE_XML:-coverage.xml}"

DAY="$(date -u +"%Y-%m-%d")"
STAMP="$(date -u +"%Y-%m-%d-%H:%M")"
SAFE_REF="${GATE_BRANCH_NAME//\//_}"
if [ -n "$SUBPATH" ]; then
  S3_PREFIX="${REPO_SLUG}/${SUBPATH}"
else
  S3_PREFIX="${REPO_SLUG}"
fi
S3_DEST="s3://${S3_COVERAGE_BUCKET}/${S3_PREFIX}/${STAMP}/${SAFE_REF}"

existing="$(
  aws s3api list-objects-v2 \
    --bucket "${S3_COVERAGE_BUCKET}" \
    --prefix "${S3_PREFIX}/${DAY}" \
    --query "Contents[?contains(Key, '${SAFE_REF}/coverage.info')].Key | [0]" \
    --output text 2>/dev/null || true
)"
if [ -n "${existing}" ] && [ "${existing}" != "None" ]; then
  echo "Coverage report already uploaded today: s3://${S3_COVERAGE_BUCKET}/${existing}"
  exit 0
fi

if [ ! -d "${HTML_DIR}" ]; then
  echo "::error::${HTML_DIR}/ missing"
  exit 1
fi
if [ ! -f "${COVERAGE_INFO}" ]; then
  echo "::error::${COVERAGE_INFO} missing"
  exit 1
fi

aws s3 sync "${HTML_DIR}/" "${S3_DEST}/" --exclude "*.gcda" --exclude "*.gcno"
aws s3 cp "${COVERAGE_INFO}" "${S3_DEST}/coverage.info"
if [ -f "${COVERAGE_XML}" ]; then
  aws s3 cp "${COVERAGE_XML}" "${S3_DEST}/coverage.xml"
fi
echo "Uploaded to ${S3_DEST}/"
