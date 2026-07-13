#!/usr/bin/env python3
"""
Generate a Markdown compatibility matrix from PTF JUnit-XML results.

The VPP SAI unit-test harness (run_test.sh) writes one JUnit-XML file per test
into its --xunit-dir (mounted out as /test-results -> a host directory). By
convention that host directory is this harness's own results tree:

    docker-sai-test-vpp/results/xml/                      <- per-test TEST-*.xml
    docker-sai-test-vpp/results/compatibility-matrix.md  <- generated matrix

This script walks the TEST-*.xml files and emits a PASS/FAIL/ERROR/SKIP table.

Usage:
    # use the default results tree next to this script
    python3 gen_compatibility_matrix.py

    # or point at an explicit xml dir / output file
    python3 gen_compatibility_matrix.py <xml_dir> [output.md]

Example workflow:
    cd docker-sai-test-vpp
    mkdir -p results/xml
    docker run --rm --privileged -e PORT_COUNT=32 \
        -v "$PWD/results/xml:/test-results" \
        docker-sai-test-vpp:phase1 \
        sai_route_test sai_rif_test sai_neighbor_test sai_ecmp_test \
        2>&1 | tee results/run.log
    python3 gen_compatibility_matrix.py        # writes results/compatibility-matrix.md
"""

import sys
import os
import glob
import datetime

# Use defusedxml (hardened against XXE / entity-expansion attacks) to parse the
# PTF JUnit XML. Install with `pip install defusedxml` if it is not already present.
import defusedxml.ElementTree as ET

# Default results tree lives alongside this script, under docker-sai-test-vpp/.
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_RESULTS_DIR = os.path.join(_SCRIPT_DIR, "results")
DEFAULT_XML_DIR = os.path.join(DEFAULT_RESULTS_DIR, "xml")
DEFAULT_OUTPUT = os.path.join(DEFAULT_RESULTS_DIR, "compatibility-matrix.md")

ICON = {
    "PASS": "\u2705 PASS",
    "FAIL": "\u274c FAIL",
    "ERROR": "\u26a0\ufe0f ERROR",
    "SKIP": "\u23ed\ufe0f SKIP",
}

# Result-status key (the icons used in the Result column).
RESULT_LEGEND = [
    ("\u2705 PASS", "test passed"),
    ("\u274c FAIL", "assertion failed (e.g. expected packet not received)"),
    ("\u26a0\ufe0f ERROR", "test errored out (exception / setUp / tearDown)"),
    ("\u23ed\ufe0f SKIP", "test was skipped"),
]

# SAI status codes that commonly appear in the Detail column. Values mirror
# SAI/inc/saistatus.h so a failing `status == -N` is readable at a glance.
SAI_STATUS_LEGEND = [
    ("0", "SAI_STATUS_SUCCESS"),
    ("-1", "SAI_STATUS_FAILURE"),
    ("-2", "SAI_STATUS_NOT_SUPPORTED"),
    ("-3", "SAI_STATUS_NO_MEMORY"),
    ("-4", "SAI_STATUS_INSUFFICIENT_RESOURCES"),
    ("-5", "SAI_STATUS_INVALID_PARAMETER"),
    ("-6", "SAI_STATUS_ITEM_ALREADY_EXISTS"),
    ("-7", "SAI_STATUS_ITEM_NOT_FOUND"),
    ("-8", "SAI_STATUS_BUFFER_OVERFLOW"),
    ("-9", "SAI_STATUS_INVALID_PORT_NUMBER"),
    ("-10", "SAI_STATUS_INVALID_PORT_MEMBER"),
    ("-11", "SAI_STATUS_INVALID_VLAN_ID"),
    ("-12", "SAI_STATUS_UNINITIALIZED"),
    ("-13", "SAI_STATUS_TABLE_FULL"),
    ("-14", "SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING"),
    ("-15", "SAI_STATUS_NOT_IMPLEMENTED"),
    ("-16", "SAI_STATUS_ADDR_NOT_FOUND"),
    ("-17", "SAI_STATUS_OBJECT_IN_USE"),
]


def collect_rows(xml_dir):
    rows = []
    for path in sorted(glob.glob(os.path.join(xml_dir, "TEST-*.xml"))):
        try:
            root = ET.parse(path).getroot()
        except Exception as e:  # malformed XML -> surface, don't crash
            rows.append(("?", os.path.basename(path), "PARSE-ERR", str(e)[:60]))
            continue
        suites = [root] if root.tag == "testsuite" else root.iter("testsuite")
        for suite in suites:
            for tc in suite.iter("testcase"):
                mod = tc.get("classname", "")
                name = tc.get("name", "")
                fail = tc.find("failure")
                err = tc.find("error")
                skip = tc.find("skipped")
                if err is not None:
                    status, detail = "ERROR", (err.get("message") or "")
                elif fail is not None:
                    status, detail = "FAIL", (fail.get("message") or "")
                elif skip is not None:
                    status, detail = "SKIP", (skip.get("message") or "")
                else:
                    status, detail = "PASS", ""
                detail = detail.replace("\n", " ").strip()[:80]
                rows.append((mod, name, status, detail))
    rows.sort()
    return rows


def render(rows):
    counts = {}
    for r in rows:
        counts[r[2]] = counts.get(r[2], 0) + 1
    total = len(rows)
    out = []
    out.append("# VPP SAI Compatibility Matrix\n")
    generated = datetime.datetime.now().astimezone().strftime("%Y-%m-%d %H:%M:%S %Z")
    out.append(f"_Generated: {generated}_\n")

    # Summary table: one row per status (in a stable, meaningful order) plus a
    # total, with counts and percentages.
    out.append("## Summary\n")
    out.append("| Result | Count | % |")
    out.append("|--------|------:|----:|")
    order = ["PASS", "FAIL", "ERROR", "SKIP"]
    seen = set(order)
    for status in order + [s for s in sorted(counts) if s not in seen]:
        n = counts.get(status, 0)
        if n == 0 and status in seen:
            continue
        pct = (100.0 * n / total) if total else 0.0
        out.append(f"| {ICON.get(status, status)} | {n} | {pct:.1f}% |")
    out.append(f"| **Total** | **{total}** | **100.0%** |")
    out.append("")

    # Legend: result-status icons + SAI status codes seen in the Detail column.
    out.append("## Legend\n")
    out.append("**Result status**\n")
    out.append("| Icon | Meaning |")
    out.append("|------|---------|")
    for icon, meaning in RESULT_LEGEND:
        out.append(f"| {icon} | {meaning} |")
    out.append("")
    out.append("**SAI status codes** (see `SAI/inc/saistatus.h`) \u2014 appear in the Detail column\n")
    out.append("| Code | Symbol |")
    out.append("|------|--------|")
    for code, symbol in SAI_STATUS_LEGEND:
        out.append(f"| `{code}` | `{symbol}` |")
    out.append("")

    out.append("## Results\n")
    out.append("| Module | Test Class | Result | Detail |")
    out.append("|--------|------------|--------|--------|")
    for mod, name, status, detail in rows:
        out.append(f"| `{mod}` | `{name}` | {ICON.get(status, status)} | {detail} |")
    return "\n".join(out)


def main(argv):
    args = argv[1:]
    if args and args[0] in ("-h", "--help"):
        sys.stderr.write(__doc__)
        return 0

    xml_dir = args[0] if len(args) >= 1 else DEFAULT_XML_DIR
    # output: explicit 2nd arg, else default file when using the default xml dir,
    # else stdout (so piping `> file.md` still works for a custom dir).
    if len(args) >= 2:
        output = args[1]
    elif len(args) == 0:
        output = DEFAULT_OUTPUT
    else:
        output = None  # custom xml dir, no output given -> stdout

    if not os.path.isdir(xml_dir):
        sys.stderr.write(
            f"error: xml dir not found: {xml_dir}\n"
            f"       run the harness with -v <host>/results/xml:/test-results first.\n"
        )
        return 2

    matrix = render(collect_rows(xml_dir))
    if output:
        os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
        with open(output, "w", encoding="utf-8") as f:
            f.write(matrix + "\n")
        sys.stderr.write(f"wrote {output}\n")
    else:
        print(matrix)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
