#!/bin/bash
#
# sai-meta-fdb.sh - operator front-end for libsairedis meta-layer FDB diagnostics
#
# libsairedis (running inside the orchagent process) subscribes a dedicated,
# tolerant consumer on the SAIREDIS_META_FDB_DEBUG redis pub/sub channel. This
# script PUBLISHes operator commands to that channel via sonic-db-cli.
#
#   dump   - log the meta-layer FDB mirror + desync counters
#            (read-only; output goes to syslog at NOTICE level)
#
# Results are visible in syslog. Grep for "meta FDB" / "FDB desync" in the
# swss/orchagent logs.
#
# NOTE: pub/sub is not scoped to a redis database, so the DB argument to
# sonic-db-cli is irrelevant for PUBLISH; ASIC_DB is used for clarity.

set -euo pipefail

CHANNEL="SAIREDIS_META_FDB_DEBUG"
DB="ASIC_DB"

usage() {
    echo "Usage:" >&2
    echo "  $0 dump" >&2
    exit 1
}

publish() {
    # $1 = op, $2 = data
    local op="$1"
    local data="$2"
    # Flat JSON array [op, data] as expected by swss NotificationConsumer::pop
    local msg
    msg=$(printf '["%s","%s"]' "$op" "$data")
    sonic-db-cli "$DB" PUBLISH "$CHANNEL" "$msg"
    echo "published to $CHANNEL: $msg"
    echo "check syslog (grep 'meta FDB' / 'FDB desync') for results"
}

[ $# -ge 1 ] || usage

cmd="$1"; shift || true

case "$cmd" in
    dump)
        [ $# -eq 0 ] || usage
        publish "dump" ""
        ;;
    *)
        usage
        ;;
esac
