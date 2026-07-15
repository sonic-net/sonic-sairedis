#!/usr/bin/env python3

import importlib.util
import os
import tempfile
import unittest


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODULE_PATH = os.path.join(SCRIPT_DIR, "evaluate_ci_baseline.py")
SPEC = importlib.util.spec_from_file_location("evaluate_ci_baseline", MODULE_PATH)
BASELINE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BASELINE)


class EvaluateCiBaselineTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.xml_dir = os.path.join(self.temp_dir.name, "xml")
        os.makedirs(self.xml_dir)

    def write_result(self, selector, result="PASS"):
        result_xml = {
            "PASS": "",
            "FAIL": '<failure message="failed"/>',
            "ERROR": '<error message="errored"/>',
            "SKIP": '<skipped message="skipped"/>',
        }[result]
        xml = (
            '<testsuite tests="1">'
            f'<testcase classname="{selector}" name="runTest">'
            f"{result_xml}</testcase></testsuite>"
        )
        filename = f"TEST-{selector}.xml"
        with open(os.path.join(self.xml_dir, filename), "w", encoding="utf-8") as xml_file:
            xml_file.write(xml)

    def write_selectors(self, filename, selectors):
        path = os.path.join(self.temp_dir.name, filename)
        with open(path, "w", encoding="utf-8") as selector_file:
            selector_file.write("\n".join(selectors) + "\n")
        return path

    def test_stable_pass_with_known_failure_and_candidate(self):
        self.write_result("module.StableTest")
        self.write_result("module.KnownFailure", "FAIL")
        self.write_result("module.NewPass")

        results = BASELINE.collect_results(self.xml_dir)
        report, regressions = BASELINE.format_report(
            {"module.StableTest"}, results, matrix_rc=1
        )

        self.assertEqual([], regressions)
        self.assertIn("New pass candidates: 1", report)
        self.assertIn("Known non-baseline non-passes: 1", report)

    def test_regressed_and_missing_baseline_tests_fail(self):
        self.write_result("module.RegressedTest", "ERROR")

        results = BASELINE.collect_results(self.xml_dir)
        _, regressions = BASELINE.format_report(
            {"module.RegressedTest", "module.MissingTest"}, results, matrix_rc=1
        )

        self.assertEqual(
            [("module.MissingTest", "MISSING"), ("module.RegressedTest", "ERROR")],
            regressions,
        )

    def test_malformed_junit_is_infrastructure_error(self):
        path = os.path.join(self.xml_dir, "TEST-broken.xml")
        with open(path, "w", encoding="utf-8") as xml_file:
            xml_file.write("<testsuite>")

        with self.assertRaisesRegex(ValueError, "cannot parse"):
            BASELINE.collect_results(self.xml_dir)

    def test_duplicate_baseline_selector_is_rejected(self):
        path = self.write_selectors("baseline.txt", ["module.Test", "module.Test"])

        with self.assertRaisesRegex(ValueError, "duplicate baseline selector"):
            BASELINE.read_baseline(path)

    def test_incomplete_matrix_is_rejected(self):
        self.write_result("module.StableTest")
        results = BASELINE.collect_results(self.xml_dir)

        error = BASELINE.validate_matrix_contract(
            {"module.StableTest", "module.ExpectedTest"}, results
        )

        self.assertIn("module.ExpectedTest", error)

    def test_infrastructure_matrix_status_fails_main(self):
        baseline = self.write_selectors("baseline.txt", ["module.StableTest"])
        expected = self.write_selectors("expected.txt", ["module.StableTest"])
        report = os.path.join(self.temp_dir.name, "report.txt")

        rc = BASELINE.main([
            "--xml-dir", self.xml_dir,
            "--baseline", baseline,
            "--expected", expected,
            "--matrix-rc", "2",
            "--report", report,
        ])

        self.assertEqual(1, rc)
        with open(report, encoding="utf-8") as report_file:
            self.assertIn("Infrastructure failure", report_file.read())


if __name__ == "__main__":
    unittest.main()
