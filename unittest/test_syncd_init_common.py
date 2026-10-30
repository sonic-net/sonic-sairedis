import os
import pathlib
import re
import shlex
import subprocess
import tempfile
import unittest


SOURCE = pathlib.Path(__file__).resolve().parents[1] / "syncd/scripts/syncd_init_common.sh"


class SyncdInitCommonTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = pathlib.Path(temporary.name)
        for path in (
            "etc/sai.d", "usr/share/sonic/hwsku", "usr/share/sonic/platform",
            "usr/share/sonic/templates", "tmp", "proc", "var/run/sswsyncd",
            "usr/share/sonic/device/x86_64-broadcom_common/x86_64-broadcom_b85",
        ):
            (self.root / path).mkdir(parents=True, exist_ok=True)
        self.etc = self.root / "etc/sai.d"
        self.hwsku = self.root / "usr/share/sonic/hwsku"
        self.platform = self.root / "usr/share/sonic/platform"
        self.templates = self.root / "usr/share/sonic/templates"
        self.template = self.templates / "sai_profile_dynamic.j2"
        self.template.touch()
        self.runtime = self.etc / "sai-runtime.profile"
        self.defaults = "SAI_INIT_CONFIG_FILE=config.xml\nSETTING=default\n"
        source = SOURCE.read_text()
        # Execute the real functions, redirecting their device paths into the fixture.
        self.functions = "\n".join(
            re.search(r"^" + name + r"\(\)\n\{.*?^\}", source, re.M | re.S).group()
            for name in ("apply_sai_profile_configdb", "config_syncd_bcm", "config_syncd_mlnx")
        )
        self.functions = re.sub(
            r"/usr/share/sonic|/var/run/sswsyncd|/etc/sai.d|/etc/mlnx|/usr/bin/asic_detect|/tmp|/proc|/dev",
            lambda match: str(self.root) + match.group(), self.functions,
        )

    def run_bash(self, body, rendered="", setup="", check=True):
        assignments = {
            "SAI_PROFILE_DIR": self.etc,
            "HWSKU_DIR": self.hwsku,
            "PLATFORM_DIR": self.platform,
            "TEMPLATES_DIR": self.templates,
            "SONIC_ASIC_SUBTYPE": "broadcom",
            "SYNCD_VARS": '{"mac":"00:11:22:33:44:55","dual_tor":"disable","dscp_remapping":"disable"}',
        }
        script = "\n".join(
            name + "=" + shlex.quote(str(value)) for name, value in assignments.items()
        )
        script += """
sonic-cfggen() { printf '%s' "$RENDERED"; }
logger() { :; }
mknod() { :; }
sync() { :; }
supervisorctl() { :; }
"""
        script += self.functions + "\n" + setup + "\n" + body
        environment = dict(os.environ, RENDERED=rendered)
        return subprocess.run(
            ["bash", "-c", script], env=environment, stdin=subprocess.DEVNULL,
            capture_output=True, text=True, check=check, timeout=10,
        )

    def initialize(self, rendered=""):
        result = self.run_bash(
            'config_syncd_bcm\nprintf "\\nSELECTED_ARGS=%s\\n" "$CMD_ARGS"', rendered,
        )
        args = result.stdout.split("SELECTED_ARGS=")[-1].strip().split()
        return pathlib.Path(args[args.index("-p") + 1])

    def test_absent_overrides_preserve_original_profile_priority(self):
        sources = (
            self.root / "tmp/sai.profile", self.etc / "sai.profile",
            self.hwsku / "sai.profile",
        )
        for source in sources:
            source.write_text(self.defaults)
        for selected in sources:
            with self.subTest(selected=selected):
                self.assertEqual(self.initialize(), selected)
                self.assertEqual(selected.read_text(), self.defaults)
                self.assertFalse(self.runtime.exists())
                selected.unlink()
        self.assertEqual(self.initialize(), sources[-1])
        self.assertFalse(self.runtime.exists())

    def test_restart_update_and_removal_leave_each_base_unchanged(self):
        for base in (
            self.etc / "sai.profile", self.hwsku / "sai.profile",
            self.root / "tmp/sai.profile",
        ):
            with self.subTest(base=base):
                base.write_text(self.defaults)
                for overrides in (
                    "SETTING=custom\nEXTRA=enabled",
                    "SETTING=custom\nEXTRA=enabled",
                    "SETTING=updated",
                ):
                    self.assertEqual(self.initialize(overrides), self.runtime)
                    self.assertEqual(self.runtime.read_text(), self.defaults + overrides + "\n")
                    self.assertEqual(base.read_text(), self.defaults)
                previous = self.runtime.read_bytes()
                self.assertEqual(self.initialize(), base)
                self.assertEqual(self.runtime.read_bytes(), previous)
                self.assertEqual(base.read_text(), self.defaults)
                self.assertEqual(self.initialize("EXTRA=new"), self.runtime)
                self.assertEqual(self.runtime.read_text(), self.defaults + "EXTRA=new\n")
                base.unlink()

    def test_no_static_profile_supports_override_only_file(self):
        self.assertEqual(self.initialize("EXTRA=enabled"), self.runtime)
        self.assertEqual(self.runtime.read_text(), "EXTRA=enabled\n")
        self.assertEqual(self.initialize("EXTRA=updated"), self.runtime)
        self.assertEqual(self.runtime.read_text(), "EXTRA=updated\n")
        self.assertEqual(self.initialize(), self.hwsku / "sai.profile")

    def test_missing_template_does_not_create_or_modify_runtime(self):
        base = self.etc / "sai.profile"
        base.write_text(self.defaults)
        self.template.unlink()
        self.assertEqual(self.initialize("SETTING=ignored"), base)
        self.assertFalse(self.runtime.exists())
        self.runtime.write_text("SETTING=old\n")
        self.assertEqual(self.initialize("SETTING=ignored"), base)
        self.assertEqual(self.runtime.read_text(), "SETTING=old\n")
        self.assertEqual(base.read_text(), self.defaults)

    def test_common_config_processing_is_preserved(self):
        for extension in ("bcm", "yml"):
            with self.subTest(extension=extension):
                config = self.hwsku / ("config." + extension)
                config.write_text("")
                base = self.etc / "sai.profile"
                original = "SAI_INIT_CONFIG_FILE=" + str(config) + "\nSETTING=default\n"
                base.write_text(original)
                (self.platform / "common_config_support").touch()
                (self.root / "proc/linux-kernel-bde").write_text("0x14e4:0xb850\n")
                (self.root / "proc/linux_ngbde").write_text("0:14e4:b850\n")
                adjusted = original.replace(str(self.hwsku), str(self.root / "tmp"))
                for overrides in ("SETTING=custom", "SETTING=custom", "SETTING=updated"):
                    self.assertEqual(self.initialize(overrides), self.runtime)
                    self.assertEqual(self.runtime.read_text(), adjusted + overrides + "\n")
                self.assertEqual(self.initialize(), self.root / "tmp/sai.profile")
                self.assertEqual((self.root / "tmp/sai.profile").read_text(), adjusted)
                self.assertEqual(base.read_text(), original)
                self.assertEqual(
                    (self.root / "var/run/sswsyncd/sai.profile").read_text(), adjusted,
                )

    def test_one_argument_helper_preserves_nvidia_append_behavior(self):
        base = self.root / "tmp/sai.profile"
        base.write_text(self.defaults)
        command = "apply_sai_profile_configdb " + shlex.quote(str(base))
        self.assertEqual(self.run_bash(command, "SETTING=custom").stdout, "")
        self.assertEqual(base.read_text(), self.defaults + "SETTING=custom\n")
        before = base.read_bytes()
        self.assertEqual(self.run_bash(command).stdout, "")
        self.assertEqual(base.read_bytes(), before)
        base.unlink()
        self.run_bash(command, "EXTRA=enabled")
        self.assertEqual(base.read_text(), "EXTRA=enabled\n")

    def test_destination_preparation_failure_stops_broadcom_startup(self):
        (self.etc / "sai.profile").write_text(self.defaults)
        for setup in ("mkdir() { return 1; }", "cp() { return 1; }"):
            with self.subTest(setup=setup):
                result = self.run_bash(
                    "config_syncd_bcm\nprintf SHOULD_NOT_START", "SETTING=custom",
                    setup=setup, check=False,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("SHOULD_NOT_START", result.stdout)
                self.assertEqual((self.etc / "sai.profile").read_text(), self.defaults)

    def test_profile_without_final_newline_gets_separate_override_line(self):
        base = self.etc / "sai.profile"
        original = "SAI_INIT_CONFIG_FILE=config.xml"
        base.write_text(original)
        self.assertEqual(self.initialize("EXTRA=value"), self.runtime)
        self.assertEqual(self.runtime.read_text(), original + "\nEXTRA=value\n")
        self.assertEqual(base.read_text(), original)
        command = "apply_sai_profile_configdb " + shlex.quote(str(base))
        self.run_bash(command, "EXTRA=value")
        self.assertEqual(base.read_text(), original + "\nEXTRA=value\n")

    def test_render_failures_stop_both_vendor_paths_without_partial_overrides(self):
        for output in ("", "PARTIAL=bad"):
            for vendor in ("bcm", "mlnx"):
                with self.subTest(output=output, vendor=vendor):
                    (self.etc / "sai.profile").write_text(self.defaults)
                    (self.hwsku / "sai.profile").write_text(self.defaults)
                    self.runtime.write_text("UNCHANGED=yes\n")
                    result = self.run_bash(
                        "config_syncd_" + vendor + "\nprintf SHOULD_NOT_START",
                        setup="sonic-cfggen() { printf '%s' " + shlex.quote(output) + "; return 7; }",
                        check=False,
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertNotIn("SHOULD_NOT_START", result.stdout)
                    self.assertEqual(self.runtime.read_text(), "UNCHANGED=yes\n")
                    profile = self.root / "tmp/sai.profile"
                    self.assertNotIn("PARTIAL=bad", profile.read_text() if profile.exists() else "")

    def test_nvidia_dump_path_and_retention_use_final_overrides(self):
        original_path = self.root / "original-dumps"
        final_path = self.root / "final-dumps"
        base = self.hwsku / "sai.profile"
        base.write_text(
            self.defaults + "SAI_DUMP_STORE_PATH=" + str(original_path)
            + "\nSAI_DUMP_STORE_AMOUNT=5\n"
        )
        for overrides, expected_path, amount in (
            ("SAI_DUMP_STORE_PATH=" + str(final_path) + "\nSAI_DUMP_STORE_AMOUNT=3", final_path, 3),
            ("", original_path, 5),
        ):
            with self.subTest(overrides=overrides):
                self.run_bash("config_syncd_mlnx", overrides)
                profile = self.root / "tmp/sai.profile"
                lines = profile.read_text().splitlines()
                self.assertEqual(
                    [line for line in lines if line.startswith("SAI_DUMP_STORE_AMOUNT=")],
                    ["SAI_DUMP_STORE_AMOUNT=" + str(amount)],
                )
                self.assertEqual(
                    [line for line in lines if line.startswith("SAI_DUMP_STORE_PATH=")],
                    ["SAI_DUMP_STORE_PATH=" + str(expected_path)],
                )
                self.assertTrue(expected_path.is_dir())
                result = self.run_bash(
                    'SAI_MAX_FAILURE_DUMPS=$(grep SAI_DUMP_STORE_AMOUNT '
                    + shlex.quote(str(profile))
                    + ' | cut -d "=" -f2)\nprintf "%s" "$((SAI_MAX_FAILURE_DUMPS+1))"'
                )
                self.assertEqual(result.stdout, str(amount + 1))
                self.assertIn("-p " + str(profile), self.run_bash(
                    'config_syncd_mlnx\nprintf "%s" "$CMD_ARGS"', overrides,
                ).stdout)

    def test_nvidia_extra_profile_without_final_newline_preserves_adjacent_settings(self):
        (self.hwsku / "sai.profile").write_text(self.defaults)
        (self.root / "tmp/sai_extra.profile").write_text("EXTRA=value")
        for missing_template in (False, True):
            with self.subTest(missing_template=missing_template):
                if missing_template:
                    self.template.unlink()
                self.run_bash("config_syncd_mlnx")
                lines = (self.root / "tmp/sai.profile").read_text().splitlines()
                self.assertIn("EXTRA=value", lines)
                self.assertIn("SAI_WARM_BOOT_WRITE_FILE=/var/warmboot/", lines)


if __name__ == "__main__":
    unittest.main(verbosity=2)
