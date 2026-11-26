import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

import check


def graph(label="@dependency//:library", mnemonic="CppCompile", output="library.o", arguments=None):
    return {"targets": [{"id": 1, "label": label}],
            "pathFragments": [{"id": 1, "label": "bazel-out"},
                              {"id": 2, "parentId": 1, "label": output}],
            "artifacts": [{"id": 1, "pathFragmentId": 2}],
            "actions": [{"targetId": 1, "mnemonic": mnemonic, "outputIds": [1],
                         "arguments": arguments or ["gcc", "-c", "source.cc"]}]}


class PreparationTest(unittest.TestCase):
    def test_external_compilation_and_owner_generator_are_allowed(self):
        self.assertEqual(check.audit(graph(), True)["owner_cpp_actions"], 0)
        check.audit(graph("//bazel/sai:redis_stub", "SaiStub", "sai_redis.cpp"), True)

    def test_each_root_label_spelling_rejects_pre_init_compilation(self):
        for prefix in ("//", "@//", "@@//", "@_main//", "@@_main//"):
            with self.subTest(prefix=prefix), self.assertRaisesRegex(AssertionError, "before tracing"):
                check.audit(graph(prefix + "lib:sairedis"), True)

    def test_traced_owner_compilation_is_allowed(self):
        self.assertEqual(check.audit(graph("//lib:sairedis"))["owner_cpp_actions"], 1)

    def test_rejects_packaging_outputs_and_wrappers(self):
        for suffix in ("deb", "udeb", "ddeb"):
            with self.subTest(suffix=suffix), self.assertRaisesRegex(AssertionError, "DEB-producing"):
                check.audit(graph(output="test." + suffix))
        with self.assertRaisesRegex(AssertionError, "packaging wrapper"):
            check.audit(graph(arguments=["/bin/bash", "-c", "cd src; /usr/bin/dpkg-buildpackage -b"]))

    def test_empty_graph_does_not_pass(self):
        with self.assertRaisesRegex(AssertionError, "empty"):
            check.audit({})


class ExecutionRootTest(unittest.TestCase):
    def test_actual_successful_bep_selects_the_prepared_root(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "output-base"
            root = base / "execroot/_main"
            root.mkdir(parents=True)
            events = base / "events.jsonl"
            records = [{"id": {"workspace": {}}, "workspaceInfo": {"localExecRoot": str(root)}},
                       {"id": {"buildFinished": {}}, "finished": {"overallSuccess": True, "exitCode": {"name": "SUCCESS"}}}]
            events.write_text("\n".join(map(json.dumps, records)) + "\n")
            self.assertEqual(check.execution_root_from_events(events, base), root)
            records[1]["finished"]["exitCode"]["code"] = 0
            events.write_text("\n".join(map(json.dumps, records)) + "\n")
            self.assertEqual(check.execution_root_from_events(events, base), root)

    def test_incomplete_failed_ambiguous_or_foreign_build_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "output-base"
            root = base / "execroot/_main"
            root.mkdir(parents=True)
            events = base / "events.jsonl"
            workspace = {"workspaceInfo": {"localExecRoot": str(root)}}
            success = {"finished": {"overallSuccess": True, "exitCode": {"name": "SUCCESS"}}}
            for records in ([workspace], [workspace, {"finished": {"exitCode": {"code": 1}}}],
                            [workspace, {"finished": {"exitCode": {"name": "SUCCESS"}}}],
                            [workspace, {"workspaceInfo": {"localExecRoot": str(base)}}, success],
                            [{"workspaceInfo": {"localExecRoot": str(base)}}, success]):
                with self.subTest(records=records), self.assertRaises(AssertionError):
                    events.write_text("\n".join(map(json.dumps, records)) + "\n")
                    check.execution_root_from_events(events, base)


class ExtractionTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.workspace = self.root / "workspace"
        self.workspace.mkdir()
        (self.workspace / "owner.cc").write_bytes(b"int main() { return 0; }\n")
        self.execroot = self.root / "execroot"
        self.execroot.mkdir()
        (self.execroot / "owner.cc").symlink_to(self.workspace / "owner.cc")
        self.report = {"status": "pass", "expected_count": 1,
                       "ownership": {"expected_pair_count": 1},
                       "component_actions": [{"primary_source": "owner.cc", "normalized_source": "owner.cc",
                                              "configuration": {"is_tool": False}}]}
        self.expected = check.snapshot(self.report, self.execroot)
        self.archive = self.root / "src.zip"
        self.physical = str(self.workspace / "owner.cc")

    def write_archive(self, content=b"int main() { return 0; }\n"):
        with zipfile.ZipFile(self.archive, "w") as archive:
            archive.writestr(self.physical.lstrip("/"), content)

    def test_actual_compilation_and_exact_archive_bytes_pass(self):
        self.write_archive()
        result = check.verify_extraction(self.expected, [[self.physical]], self.archive)
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["sources"]["owner.cc"]["sha256"], hashlib.sha256((self.workspace / "owner.cc").read_bytes()).hexdigest())

    def test_archive_presence_without_compilation_is_rejected(self):
        self.write_archive()
        with self.assertRaisesRegex(AssertionError, "No actual extracted compilation"):
            check.verify_extraction(self.expected, [], self.archive)

    def test_same_basename_elsewhere_is_not_coverage(self):
        self.write_archive()
        with self.assertRaisesRegex(AssertionError, "No actual extracted compilation"):
            check.verify_extraction(self.expected, [["/unrelated/owner.cc"]], self.archive)

    def test_stale_extracted_bytes_are_rejected(self):
        self.write_archive(b"int main() { return 1; }\n")
        with self.assertRaisesRegex(AssertionError, "bytes differ"):
            check.verify_extraction(self.expected, [[self.physical]], self.archive)

    def test_missing_generated_file_and_failed_reachability_are_rejected(self):
        report = copy.deepcopy(self.report)
        report["component_actions"][0]["primary_source"] = "not-generated.cc"
        with self.assertRaises(FileNotFoundError):
            check.snapshot(report, self.execroot)
        report["status"] = "fail"
        with self.assertRaisesRegex(AssertionError, "did not pass"):
            check.snapshot(report, self.execroot)


if __name__ == "__main__":
    unittest.main()
