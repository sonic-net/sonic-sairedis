import pathlib
import re
import shlex
import subprocess
import tempfile
import unittest


class BroadcomSaiProfileTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = pathlib.Path(self.temp_dir.name)
        self.etc = self.root / "etc/sai.d"
        self.hwsku = self.root / "usr/share/sonic/hwsku"
        self.platform = self.root / "usr/share/sonic/platform"
        self.templates = self.root / "usr/share/sonic/templates"
        for directory in (
            self.etc, self.hwsku, self.platform, self.templates,
            self.root / "tmp", self.root / "proc",
            self.root / "var/run/sswsyncd",
        ):
            directory.mkdir(parents=True, exist_ok=True)
        self.template = self.templates / "sai_profile_dynamic.j2"
        self.template.touch()
        self.overrides = self.root / "overrides"
        self.runtime = self.etc / "sai-runtime.profile"
        source = pathlib.Path(__file__).with_name("syncd_init_common.sh").read_text()
        self.functions = "\n".join(
            re.search(r"^" + name + r"\(\)\n\{.*?^\}", source, re.M | re.S).group()
            for name in ("apply_sai_profile_configdb", "config_syncd_bcm")
        )
        # Redirect the container's absolute paths into an isolated fixture.
        self.functions = re.sub(
            r"/usr/share/sonic|/var/run/sswsyncd|/tmp|/proc",
            lambda match: str(self.root) + match.group(), self.functions,
        )

    def initialize(self, overrides):
        self.overrides.write_text(overrides)
        assignments = {
            "SAI_PROFILE_DIR": self.etc,
            "HWSKU_DIR": self.hwsku,
            "PLATFORM_DIR": self.platform,
            "TEMPLATES_DIR": self.templates,
            "OVERRIDES": self.overrides,
            "SONIC_ASIC_SUBTYPE": "broadcom",
        }
        script = "\n".join(
            name + "=" + shlex.quote(str(value))
            for name, value in assignments.items()
        )
        script += """
sonic-cfggen() { cat "$OVERRIDES"; }
logger() { :; }
mknod() { :; }
sync() { :; }
"""
        script += self.functions + '\nconfig_syncd_bcm\nprintf "%s\\n" "$CMD_ARGS"\n'
        result = subprocess.run(
            ["bash", "-c", script], stdin=subprocess.DEVNULL,
            capture_output=True, text=True, check=True,
        )
        self.assertIn("-p " + str(self.runtime), result.stdout)
        return self.runtime.read_text()

    def test_restart_rebuilds_base_and_removes_deleted_overrides(self):
        for use_etc in (True, False):
            with self.subTest(use_etc=use_etc):
                base = (self.etc if use_etc else self.hwsku) / "sai.profile"
                original = "SAI_INIT_CONFIG_FILE=config.xml\nSETTING=default\n"
                base.write_text(original)
                (self.root / "tmp/sai.profile").write_text("SETTING=stale\n")
                self.runtime.write_text("SETTING=stale-runtime\n")
                first = self.initialize("SETTING=custom\nEXTRA=enabled\n")
                self.assertEqual(first, original + "SETTING=custom\nEXTRA=enabled\n")
                self.assertEqual(self.initialize("SETTING=custom\nEXTRA=enabled\n"), first)
                self.assertEqual(self.initialize("SETTING=updated\n"), original + "SETTING=updated\n")
                self.assertEqual(self.initialize(""), original)
                self.assertEqual(base.read_text(), original)
                base.unlink()

    def test_no_static_profile_rebuilds_from_overrides_only(self):
        self.assertEqual(self.initialize("EXTRA=enabled\n"), "EXTRA=enabled\n")
        self.assertEqual(self.initialize(""), "")

    def test_missing_template_clears_previous_overrides(self):
        base = self.etc / "sai.profile"
        base.write_text("SAI_INIT_CONFIG_FILE=config.xml\nSETTING=default\n")
        self.initialize("SETTING=custom\n")
        self.template.unlink()
        self.assertEqual(self.initialize("SETTING=custom\n"), base.read_text())

    def test_common_config_preserves_runtime_path_adjustments(self):
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
                common = self.root / "usr/share/sonic/device/x86_64-broadcom_common/x86_64-broadcom_b85"
                common.mkdir(parents=True, exist_ok=True)
                adjusted = original.replace(str(self.hwsku), str(self.root / "tmp"))
                first = self.initialize("SETTING=custom\n")
                self.assertEqual(first, adjusted + "SETTING=custom\n")
                self.assertEqual(self.initialize("SETTING=custom\n"), first)
                self.assertEqual(self.initialize(""), adjusted)
                self.assertEqual(base.read_text(), original)
                self.assertFalse((self.root / "tmp/sai.profile").exists())
                self.assertEqual(
                    (self.root / "var/run/sswsyncd/sai.profile").read_text(), adjusted,
                )


if __name__ == "__main__":
    unittest.main()
