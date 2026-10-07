#!/usr/bin/env python3
"""Evaluate the stable SAIVPP baseline against PTF JUnit XML results."""

import argparse
import glob
import os
import sys
from collections import defaultdict
from xml.sax.saxutils import escape, quoteattr

import defusedxml.ElementTree as ET


STATUS_ORDER = ("ERROR", "FAIL", "SKIP", "PASS")
RESULT_TAGS = {"ERROR": "error", "FAIL": "failure", "SKIP": "skipped"}


def read_baseline(path):
    selectors = []
    seen = set()
    with open(path, encoding="utf-8") as baseline_file:
        for line_number, raw_line in enumerate(baseline_file, 1):
            line = raw_line.split("#", 1)[0].strip()
            if not line:
                continue
            if line in seen:
                raise ValueError(f"duplicate baseline selector at line {line_number}: {line}")
            seen.add(line)
            selectors.append(line)
    if not selectors:
        raise ValueError(f"baseline is empty: {path}")
    return set(selectors)


def testcase_status(testcase):
    if testcase.find("error") is not None:
        return "ERROR"
    if testcase.find("failure") is not None:
        return "FAIL"
    if testcase.find("skipped") is not None:
        return "SKIP"
    return "PASS"


def collect_results(xml_dir):
    statuses = defaultdict(list)
    for selector, testcase in iter_testcases(xml_dir):
        statuses[selector].append(testcase_status(testcase))

    if not statuses:
        raise ValueError(f"no testcases found in {xml_dir}")
    return statuses


def iter_testcases(xml_dir):
    xml_paths = sorted(glob.glob(os.path.join(xml_dir, "TEST-*.xml")))
    if not xml_paths:
        raise ValueError(f"no TEST-*.xml files found in {xml_dir}")

    for xml_path in xml_paths:
        try:
            root = ET.parse(xml_path).getroot()
        except Exception as exc:
            raise ValueError(f"cannot parse {xml_path}: {exc}") from exc

        suites = [root] if root.tag == "testsuite" else root.iter("testsuite")
        for suite in suites:
            for testcase in suite.iter("testcase"):
                selector = (testcase.get("classname") or "").strip()
                if not selector:
                    module = (suite.get("name") or "").strip()
                    name = (testcase.get("name") or "").strip()
                    selector = f"{module}.{name}" if module and name else ""
                if not selector:
                    raise ValueError(f"testcase without selector in {xml_path}")
                yield selector, testcase


def write_junit(xml_dir, path, suite_name="VPP SAI PTF"):
    """Write one JUnit file whose testcases are named module.Class (PTF names them all runTest)."""
    cases = []
    counts = defaultdict(int)
    for selector, testcase in iter_testcases(xml_dir):
        status = testcase_status(testcase)
        counts[status] += 1
        body = ""
        if status in RESULT_TAGS:
            tag = RESULT_TAGS[status]
            result = testcase.find(tag)
            message = quoteattr(result.get("message") or "")
            body = f"<{tag} message={message}>{escape(result.text or '')}</{tag}>"
        time = quoteattr(testcase.get("time") or "0")
        cases.append(
            f"  <testcase classname={quoteattr(selector)} name={quoteattr(selector)} "
            f"time={time}>{body}</testcase>"
        )

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as junit_file:
        junit_file.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        junit_file.write(
            f"<testsuite name={quoteattr(suite_name)} tests=\"{len(cases)}\" "
            f"failures=\"{counts['FAIL']}\" errors=\"{counts['ERROR']}\" "
            f"skipped=\"{counts['SKIP']}\">\n"
        )
        junit_file.write("\n".join(cases) + "\n</testsuite>\n")


def aggregate_status(statuses):
    for status in STATUS_ORDER:
        if status in statuses:
            return status
    raise ValueError(f"unknown test status list: {statuses}")


def format_report(baseline, results, matrix_rc):
    observed = {selector: aggregate_status(statuses)
                for selector, statuses in results.items()}
    regressions = []
    for selector in sorted(baseline):
        status = observed.get(selector)
        if status is None:
            regressions.append((selector, "MISSING"))
        elif status != "PASS":
            regressions.append((selector, status))

    candidates = sorted(selector for selector, status in observed.items()
                       if status == "PASS" and selector not in baseline)
    nonpasses = sorted((selector, status) for selector, status in observed.items()
                       if status != "PASS" and selector not in baseline)
    regressed = {selector for selector, _ in regressions}
    baseline_passes = sorted(baseline - regressed)

    lines = [
        "SAIVPP CI baseline evaluation",
        f"Matrix exit code: {matrix_rc}",
        f"Baseline selectors: {len(baseline)}",
        f"Observed selectors: {len(observed)}",
        f"Stable baseline passes: {len(baseline_passes)}",
        f"Regressions: {len(regressions)}",
        f"New pass candidates: {len(candidates)}",
        f"Known non-baseline non-passes: {len(nonpasses)}",
    ]
    if regressions:
        lines.append("Regressions:")
        lines.extend(f"  {selector}: {status}" for selector, status in regressions)
    if baseline_passes:
        lines.append("Stable baseline passes:")
        lines.extend(f"  {selector}" for selector in baseline_passes)
    if candidates:
        lines.append("New pass candidates:")
        lines.extend(f"  {selector}" for selector in candidates)
    if nonpasses:
        lines.append("Known non-baseline non-passes:")
        lines.extend(f"  {selector}: {status}" for selector, status in nonpasses)
    return "\n".join(lines) + "\n", regressions


def validate_matrix_contract(expected, results):
    observed = set(results)
    missing = sorted(expected - observed)
    unexpected = sorted(observed - expected)
    if not missing and not unexpected:
        return None

    lines = ["Matrix selector contract mismatch"]
    if missing:
        lines.append("Missing expected selectors:")
        lines.extend(f"  {selector}" for selector in missing)
    if unexpected:
        lines.append("Unexpected selectors:")
        lines.extend(f"  {selector}" for selector in unexpected)
    return "\n".join(lines)


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xml-dir", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--expected", required=True)
    parser.add_argument("--matrix-rc", type=int, default=0)
    parser.add_argument("--report")
    parser.add_argument("--junit-out", help="write a merged JUnit file with module.Class test names")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    try:
        baseline = read_baseline(args.baseline)
        expected = read_baseline(args.expected)
        if not baseline <= expected:
            unknown = ", ".join(sorted(baseline - expected))
            raise ValueError(f"baseline selectors are not in expected matrix: {unknown}")
        if args.matrix_rc >= 2:
            report = (
                "SAIVPP CI baseline evaluation\n"
                f"Matrix exit code: {args.matrix_rc}\n"
                "Infrastructure failure: matrix setup did not complete\n"
            )
            regressions = [("<matrix>", "INFRASTRUCTURE")]
        else:
            results = collect_results(args.xml_dir)
            contract_error = validate_matrix_contract(expected, results)
            if contract_error:
                report = (
                    "SAIVPP CI baseline evaluation\n"
                    f"Matrix exit code: {args.matrix_rc}\n"
                    f"Infrastructure failure: {contract_error}\n"
                )
                regressions = [("<matrix>", "INCOMPLETE")]
            else:
                report, regressions = format_report(baseline, results, args.matrix_rc)
    except (OSError, ValueError) as exc:
        report = f"SAIVPP CI baseline evaluation\nInfrastructure failure: {exc}\n"
        regressions = [("<evaluator>", "INFRASTRUCTURE")]

    if args.report:
        report_dir = os.path.dirname(os.path.abspath(args.report))
        os.makedirs(report_dir, exist_ok=True)
        with open(args.report, "w", encoding="utf-8") as report_file:
            report_file.write(report)
    if args.junit_out:
        try:
            write_junit(args.xml_dir, args.junit_out)
        except (OSError, ValueError) as exc:
            print(f"warning: cannot write {args.junit_out}: {exc}", file=sys.stderr)
    print(report, end="")
    return 1 if regressions else 0


if __name__ == "__main__":
    sys.exit(main())
