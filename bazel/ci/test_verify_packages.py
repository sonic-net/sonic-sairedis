"""Exercise provenance checks when draft CI selects an immutable registry."""

import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import verify_packages


class RegistryProvenanceTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.main = "https://example.invalid/registry/main"
        self.draft = "https://example.invalid/registry/" + "a" * 40
        (self.root / ".bazelrc").write_text("common --registry=" + self.main + "\n")
        self.effective = self.root / "effective.bazelrc"
        self.effective.write_text("common --registry=" + self.draft + "\n")
        self.args = SimpleNamespace(bazel="bazel", bazel_config="", registry_config=self.effective)
        self.versions = {"sai": "1.0", "sonic-swss-common": "2.0", "sonic-build-infra": "3.0"}
        self.targets = {
            "@sai_source//:headers": "sai",
            "@sonic_swss_common//:libswsscommon_shared": "sonic-swss-common",
            "@sonic_build_infra//platforms:x86_64_trixie": "sonic-build-infra",
        }
        self.lock = {}
        for name, version in self.versions.items():
            module_root = self.root / "external" / (name + "+")
            module_root.mkdir(parents=True)
            (module_root / "MODULE.bazel").write_text(
                'module(name = "' + name + '", version = "' + version + '")\n')
            suffix = "/modules/" + name + "/" + version + "/source.json"
            self.lock[self.main + suffix] = "1" * 64
            self.lock[self.draft + suffix] = "2" * 64
        (self.root / "external/sai+/SOURCE_PROVENANCE.json").write_text(
            json.dumps({"source_commit": "b" * 40}))

    def resolve(self):
        (self.root / "MODULE.bazel.lock").write_text(json.dumps({"registryFileHashes": self.lock}))

        def query(command, cwd, log):
            self.assertEqual(command[:2], ["bazel", "cquery"])
            selected = command[2].removeprefix("config(").removesuffix(", target)")
            return (self.targets[selected] + "+\n").encode()

        with patch.object(verify_packages, "run_command", side_effect=query):
            return verify_packages.resolved_dependency_provenance(
                self.args, self.root, self.root, self.root,
                {"bazel_dependency_versions": self.versions})

    def test_effective_snapshot_excludes_stale_main_entries(self):
        """Draft builds must use their actual snapshot even if the lock also contains main."""
        for dependency in self.resolve().values():
            sources = dependency["registry_source"]
            self.assertEqual(list(sources.values()), ["2" * 64])
            self.assertTrue(next(iter(sources)).startswith(self.draft + "/"))

    def test_tracked_configuration_remains_supported(self):
        """Ordinary main-registry builds retain the original source-selection contract."""
        self.args.registry_config = self.root / ".bazelrc"
        for dependency in self.resolve().values():
            self.assertEqual(list(dependency["registry_source"].values()), ["1" * 64])

    def test_unselected_registry_cannot_supply_provenance(self):
        """A lock entry from another registry cannot hide a missing selected source hash."""
        self.lock = {url: digest for url, digest in self.lock.items() if url.startswith(self.main + "/")}
        with self.assertRaisesRegex(ValueError, "one selected registry source: sai"):
            self.resolve()

    def test_ambiguous_registry_sources_fail(self):
        """Two configured source hashes for one module must fail instead of choosing one."""
        self.effective.write_text("common --registry=" + self.main + "\ncommon --registry=" + self.draft + "\n")
        with self.assertRaisesRegex(ValueError, "one selected registry source: sai"):
            self.resolve()

    def test_fetched_module_version_must_still_match(self):
        """An effective registry override must not relax the fetched module version check."""
        (self.root / "external/sai+/MODULE.bazel").write_text('module(name="sai", version="9.9")\n')
        with self.assertRaisesRegex(ValueError, "resolved module differs from declared dependency: sai"):
            self.resolve()


if __name__ == "__main__":
    unittest.main()
