"""Check effective fortification arguments in target C/C++ compilation actions."""

import argparse
import collections
import json

from check_source_reachability import normalize_main_label, primary_source


TARGETS = {
    "//lib:sairedis.a",
    "//meta:libsaimetadata",
    "//pyext:_pysairedis.so",
    "//syncd:libsyncd",
    "//syncd:syncd",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("normal", "asan"))
    parser.add_argument("aquery", type=argparse.FileType("r"))
    args = parser.parse_args()
    graph = json.load(args.aquery)
    labels = {t["id"]: normalize_main_label(t["label"]) for t in graph["targets"]}
    tool_configs = {c["id"] for c in graph["configuration"] if c.get("isTool")}
    counts = collections.Counter()
    for action in graph["actions"]:
        if action["mnemonic"] != "CppCompile" or action["configurationId"] in tool_configs:
            continue
        label = labels[action["targetId"]]
        if label not in TARGETS:
            continue
        argv = action["arguments"]
        flags = [a for a in argv if a.startswith("-D_FORTIFY_SOURCE") or a == "-U_FORTIFY_SOURCE"]
        if args.mode == "normal":
            ok = flags[-2:] == ["-U_FORTIFY_SOURCE", "-D_FORTIFY_SOURCE=3"]
        else:
            ok = "-D_FORTIFY_SOURCE=3" not in flags
            if label == "//syncd:syncd":
                ok = ok and flags[-1:] == ["-U_FORTIFY_SOURCE"] and "-fsanitize=address" in argv
        if not ok:
            raise SystemExit(f"{args.mode} {label} {primary_source(action)}: unexpected compiler flags {flags}")
        counts[label] += 1
    if missing := TARGETS.difference(counts):
        raise SystemExit(f"{args.mode}: missing compile actions for {sorted(missing)}")
    print(f"{args.mode}: verified fortification on target compile actions: {dict(sorted(counts.items()))}")


if __name__ == "__main__":
    main()
