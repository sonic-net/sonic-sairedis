#!/usr/bin/env bash
# Follow Common's prepare-before-init / trace-owner-only CodeQL boundary.
set -euo pipefail
mode="${1:?expected prepare or trace}"
base="${SAIREDIS_CODEQL_OUTPUT_BASE:?missing fresh CodeQL output base}"
evidence="$PWD/artifacts/codeql"
flags=(--lockfile_mode=update --jobs=2 --spawn_strategy=local
       --action_env=PATH=/usr/bin:/bin
       --noremote_accept_cached --noremote_upload_local_results
       --disk_cache= --remote_cache= --remote_executor=)
targets=(@sonic_swss_common//:libswsscommon_shared
         @libnl3//:libnl_3 @libnl3//:libnl_route_3
         @libnl3//:libnl_nf_3 @libnl3//:libnl_genl_3
         @com_google_googletest//:gtest_main
         //bazel/sai:metadata //bazel/sai:redis_stub
         //bazel/sai:vs_stub //bazel/sai:proxy_stub
         //pyext:pysairedis_swig_py3)
case "$mode" in
  prepare)
    test -d "$base"
    test -z "$(find "$base" -mindepth 1 -maxdepth 1 -print -quit)"
    mkdir -p "$evidence"
    git rev-parse HEAD > "$evidence/tested-revision.txt"
    bazel --batch --output_base="$base" aquery 'deps(//:canonical_outputs)' \
      "${flags[@]}" --output=jsonproto --include_commandline > "$base/canonical-actions.json"
    python3 bazel/ci/codeql/check.py audit "$base/canonical-actions.json" \
      --output "$evidence/canonical-audit.json"
    python3 bazel/ci/check_source_reachability.py \
      --manifest bazel/ci/expected_no_vpp_sources.json \
      --output "$evidence/source-reachability.json" < "$base/canonical-actions.json"
    bazel --batch --output_base="$base" aquery \
      "deps(set(${targets[*]}))" "${flags[@]}" --output=jsonproto --include_commandline > "$base/prepared-actions.json"
    python3 bazel/ci/codeql/check.py audit "$base/prepared-actions.json" \
      --preparation --output "$evidence/preparation-audit.json"
    bazel --batch --output_base="$base" build "${flags[@]}" \
      --build_event_json_file="$evidence/prepared-events.jsonl" "${targets[@]}"
    python3 bazel/ci/codeql/check.py snapshot "$evidence/source-reachability.json" \
      --build-events "$evidence/prepared-events.jsonl" --output-base "$base" \
      --output "$evidence/expected-sources.json"
    cp MODULE.bazel.lock "$evidence/MODULE.bazel.lock"
    ;;
  trace)
    test "$(cat "$evidence/tested-revision.txt")" = "$(git rev-parse HEAD)"
    test -s "$evidence/expected-sources.json"
    # This fresh base has prepared dependencies, but no Sairedis C/C++ action.
    # All owner compilation therefore occurs after CodeQL initialization.
    bazel --batch --output_base="$base" build "${flags[@]}" //:canonical_outputs
    ;;
  *) printf 'Unknown stage: %s\n' "$mode" >&2; exit 2 ;;
esac
