"""Keep review selection, ordinary defaults and Bazel startup arguments distinct."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("prepare_review", Path(__file__).with_name("prepare_review.py"))
review = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(review)


class ReviewConfigurationTest(unittest.TestCase):
    def test_one_catalog_preserves_defaults_caches_and_startup_arguments(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            home = root / "home"
            home.mkdir()
            declared = "common --registry=" + review.MAIN + "\ncommon --registry=" + review.BCR + "\ncommon:aarch64 --platforms=@infra//:arm\n"
            (root / ".bazelrc").write_text(declared)
            (home / ".bazelrc").write_text("build --disk_cache=/retained/cache\ncommon --registry=https://unselected.invalid\nbuild --override_repository=owner=/unselected\n")
            binary = review.prepare(root, home, root / "evidence", "a" * 40, "/bin/echo")
            effective = (root / "evidence/effective.bazelrc").read_text()
            self.assertEqual((root / ".bazelrc").read_text(), declared)
            self.assertEqual(effective.count("--registry="), 2)
            self.assertNotIn(review.MAIN, effective)
            self.assertNotIn("unselected", effective)
            self.assertIn("build --disk_cache=/retained/cache", effective)
            self.assertIn("common:aarch64 --platforms=@infra//:arm", effective)
            arguments = ["--batch", "--output_base=/private/test", "build", "--config=aarch64", "//:target"]
            actual = subprocess.check_output([str(binary), *arguments], text=True)
            self.assertTrue(actual.rstrip().endswith(" ".join(arguments)))

    def test_mutable_catalog_and_ambiguous_declarations_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            text = "common --registry=" + review.MAIN + "\ncommon --registry=" + review.BCR + "\n"
            (root / ".bazelrc").write_text(text)
            with self.assertRaisesRegex(ValueError, "immutable"):
                review.prepare(root, root, root / "evidence", "main", "/bin/echo")
            (root / ".bazelrc").write_text(text + "common --registry=https://extra.invalid\n")
            with self.assertRaisesRegex(ValueError, "exactly"):
                review.prepare(root, root, root / "evidence", "a" * 40, "/bin/echo")

    def test_standalone_version_reaches_launcher_without_startup_options(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / ".bazelrc").write_text("common --registry=" + review.MAIN +
                                         "\ncommon --registry=" + review.BCR + "\n")
            launcher = root / "original-bazel"
            launcher.write_text('#!/bin/sh\n[ "$#" -eq 1 ] && [ "$1" = "--version" ] || exit 2\n'
                                'printf "bazel 8.5.1\\n"\n')
            launcher.chmod(0o755)
            binary = review.prepare(root, root, root / "evidence", "a" * 40, str(launcher))
            self.assertEqual(subprocess.check_output([str(binary), "--version"], text=True),
                             "bazel 8.5.1\n")


if __name__ == "__main__":
    unittest.main()
