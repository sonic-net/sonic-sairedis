#!/usr/bin/env python3

import importlib.util
import os
import tempfile
import unittest
from unittest import mock


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MODULE_PATH = os.path.join(SCRIPT_DIR, "derive_ptf_version.py")
SPEC = importlib.util.spec_from_file_location("derive_ptf_version", MODULE_PATH)
VERSION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERSION)


class DerivePtfVersionTest(unittest.TestCase):
    def test_release_tag(self):
        self.assertEqual("0.12.1", VERSION.version_from_describe("v0.12.1-0-gabc1234"))

    def test_post_release(self):
        self.assertEqual(
            "0.12.1.post7+gd587084",
            VERSION.version_from_describe("v0.12.1-7-gd587084"),
        )

    def test_invalid_description(self):
        with self.assertRaisesRegex(ValueError, "unsupported git describe"):
            VERSION.version_from_describe("d587084")

    def test_version_file_fallback(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            with open(os.path.join(temp_dir, "Version.txt"), "w", encoding="utf-8") as version_file:
                version_file.write("0.12.1\n")
            self.assertEqual(
                "0.12.1+gd587084",
                VERSION.fallback_version(temp_dir, "d587084"),
            )

    def test_untagged_fallback(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            with mock.patch.object(VERSION, "git", return_value="185"):
                self.assertEqual(
                    "0.0.post185+g4ee4c6a",
                    VERSION.fallback_version(temp_dir, "4ee4c6a"),
                )


if __name__ == "__main__":
    unittest.main()
