#!/usr/bin/env python3
"""Verify already-built sairedis tar outputs and detached debug symbols."""

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import posixpath
import re
import shutil
import stat
import struct
import subprocess
import tarfile
import tempfile
import zlib


METADATA_FIELDS = ("mode", "uid", "gid")
SETTING_VALUES = {
    "//:sai_backend": "vs",
    "//:asic_platform": "generic",
    "//:enable_debug": False,
    "//:enable_coverage": False,
    "//:enable_asan": False,
    "//:enable_vpp": False,
    "//:enable_rpcserver": False,
    "//:enable_dashsai": False,
    "@sonic_swss_common//tools/bazel:yang_modules": True,
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def write_json(path, value):
    path = Path(path)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def stable_file(path):
    path = Path(path)
    require(path.is_file(), "expected a regular file: " + str(path))
    before = path.stat()
    digest = sha256(path)
    after = path.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), "file changed while hashing: " + str(path))
    return {"sha256": digest, "size": after.st_size}


def run_command(command, cwd, log=None):
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if log is not None:
        with Path(log).open("ab") as stream:
            stream.write((json.dumps({"command": command, "return_code": result.returncode}) + "\n").encode())
            stream.write(result.stderr)
            if result.stderr and not result.stderr.endswith(b"\n"):
                stream.write(b"\n")
    require(result.returncode == 0, "command failed: " + command[0] + " " + (command[1] if len(command) > 1 else ""))
    return result.stdout


def normalize_label(value):
    if value.startswith("@@//"):
        return value[2:]
    if value.startswith("@//"):
        return value[1:]
    if value.startswith("@@sonic-swss-common+//"):
        return "@sonic_swss_common//" + value.split("//", 1)[1]
    if value.startswith("@@sonic-build-infra+//"):
        return "@sonic_build_infra//" + value.split("//", 1)[1]
    return value


def canonical_archive_name(name, allow_trailing_slash=False):
    require(isinstance(name, str) and not name.startswith("/") and "\0" not in name, "invalid absolute or NUL archive path")
    while name.startswith("./"):
        name = name[2:]
    require(not name.endswith("/") or allow_trailing_slash, "non-directory archive path has a trailing slash")
    name = name.rstrip("/")
    if name in ("", "."):
        return "."
    require(all(part not in ("", ".", "..") for part in name.split("/")), "non-canonical archive path")
    return name


def safe_link(name, target):
    require(target and not target.startswith("/") and "\0" not in target, "invalid archive symlink target")
    resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), target))
    require(resolved != ".." and not resolved.startswith("../"), "archive symlink escapes its root")


class Audit:
    def __init__(self):
        self.checks = []
        self.metadata_observations = []
        self.archives = []
        self.pairs = []
        self.gdb = None

    def check(self, name, passed, **details):
        self.checks.append({"name": name, "passed": bool(passed), **details})
        return bool(passed)

    def failure(self, name, error, **details):
        self.check(name, False, error={"type": type(error).__name__, "message": str(error)}, **details)

    def observe_metadata(self, label, item, reference, classification, occurrence):
        actual = {field: item[field] for field in METADATA_FIELDS}
        comparisons = {}
        for name, value in reference.items():
            if not isinstance(value, dict):
                continue
            expected = {field: value[field] for field in METADATA_FIELDS if field in value}
            if not expected:
                continue
            comparison = {"reference": expected}
            if name == "inherited_generated_mtree":
                comparison["applicability"] = "not established for this individual directory member"
            else:
                comparison["matches"] = all(actual[field] == expected[field] for field in expected)
            comparisons[name] = comparison
        self.metadata_observations.append({
            "label": label, "path": item["path"], "occurrence": occurrence,
            "classification": classification, "actual": actual, "comparisons": comparisons,
            "gates_pass_fail": False,
        })

    def check_entry(self, label, records, expected):
        for occurrence, item in enumerate(records):
            self.check("entry_type", item["type"] == expected["type"], label=label, path=item["path"], occurrence=occurrence, actual=item["type"], expected=expected["type"])
            for field, value in expected.get("required_metadata", {}).items():
                self.check("explicit_entry_" + field, item[field] == value, label=label, path=item["path"], occurrence=occurrence, actual=item[field], expected=value)
            if expected["type"] == "symlink":
                self.check("symlink_target", item["linkname"] == expected["linkname"], label=label, path=item["path"], occurrence=occurrence, actual=item["linkname"], expected=expected["linkname"])
            evidence = expected.get("metadata_evidence", {})
            self.observe_metadata(label, item, expected.get("observation_reference", {}), evidence.get("classification", "explicit_or_inherited_as_recorded"), occurrence)

    def inventory(self, archive, expected, directory_contract):
        label = archive["label"]
        wanted = {entry["path"]: entry for entry in expected}
        actual = archive["non_directories"]
        self.check("payload_paths", set(actual) == set(wanted), label=label, missing=sorted(set(wanted) - set(actual)), extra=sorted(set(actual) - set(wanted)))
        for path in sorted(set(actual) & set(wanted)):
            self.check_entry(label, actual[path], wanted[path])
        allowed = {"."} if directory_contract["allow_normalized_root"] else set()
        for path in wanted:
            parent = PurePosixPath(path).parent
            while str(parent) != ".":
                allowed.add(str(parent))
                parent = parent.parent
        for path, records in archive["directories"].items():
            self.check("directory_is_ancestor", path in allowed, label=label, path=path)
            for occurrence, item in enumerate(records):
                self.observe_metadata(label, item, directory_contract.get("observation_reference", {}), "inherited_directory_metadata", occurrence)


def inspect_archive(spec, destination, audit):
    path = Path(spec["path"])
    require(stable_file(path) == {key: spec[key] for key in ("sha256", "size")}, "archive differs from cquery output evidence")
    destination.mkdir(parents=True)
    entries, non_directories, directories = [], defaultdict(list), defaultdict(list)
    signatures = {}
    with tarfile.open(path, "r:*") as archive:
        for number, member in enumerate(archive):
            kind = "directory" if member.isdir() else "file" if member.isfile() else "symlink" if member.issym() else None
            require(kind is not None, "unsupported archive entry type at " + member.name)
            name = canonical_archive_name(member.name, allow_trailing_slash=kind == "directory")
            require(name != "." or kind == "directory", "non-directory archive root")
            if kind == "symlink":
                safe_link(name, member.linkname)
            item = {
                "path": name, "type": kind, "mode": format(member.mode & 0o7777, "04o"),
                "uid": member.uid, "gid": member.gid, "linkname": member.linkname,
                "size": member.size, "mtime": member.mtime,
            }
            if kind == "file":
                payload = destination / ("payload-" + str(number))
                digest, crc, size = hashlib.sha256(), 0, 0
                source = archive.extractfile(member)
                require(source is not None, "cannot read archive payload at " + name)
                descriptor = os.open(payload, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                with source, os.fdopen(descriptor, "wb") as target:
                    for block in iter(lambda: source.read(1024 * 1024), b""):
                        target.write(block)
                        digest.update(block)
                        crc = zlib.crc32(block, crc)
                        size += len(block)
                require(size == member.size, "archive payload size differs from header at " + name)
                item.update({"sha256": digest.hexdigest(), "crc32": crc & 0xFFFFFFFF, "_payload": payload})
            signature = (kind, item.get("sha256"), item["size"]) if kind == "file" else (kind, item["linkname"]) if kind == "symlink" else (kind,)
            if name in signatures:
                audit.check("duplicate_content_consistent", signatures[name] == signature, label=spec["label"], path=name)
            else:
                signatures[name] = signature
            (directories if kind == "directory" else non_directories)[name].append(item)
            entries.append(item)
    require(stable_file(path) == {key: spec[key] for key in ("sha256", "size")}, "archive changed during inspection")
    result = {"label": spec["label"], "entries": entries, "non_directories": dict(non_directories), "directories": dict(directories)}
    audit.archives.append({
        "label": spec["label"], "exec_path": spec["exec_path"], "sha256": spec["sha256"], "size": spec["size"],
        "non_directory_paths": sorted(non_directories), "directory_count": sum(len(rows) for rows in directories.values()),
        "duplicate_path_count": sum(len(rows) > 1 for rows in list(non_directories.values()) + list(directories.values())),
    })
    return result


class Elf:
    """Read ELF identity and detached-debug fields without loading the ELF."""

    def __init__(self, path):
        self.path = Path(path)
        self.size = self.path.stat().st_size
        self.stream = self.path.open("rb")
        ident = self.read(0, 16)
        require(ident[:4] == b"\x7fELF" and ident[4] in (1, 2) and ident[5] in (1, 2), "unsupported ELF identification")
        self.bits = {1: 32, 2: 64}[ident[4]]
        self.endian = {1: "<", 2: ">"}[ident[5]]
        self.endianness = {1: "little", 2: "big"}[ident[5]]
        header_format = self.endian + ("HHIQQQIHHHHHH" if self.bits == 64 else "HHIIIIIHHHHHH")
        header = struct.unpack(header_format, self.read(16, struct.calcsize(header_format)))
        self.elf_type, self.machine = header[0], header[1]
        section_offset, section_size, section_count, names_index = header[5], header[10], header[11], header[12]
        section_format = self.endian + ("IIQQQQIIQQ" if self.bits == 64 else "IIIIIIIIII")
        expected_size = struct.calcsize(section_format)
        require(section_size >= expected_size and section_offset, "missing or invalid ELF section table")
        first = struct.unpack(section_format, self.read(section_offset, expected_size))
        if section_count == 0:
            section_count = first[5]
        if names_index == 0xFFFF:
            names_index = first[6]
        require(0 < section_count < 100000 and names_index < section_count, "invalid ELF section count or names index")
        self.sections = []
        for number in range(section_count):
            values = struct.unpack(section_format, self.read(section_offset + number * section_size, expected_size))
            section = dict(zip(("name_offset", "type", "flags", "address", "offset", "size", "link", "info", "alignment", "entry_size"), values))
            require(section["type"] == 8 or section["offset"] + section["size"] <= self.size, "ELF section extends beyond file")
            self.sections.append(section)
        names = self.section_data(self.sections[names_index])
        for section in self.sections:
            section["name"] = self.string(names, section["name_offset"])
        self.by_name = {section["name"]: section for section in self.sections}

    def close(self):
        self.stream.close()

    def read(self, offset, size):
        require(offset >= 0 and size >= 0 and offset + size <= self.size, "ELF read extends beyond file")
        self.stream.seek(offset)
        data = self.stream.read(size)
        require(len(data) == size, "short ELF read")
        return data

    def section_data(self, section):
        require(section["type"] != 8, "cannot read ELF NOBITS bytes")
        return self.read(section["offset"], section["size"])

    @staticmethod
    def string(data, offset):
        require(offset < len(data), "invalid ELF string offset")
        end = data.find(b"\0", offset)
        require(end >= 0, "unterminated ELF string")
        return data[offset:end].decode("utf-8", "strict")

    def build_id(self):
        section = self.by_name.get(".note.gnu.build-id")
        if section is None or section["type"] == 8:
            return None
        data, offset, ids = self.section_data(section), 0, []
        while offset + 12 <= len(data):
            namesz, descsz, note_type = struct.unpack_from(self.endian + "III", data, offset)
            offset += 12
            name = data[offset:offset + namesz]
            offset = (offset + namesz + 3) & ~3
            desc = data[offset:offset + descsz]
            offset = (offset + descsz + 3) & ~3
            require(len(desc) == descsz, "truncated ELF note")
            if name.rstrip(b"\0") == b"GNU" and note_type == 3:
                ids.append(desc.hex())
        require(len(ids) == 1, "expected exactly one GNU build ID")
        return ids[0]

    def dynamic(self):
        section = self.by_name.get(".dynamic")
        result = {"soname": None, "needed": [], "rpath": [], "runpath": []}
        if section is None or section["type"] == 8:
            return result
        require(section["link"] < len(self.sections), "invalid dynamic string-table link")
        strings = self.section_data(self.sections[section["link"]])
        entry_format = self.endian + ("qQ" if self.bits == 64 else "iI")
        entry_size = section["entry_size"] or struct.calcsize(entry_format)
        data = self.section_data(section)
        require(entry_size >= struct.calcsize(entry_format) and len(data) % entry_size == 0, "invalid ELF dynamic entry size")
        names = []
        for offset in range(0, len(data), entry_size):
            tag, value = struct.unpack_from(entry_format, data, offset)
            if tag == 14:
                names.append(self.string(strings, value))
            elif tag in (1, 15, 29):
                result[{1: "needed", 15: "rpath", 29: "runpath"}[tag]].append(self.string(strings, value))
        require(len(names) <= 1, "multiple ELF SONAME entries")
        result["soname"] = names[0] if names else None
        return result

    def debuglink(self):
        section = self.by_name.get(".gnu_debuglink")
        if section is None or section["type"] == 8:
            return None
        data = self.section_data(section)
        end = data.find(b"\0")
        require(end >= 0, "unterminated GNU debuglink")
        crc_offset = (end + 4) & ~3
        require(crc_offset + 4 <= len(data), "truncated GNU debuglink CRC")
        return {"filename": data[:end].decode("utf-8", "strict"), "crc32": struct.unpack_from(self.endian + "I", data, crc_offset)[0]}

    def summary(self):
        return {
            "class": self.bits, "machine": self.machine, "endianness": self.endianness, "type": self.elf_type,
            **self.dynamic(), "build_id": self.build_id(), "debuglink": self.debuglink(),
            "debug_sections": sorted(name for name in self.by_name if name.startswith((".debug_", ".zdebug_"))),
            "debug_section_details": {
                name: {"type": section["type"], "size": section["size"], "file_backed": section["type"] != 8}
                for name, section in self.by_name.items() if name.startswith((".debug_", ".zdebug_"))
            },
        }


def inspect_elf(path):
    elf = Elf(path)
    try:
        return elf.summary()
    finally:
        elf.close()


def validate_contract(contract, required, architecture):
    require(contract.get("schema_version") == 1 and required.get("schema_version") == 1, "unsupported contract schema")
    require(architecture in contract["architectures"], "architecture is absent from package contract")
    require(contract["architectures"][architecture].get("elf_endianness") == "little", "supported package architectures require little-endian ELF")
    policy = contract["metadata_policy"]
    require(policy.get("observation_reference_gates_pass_fail") is False, "observational metadata must not gate package status")
    require(contract["debug_contract"].get("required_metadata") == {}, "debug metadata is inherited and must remain observational")
    require(set(contract["debug_contract"].get("matching_elf_fields", [])) == {"class", "machine", "endianness", "build_id"}, "unsupported detached ELF matching fields")
    require(contract["debug_contract"].get("required_sections") == [".debug_info", ".debug_line"], "unsupported detached debug section contract")
    require(contract["debug_contract"].get("required_sections_file_backed_nonempty") is True, "detached debug sections must be file-backed and nonempty")
    require(contract["debug_contract"].get("runtime_sections_absent") == [".debug_info", ".debug_line"], "unsupported runtime debug section contract")
    require(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", contract["debug_contract"]["gdb_lookup"]["symbol"]), "invalid GDB symbol contract")
    require(contract["directory_contract"].get("required_metadata") == {}, "directory metadata must remain observational")
    require(contract["directory_contract"].get("path_policy") == "optional_ancestors_only", "unsupported directory path policy")
    labels, aliases, elf_labels = [], {}, []
    keys = set()
    for package in contract["packages"]:
        require(package["key"] not in keys, "duplicate package key")
        keys.add(package["key"])
        labels.extend((package["runtime_label"], package["debug_label"]))
        if package.get("dev_label"):
            labels.extend((package["dev_label"], package["dev_debug_alias"]))
            aliases[package["dev_debug_alias"]] = package["dev_debug_alias_target"]
        for group in ("runtime_entries", "dev_additions"):
            paths = set()
            for entry in package.get(group, []):
                path = entry["path"].format(**contract["architectures"][architecture])
                require(canonical_archive_name(path) == path and path != ".", "invalid contract payload path")
                require(path not in paths, "duplicate contract payload path")
                paths.add(path)
                require(entry["type"] in ("file", "symlink"), "unsupported contract payload type")
                metadata = entry.get("required_metadata", {})
                classification = entry.get("metadata_evidence", {}).get("classification")
                require(classification in ("literal_explicit_component_mtree", "inherited_generated_mtree"), "unknown component metadata classification")
                if classification == "inherited_generated_mtree":
                    require(metadata == {}, "inherited component metadata must remain observational")
                require(set(metadata) <= set(METADATA_FIELDS), "unknown required metadata field")
                if "mode" in metadata:
                    require(re.fullmatch(r"[0-7]{4}", metadata["mode"]), "invalid required mode")
                for field in ("uid", "gid"):
                    if field in metadata:
                        require(isinstance(metadata[field], int) and metadata[field] >= 0, "invalid required ownership")
                if entry["type"] == "symlink":
                    safe_link(path, entry["linkname"])
                if entry["role"] == "elf":
                    require(group == "runtime_entries" and entry["type"] == "file", "ELF contract entry is outside runtime files")
                    elf_labels.append(entry["label"])
    require(len(labels) == len(set(labels)) == policy["required_public_label_count"] == 16, "contract does not contain 16 distinct public labels")
    require(len(set(labels) - set(aliases)) == policy["required_unique_tar_count"] == 13, "contract does not contain 13 unique tar labels")
    require(len(elf_labels) == len(set(elf_labels)) == policy["required_elf_pair_count"] == 14, "contract does not contain 14 distinct ELF labels")
    require(set(required["package_build"]) == set(labels) and len(required["package_build"]) == 16, "required target list differs from package contract")
    require(required["debug_aliases"] == aliases, "required debug aliases differ from package contract")
    return required["package_build"], aliases


FILES_FORMATTER = '''def format(target):
    options = build_options(target)
    info = providers(target).get("DefaultInfo")
    if options == None or info == None:
        return json.encode({"label": str(target.label), "error": "missing configuration or DefaultInfo"})
    return json.encode({
        "label": str(target.label),
        "files": [value.path for value in info.files.to_list()],
        "native_selection": {
            "platforms": [str(value) for value in options.get("//command_line_option:platforms", [])],
            "host_platform": str(options.get("//command_line_option:host_platform")),
            "compilation_mode": str(options.get("//command_line_option:compilation_mode")),
        },
    })
'''


SETTINGS_FORMATTER = '''def format(target):
    key = "@@bazel_skylib+//rules:common_settings.bzl%BuildSettingInfo"
    info = providers(target).get(key)
    return json.encode({"label": str(target.label), "provider_key": key, "values": [info.value] if info != None else []})
'''


def resolve_outputs(args, repo, temporary, validation, labels, aliases, architecture, audit):
    bazel = args.bazel
    common = ["--lockfile_mode=update"] + (["--config=" + args.bazel_config] if args.bazel_config else [])
    log = validation / "cquery.log"
    version = run_command([bazel, "--version"], repo, log).decode().strip()
    # Execution-root discovery does not need component target selection.
    info_only_overrides = ["--platforms=", "--host_platform=@bazel_tools//tools:host_platform"]
    execution_root_text = run_command([bazel, "info", "execution_root", *common, *info_only_overrides], repo, log).decode().strip()
    execution_root = Path(execution_root_text)
    require(execution_root.is_absolute() and execution_root.is_dir(), "Bazel execution root is not an existing absolute directory")
    files_formatter = temporary / "package-files.cquery"
    files_formatter.write_text(FILES_FORMATTER)
    expected_platform = "@sonic_build_infra//platforms:" + architecture["native_machine"] + "_trixie"
    expected_native = {"platforms": [expected_platform], "host_platform": expected_platform, "compilation_mode": "fastbuild"}
    outputs, native_selections = {}, {}
    for label in labels:
        command = [bazel, "cquery", "config(" + label + ", target)", *common, "--output=starlark", "--starlark:file=" + str(files_formatter)]
        lines = [line for line in run_command(command, repo, log).decode().splitlines() if line.strip()]
        require(len(lines) == 1, "cquery did not return one configured target for " + label)
        row = json.loads(lines[0])
        require("error" not in row and len(row.get("files", [])) == 1, "cquery did not return one output for " + label)
        native = row["native_selection"]
        native["platforms"] = [normalize_label(value) for value in native["platforms"]]
        native["host_platform"] = normalize_label(native["host_platform"])
        native_selections[label] = native
        audit.check("native_package_selection", native == expected_native, label=label, actual=native, expected=expected_native)
        relative = row["files"][0]
        require(isinstance(relative, str) and relative.startswith("bazel-out/") and relative.endswith(".tar"), "cquery output is not a generated tar for " + label)
        require(canonical_archive_name(relative) == relative, "cquery output path is not canonical")
        path = execution_root / relative
        real = path.resolve(strict=True)
        require(real.is_relative_to(execution_root.resolve()) and real.is_file(), "cquery tar resolves outside the execution root")
        configured_label = normalize_label(row["label"])
        outputs[label] = {
            "label": label, "configured_label": configured_label, "exec_path": relative,
            "path": str(path), "real_path": str(real), **stable_file(path),
        }
    for alias, actual in aliases.items():
        fields = ("exec_path", "sha256", "size")
        audit.check("debug_alias_output", all(outputs[alias][field] == outputs[actual][field] for field in fields), label=alias, actual_label=actual)
    audit.check("unique_tar_count", len({item["exec_path"] for item in outputs.values()}) == 13, actual=len({item["exec_path"] for item in outputs.values()}), expected=13)
    settings_formatter = temporary / "package-settings.cquery"
    settings_formatter.write_text(SETTINGS_FORMATTER)
    setting_query = "config(set(" + " ".join(SETTING_VALUES) + "), target)"
    setting_lines = run_command([bazel, "cquery", setting_query, *common, "--output=starlark", "--starlark:file=" + str(settings_formatter)], repo, log).decode().splitlines()
    settings = {}
    for line in setting_lines:
        if not line.strip():
            continue
        row = json.loads(line)
        label = normalize_label(row["label"])
        require(row.get("provider_key") == "@@bazel_skylib+//rules:common_settings.bzl%BuildSettingInfo", "setting cquery provider key differs from the pinned source")
        require(label not in settings and len(row.get("values", [])) == 1, "setting cquery did not identify one BuildSettingInfo value")
        settings[label] = row["values"][0]
    setting_match = set(settings) == set(SETTING_VALUES) and all(type(settings[label]) is type(value) and settings[label] == value for label, value in SETTING_VALUES.items())
    audit.check("effective_component_settings", setting_match, actual=settings, expected=SETTING_VALUES)
    config_query = "config(set(" + " ".join(labels + list(SETTING_VALUES)) + "), target)"
    config_lines = run_command([bazel, "cquery", config_query, *common, "--output=label"], repo, log).decode().splitlines()
    configurations = {}
    for line in config_lines:
        if not line.strip():
            continue
        match = re.fullmatch(r"(.+?) \(([0-9a-f]{7,64})\)", line.strip())
        require(match is not None, "unexpected cquery configuration-label output")
        label = normalize_label(match.group(1))
        require(label not in configurations, "configured package label appeared more than once")
        configurations[label] = match.group(2)
    expected_configured_labels = set(labels) | set(settings)
    audit.check("configured_package_and_setting_labels", set(configurations) == expected_configured_labels, actual=sorted(configurations), expected=sorted(expected_configured_labels))
    # Alias wrappers can have different configurations from their resolved
    # targets. Their output equality and native selection are checked above.
    non_alias_labels = (set(labels) - set(aliases)) | set(settings)
    non_alias_configurations = {configurations[label] for label in non_alias_labels if label in configurations}
    audit.check("one_non_alias_package_and_setting_configuration", non_alias_labels.issubset(configurations) and len(non_alias_configurations) == 1, actual=sorted(non_alias_configurations), expected_count=1, alias_labels_recorded_separately=sorted(aliases))
    selection = {"native_by_label": native_selections, "expected_native": expected_native, "component_settings": settings, "bazel_config": args.bazel_config}
    selection_hash = hashlib.sha256(json.dumps(selection, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return outputs, {
        "executable": bazel, "version": version, "execution_root": str(execution_root),
        "execution_root_info_only_overrides": info_only_overrides,
        "configuration_hash_prefixes_from_cquery": configurations,
        "known_selection": selection, "known_selection_sha256": selection_hash,
        "configuration_identity_limit": "cquery emits configuration hash prefixes; this verifier records the exact emitted prefixes and a digest of the checked selection fields. Alias wrapper prefixes are recorded separately from the shared non-alias package and setting configuration.",
    }


def source_snapshot(repo, output_dir):
    head = run_command(["git", "rev-parse", "HEAD"], repo).decode().strip()
    diff = run_command(["git", "diff", "--binary", "--no-ext-diff", "--no-textconv", "HEAD", "--"], repo)
    excluded = None
    if output_dir.is_relative_to(repo):
        excluded = output_dir.relative_to(repo).as_posix()
        tracked_outputs = run_command(["git", "ls-files", "-z", "--", excluded], repo)
        require(not tracked_outputs, "artifact output directory contains tracked source files")
    untracked = []
    for value in run_command(["git", "ls-files", "--others", "--exclude-standard", "-z"], repo).split(b"\0"):
        if not value:
            continue
        relative = value.decode("utf-8", "strict")
        path = repo / relative
        if path.is_relative_to(output_dir):
            continue
        info = path.lstat()
        if stat.S_ISREG(info.st_mode):
            item = {"path": relative, "type": "file", **stable_file(path)}
        elif stat.S_ISLNK(info.st_mode):
            target = os.readlink(path)
            item = {"path": relative, "type": "symlink", "target": target, "sha256": hashlib.sha256(target.encode()).hexdigest(), "size": len(target.encode())}
        else:
            raise ValueError("unsupported untracked source type: " + relative)
        item["mode"] = format(stat.S_IMODE(info.st_mode), "04o")
        untracked.append(item)
    gitlink_text = run_command(["git", "ls-files", "--stage", "--", "SAI"], repo).decode().strip()
    match = re.fullmatch(r"160000 ([0-9a-f]{40}) 0\tSAI", gitlink_text)
    require(match is not None, "SAI gitlink is missing or unmerged")
    return {
        "head": head, "tracked_diff": {"sha256": hashlib.sha256(diff).hexdigest(), "size": len(diff)},
        "untracked": sorted(untracked, key=lambda item: item["path"]), "sai_gitlink": match.group(1),
        "excluded_artifact_directory": excluded,
    }


def declaration_provenance(repo):
    files = [".bazelversion", ".bazelrc", "MODULE.bazel"]
    hashes = {name: stable_file(repo / name) for name in files}
    module = (repo / "MODULE.bazel").read_text()
    dependencies = {}
    for block in re.findall(r"bazel_dep\((.*?)\)", module, re.S):
        name = re.search(r'\bname\s*=\s*"([^"]+)"', block)
        version = re.search(r'\bversion\s*=\s*"([^"]+)"', block)
        if name and version:
            dependencies[name.group(1)] = version.group(1)
    wanted = ("sai", "sonic-build-infra", "sonic-swss-common", "swig", "rules_python", "rules_cc", "rules_distroless", "tar.bzl")
    require(all(name in dependencies for name in wanted), "MODULE.bazel lacks a required dependency declaration")
    return {
        "files": hashes,
        "bazel_dependency_versions": {name: dependencies[name] for name in wanted},
    }


def resolved_dependency_provenance(args, repo, validation, execution_root, declarations):
    """Check actual fetched modules against declarations and registry source pins."""
    common = ["--lockfile_mode=update"] + (["--config=" + args.bazel_config] if args.bazel_config else [])
    targets = {
        "sai": "@sai_source//:headers",
        "sonic-swss-common": "@sonic_swss_common//:libswsscommon_shared",
        "sonic-build-infra": "@sonic_build_infra//platforms:x86_64_trixie",
    }
    registries = re.findall(r"--registry=(\S+)", (repo / ".bazelrc").read_text())
    result = {}
    for name, target in targets.items():
        # The same headers can exist in both execution and target configurations.
        # Select the target configuration before reading its canonical repo name.
        canonical = run_command([args.bazel, "cquery", "config(" + target + ", target)", *common,
                                 "--output=starlark", "--starlark:expr=target.label.repo_name"],
                                repo, validation / "dependency-cquery.log").decode().strip()
        require(re.fullmatch(r"[A-Za-z0-9._+-]+", canonical) is not None,
                "dependency query did not identify one canonical repository: " + name)
        root = Path(execution_root) / "external" / canonical
        module = root / "MODULE.bazel"
        block = re.search(r"module\((.*?)\)", module.read_text(), re.S)
        require(block is not None, "resolved dependency lacks a module declaration: " + name)
        selected_name = re.search(r'\bname\s*=\s*"([^"]+)"', block.group(1))
        selected_version = re.search(r'\bversion\s*=\s*"([^"]+)"', block.group(1))
        version = declarations["bazel_dependency_versions"][name]
        require(selected_name is not None and selected_name.group(1) == name and
                selected_version is not None and selected_version.group(1) == version,
                "resolved module differs from declared dependency: " + name)
        suffix = "/modules/" + name + "/" + version + "/source.json"
        lock = json.loads((repo / "MODULE.bazel.lock").read_text())["registryFileHashes"]
        sources = {url: digest for url, digest in lock.items() if url.endswith(suffix) and digest != "not found" and
                   any(url == registry.rstrip("/") + suffix for registry in registries)}
        require(len(sources) == 1, "lockfile must identify one selected registry source: " + name)
        result[name] = {"version": version, "canonical_repository": canonical,
                        "module_file": stable_file(module), "registry_source": sources}
        if name == "sai":
            source_provenance = root / "SOURCE_PROVENANCE.json"
            provenance = json.loads(source_provenance.read_text())
            result[name]["source_provenance"] = provenance
            result[name]["source_provenance_file"] = stable_file(source_provenance)
    return result


def generated_resolution_provenance(args, repo, validation, required):
    """Retain generated resolution evidence after all dependency queries."""
    require(not run_command(["git", "ls-files", "--", "MODULE.bazel.lock"], repo),
            "MODULE.bazel.lock must not be tracked")
    run_command(["git", "check-ignore", "--quiet", "MODULE.bazel.lock"], repo)
    evidence = validation / "dependencies"
    evidence.mkdir(parents=True, exist_ok=True)
    roots = required["raw_build"] + required["service_free_tests"] + required["package_build"]
    query = "deps(set(" + " ".join(roots) + "))"
    # Query the built configurations. mod graph evaluates even unused module
    # extensions, including legacy extensions outside this component's build.
    command = [args.bazel, "cquery", query, "--lockfile_mode=update", "--output=jsonproto"]
    if args.bazel_config:
        command.append("--config=" + args.bazel_config)
    graph = run_command(command, repo, validation / "dependency-graph.log")
    require(json.loads(graph).get("results"), "configured dependency graph is empty")
    (evidence / "dependency-graph.json").write_bytes(graph)
    # Queries can update the lock: copy it after the last Bazel call.
    lock = repo / "MODULE.bazel.lock"
    json.loads(lock.read_text())
    before = stable_file(lock)
    shutil.copyfile(lock, evidence / lock.name)
    require(stable_file(lock) == stable_file(evidence / lock.name) == before,
            "generated lock changed while collecting resolution evidence")
    result = {
        "kind": "generated_bazel_resolution",
        "source_revision": run_command(["git", "rev-parse", "HEAD"], repo).decode().strip(),
        "github_event_revision": args.event_revision,
        "architecture": args.architecture,
        "bazel_config": args.bazel_config,
        "lockfile_mode": "update",
        "dependency_graph_kind": "configured_target_dependencies",
        "dependency_graph_roots": roots,
        "dependency_graph_command": command,
        "directory": "validation/dependencies",
        "files": {name: stable_file(evidence / name)
                  for name in ("MODULE.bazel.lock", "dependency-graph.json")},
    }
    write_json(evidence / "manifest.json", result)
    return result


def environment_provenance(args, repo, output_dir):
    release = platform.freedesktop_os_release()
    architecture = run_command(["dpkg", "--print-architecture"], repo).decode().strip()
    host_packages = Path(args.host_packages).resolve() if args.host_packages else output_dir / "validation/host-packages.txt"
    return {
        "os": {key: release.get(key) for key in ("ID", "VERSION_ID", "VERSION_CODENAME")},
        "dpkg_architecture": architecture, "native_machine": platform.machine(),
        "image": args.image, "image_evidence_status": "Caller-declared container image identity.",
        "host_packages": {"path": str(host_packages), **stable_file(host_packages)},
    }


def usable_file(archive, path):
    records = archive["non_directories"].get(path, [])
    if not records or any(item["type"] != "file" for item in records):
        return None
    if len({(item.get("sha256"), item["size"]) for item in records}) != 1:
        return None
    return records[0]


def formatted_entries(entries, architecture):
    return [{**entry, "path": entry["path"].format(**architecture)} for entry in entries]


def validate_archive_set(contract, architecture, loaded, audit):
    debug_contract = contract["debug_contract"]
    directory_contract = contract["directory_contract"]
    internal_pairs = []
    for package in contract["packages"]:
        key = package["key"]
        entries = formatted_entries(package["runtime_entries"], architecture)
        additions = formatted_entries(package.get("dev_additions", []), architecture)
        runtime = loaded.get(package["runtime_label"])
        debug = loaded.get(package["debug_label"])
        dev = loaded.get(package.get("dev_label"))
        if runtime is not None:
            audit.inventory(runtime, entries, directory_contract)
        if dev is not None:
            audit.inventory(dev, entries + additions, directory_contract)
            if runtime is not None:
                for entry in entries:
                    path = entry["path"]
                    left, right = runtime["non_directories"].get(path, []), dev["non_directories"].get(path, [])
                    fields = ("type", "linkname", "sha256")
                    same = bool(left and right) and all(left[0].get(field) == right[0].get(field) for field in fields)
                    audit.check("dev_embeds_runtime_content", same, package=key, path=path)
        runtime_records = []
        for entry in entries:
            if entry["role"] != "elf":
                continue
            item = usable_file(runtime, entry["path"]) if runtime is not None else None
            if item is None:
                audit.check("runtime_elf_available", False, package=key, path=entry["path"])
                continue
            try:
                info = inspect_elf(item["_payload"])
            except Exception as error:
                audit.failure("runtime_elf_read", error, package=key, path=entry["path"])
                continue
            start = len(audit.checks)
            for field, expected in (("class", architecture["elf_class"]), ("machine", architecture["elf_machine"]), ("endianness", architecture["elf_endianness"]), ("soname", entry.get("soname"))):
                audit.check("runtime_elf_" + field, info[field] == expected, package=key, path=entry["path"], actual=info[field], expected=expected)
            types = [3] if entry.get("soname") else [2, 3]
            audit.check("runtime_elf_type", info["type"] in types, package=key, path=entry["path"], actual=info["type"], expected=types)
            build_id = info["build_id"]
            valid_build_id = isinstance(build_id, str) and len(build_id) % 2 == 0 and re.fullmatch(r"[0-9a-f]{4,}", build_id) is not None
            audit.check("runtime_build_id", valid_build_id, package=key, path=entry["path"], actual=build_id)
            for section in debug_contract["runtime_sections_absent"]:
                compressed = section.replace(".debug_", ".zdebug_", 1)
                audit.check("runtime_debug_section_absent", section not in info["debug_sections"] and compressed not in info["debug_sections"], package=key, path=entry["path"], section=section)
            audit.check("runtime_debuglink_present", info["debuglink"] is not None, package=key, path=entry["path"])
            runtime_records.append({
                "package": key, "label": entry["label"], "runtime_label": package["runtime_label"], "debug_label": package["debug_label"],
                "runtime_path": entry["path"], "runtime_sha256": item["sha256"], "runtime_elf": info,
                "_runtime_payload": item["_payload"], "runtime_content_checks_passed": all(check["passed"] for check in audit.checks[start:]),
                "debug_path": debug_contract["path_template"].format(build_id_prefix=build_id[:2], build_id_rest=build_id[2:]) if valid_build_id else None,
            })
        wanted_debug = []
        for record in runtime_records:
            if record["debug_path"]:
                wanted_debug.append({
                    "path": record["debug_path"], "type": debug_contract["type"], "role": "debug",
                    "required_metadata": {}, "metadata_evidence": {"classification": "inherited_debug_metadata"},
                    "observation_reference": debug_contract.get("observation_reference", {}),
                })
        audit.check("unique_family_debug_paths", len({item["path"] for item in wanted_debug}) == len(wanted_debug), package=key)
        if debug is not None:
            audit.inventory(debug, wanted_debug, directory_contract)
        for record in runtime_records:
            path = record["debug_path"]
            item = usable_file(debug, path) if debug is not None and path is not None else None
            if item is None:
                audit.check("detached_file_available", False, package=key, path=path, runtime_path=record["runtime_path"])
                continue
            try:
                info = inspect_elf(item["_payload"])
            except Exception as error:
                audit.failure("detached_elf_read", error, package=key, path=path)
                continue
            start = len(audit.checks)
            for field in debug_contract["matching_elf_fields"]:
                audit.check("pair_" + field, info[field] == record["runtime_elf"][field], package=key, path=record["runtime_path"], actual=info[field], expected=record["runtime_elf"][field])
            for section in debug_contract["required_sections"]:
                details = info["debug_section_details"].get(section)
                audit.check("detached_section_file_backed_nonempty", details is not None and details["file_backed"] and details["size"] > 0, package=key, path=path, section=section, actual=details)
            debuglink = record["runtime_elf"]["debuglink"] or {}
            audit.check("debuglink_filename", debuglink.get("filename") == PurePosixPath(path).name, package=key, path=record["runtime_path"], actual=debuglink.get("filename"), expected=PurePosixPath(path).name)
            audit.check("debuglink_crc32", debuglink.get("crc32") == item["crc32"], package=key, path=record["runtime_path"], actual=debuglink.get("crc32"), expected=item["crc32"])
            record.update({
                "debug_sha256": item["sha256"], "debug_elf": info, "_debug_payload": item["_payload"],
                "pair_content_checks_passed": record["runtime_content_checks_passed"] and all(check["passed"] for check in audit.checks[start:]),
            })
            internal_pairs.append(record)
            audit.pairs.append({name: value for name, value in record.items() if not name.startswith("_")})
    expected_count = contract["metadata_policy"]["required_elf_pair_count"]
    audit.check("elf_pair_count", len(internal_pairs) == expected_count, actual=len(internal_pairs), expected=expected_count)
    audit.check("unique_detached_pair_count", len({(item["debug_label"], item["debug_path"]) for item in internal_pairs}) == expected_count, actual=len({(item["debug_label"], item["debug_path"]) for item in internal_pairs}), expected=expected_count)
    return internal_pairs


def gdb_lookup(pair, symbol, executable, temporary, validation, audit):
    if pair is None or not pair.get("pair_content_checks_passed"):
        audit.check("gdb_pair_available", False, reason="no content-validated libsairedis runtime/debug pair")
        return
    staging = temporary / "gdb"
    runtime = staging / "runtime" / PurePosixPath(pair["runtime_path"]).name
    debug_directory = staging / "debug"
    debug = debug_directory / ".build-id" / pair["runtime_elf"]["build_id"][:2] / (pair["runtime_elf"]["build_id"][2:] + ".debug")
    runtime.parent.mkdir(parents=True)
    debug.parent.mkdir(parents=True)
    shutil.copyfile(pair["_runtime_payload"], runtime)
    shutil.copyfile(pair["_debug_payload"], debug)
    require(sha256(runtime) == pair["runtime_sha256"] and sha256(debug) == pair["debug_sha256"], "GDB staging changed pair bytes")
    script = staging / "lookup.gdb"
    python_lines = [
        "import gdb, json, os, re",
        "runtime = " + repr(str(runtime)),
        "debug_directory = " + repr(str(debug_directory)),
        "symbol = " + repr(symbol),
        "for command in ['set pagination off', 'set confirm off', 'set debuginfod enabled off', 'set auto-load python-scripts off', 'set auto-load gdb-scripts off', 'set auto-load local-gdbinit off', 'set auto-load libthread-db off']:",
        "    gdb.execute(command)",
        "gdb.set_parameter('debug-file-directory', debug_directory)",
        "gdb.execute('file ' + json.dumps(runtime, ensure_ascii=False))",
        "text = gdb.execute('info line ' + symbol, to_string=True)",
        "match = re.search(r'Line ([0-9]+) of \"([^\"]+)\"', text)",
        "print('SAI_GDB_JSON=' + json.dumps({'info_line': text, 'line': int(match.group(1)) if match else None, 'source': match.group(2) if match else None, 'objfiles': [os.path.realpath(obj.filename) for obj in gdb.objfiles()]}))",
    ]
    script.write_text("python\n" + "\n".join(python_lines) + "\nend\n")
    command = [executable, "-nx", "-nh", "-batch", "-x", str(script)]
    log = validation / "gdb.log"
    try:
        result = subprocess.run(command, cwd=staging, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, timeout=60)
        log.write_bytes(result.stdout + result.stderr)
        rows = [line[len("SAI_GDB_JSON="):] for line in result.stdout.decode(errors="replace").splitlines() if line.startswith("SAI_GDB_JSON=")]
        evidence = json.loads(rows[0]) if len(rows) == 1 else None
        audit.check("gdb_return_code", result.returncode == 0, actual=result.returncode, expected=0)
        audit.check("gdb_evidence_record", evidence is not None, actual_count=len(rows), expected_count=1)
        audit.check("gdb_exact_detached_file", evidence is not None and str(debug.resolve()) in evidence.get("objfiles", []), expected=str(debug.resolve()))
        audit.check("gdb_positive_source_line", evidence is not None and isinstance(evidence.get("line"), int) and evidence["line"] > 0 and bool(evidence.get("source")), actual=evidence.get("line") if evidence else None)
        audit.gdb = {
            "command": command, "target_execution_requested": False, "symbol": symbol,
            "runtime_sha256": pair["runtime_sha256"], "debug_sha256": pair["debug_sha256"],
            "evidence": evidence, "log": {"path": str(log), **stable_file(log)},
            "source_limit": "The required lookup resolves a recorded source file and line. Source files are not bundled or listed by this check.",
        }
    except Exception as error:
        audit.failure("gdb_lookup", error)
        if isinstance(error, subprocess.TimeoutExpired):
            stdout = error.stdout or b""
            stderr = error.stderr or b""
            log.write_bytes(stdout + stderr)
        audit.gdb = {
            "command": command, "target_execution_requested": False, "symbol": symbol,
            "runtime_sha256": pair["runtime_sha256"], "debug_sha256": pair["debug_sha256"],
            "error": {"type": type(error).__name__, "message": str(error)},
            "log": {"path": str(log), **stable_file(log)} if log.is_file() else None,
        }


def copy_artifacts(outputs, aliases, output_dir):
    destination = output_dir / "packages"
    require(not destination.exists(), "successful package directory already exists")
    staging = Path(tempfile.mkdtemp(prefix=".packages-", dir=output_dir))
    copied, names = [], set()
    try:
        for label, item in outputs.items():
            if label in aliases:
                continue
            name = PurePosixPath(item["exec_path"]).name
            require(name not in names, "unique package outputs have colliding basenames")
            names.add(name)
            require(stable_file(item["path"]) == {key: item[key] for key in ("sha256", "size")}, "package artifact changed before copying")
            target = staging / name
            shutil.copyfile(item["path"], target)
            require(stable_file(target) == {key: item[key] for key in ("sha256", "size")}, "copied package artifact differs from selected output")
            copied.append({"label": label, "file": "packages/" + name, "sha256": item["sha256"], "size": item["size"]})
        require(len(copied) == 13, "successful artifact set does not contain 13 tars")
        require(not destination.exists(), "successful package directory appeared during copying")
        staging.rename(destination)
        return copied
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--architecture", choices=("amd64", "arm64"), required=True)
    parser.add_argument("--bazel-config", default="")
    parser.add_argument("--image", required=True)
    parser.add_argument("--targets", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path("."))
    parser.add_argument("--bazel", default="bazel", help="Bazel executable path, including a local startup wrapper when needed")
    parser.add_argument("--gdb", default="gdb")
    parser.add_argument("--host-packages", type=Path)
    parser.add_argument("--event-revision", default=os.environ.get("GITHUB_SHA"))
    args = parser.parse_args()
    repo = args.repo_root.resolve()
    output_dir = args.output_dir.resolve()
    validation = output_dir / "validation"
    validation.mkdir(parents=True, exist_ok=True)
    report_path = validation / "package-report.json"
    audit = Audit()
    started = datetime.now(timezone.utc).isoformat()
    provenance = {"github_event_revision": args.event_revision, "architecture": args.architecture}
    outputs, aliases, source_before, copied = {}, {}, None, []
    required_work_completed = False
    try:
        require(repo.is_dir() and output_dir != repo, "repository and artifact output directory must be distinct directories")
        require(not (output_dir / "packages").exists() and not (output_dir / "provenance.json").exists(), "artifact output directory already contains success artifacts")
        require(args.bazel_config == ("" if args.architecture == "amd64" else "aarch64"), "Bazel config name differs from the native architecture contract")
        require(re.search(r"(?:^|@)sha256:[0-9a-f]{64}$", args.image), "image identity must contain an immutable SHA256 digest")
        if args.event_revision is not None:
            require(re.fullmatch(r"[0-9a-f]{40}", args.event_revision), "GitHub event revision is not a full commit SHA")
        contract_path, targets_path = args.contract.resolve(), args.targets.resolve()
        contract = json.loads(contract_path.read_text())
        required = json.loads(targets_path.read_text())
        labels, aliases = validate_contract(contract, required, args.architecture)
        architecture = contract["architectures"][args.architecture]
        provenance["inputs"] = {
            "contract": {"path": str(contract_path), **stable_file(contract_path)},
            "required_targets": {"path": str(targets_path), **stable_file(targets_path)},
            "verifier": {"path": str(Path(__file__).resolve()), **stable_file(Path(__file__).resolve())},
        }
        source_before = source_snapshot(repo, output_dir)
        provenance["source_before"] = source_before
        audit.check("tracked_source_clean", source_before["tracked_diff"]["size"] == 0)
        declarations = declaration_provenance(repo)
        provenance["declarations"] = declarations
        environment = environment_provenance(args, repo, output_dir)
        provenance["environment"] = environment
        audit.check("debian_trixie", environment["os"].get("ID") == "debian" and environment["os"].get("VERSION_CODENAME") == "trixie", actual=environment["os"])
        audit.check("native_architecture", environment["dpkg_architecture"] == args.architecture and environment["native_machine"] == architecture["native_machine"], actual={"dpkg": environment["dpkg_architecture"], "machine": environment["native_machine"]}, expected={"dpkg": args.architecture, "machine": architecture["native_machine"]})
        with tempfile.TemporaryDirectory(prefix="package-verifier-", dir=validation) as temporary_text:
            temporary = Path(temporary_text)
            outputs, bazel = resolve_outputs(args, repo, temporary, validation, labels, aliases, architecture, audit)
            provenance["bazel"] = bazel
            resolved = resolved_dependency_provenance(args, repo, validation, bazel["execution_root"], declarations)
            provenance["resolved_dependencies"] = resolved
            provenance["generated_resolution"] = generated_resolution_provenance(args, repo, validation, required)
            sai_revision = resolved["sai"]["source_provenance"]["source_commit"]
            audit.check("sai_gitlink_matches_registry", source_before["sai_gitlink"] == sai_revision,
                        actual=source_before["sai_gitlink"], expected=sai_revision)
            provenance["public_outputs"] = list(outputs.values())
            provenance["debug_aliases"] = aliases
            loaded = {}
            for number, label in enumerate(labels):
                if label in aliases:
                    continue
                try:
                    loaded[label] = inspect_archive(outputs[label], temporary / "payloads" / str(number), audit)
                except Exception as error:
                    audit.failure("archive_inspection", error, label=label)
            pairs = validate_archive_set(contract, architecture, loaded, audit)
            lookup = contract["debug_contract"]["gdb_lookup"]
            candidates = [item for item in pairs if item["package"] == lookup["package"]]
            gdb_lookup(candidates[0] if len(candidates) == 1 else None, lookup["symbol"], args.gdb, temporary, validation, audit)
        required_work_completed = True
    except Exception as error:
        audit.failure("verifier", error)
    except KeyboardInterrupt as error:
        audit.failure("verifier_interrupted", error)
    finally:
        audit.check("required_work_completed", required_work_completed)
        if source_before is not None:
            try:
                source_after = source_snapshot(repo, output_dir)
                provenance["source_after"] = source_after
                audit.check("source_unchanged_during_verification", source_after == source_before)
            except Exception as error:
                audit.failure("source_recheck", error)
        for label, item in outputs.items():
            try:
                audit.check("artifact_unchanged_during_verification", stable_file(item["path"]) == {key: item[key] for key in ("sha256", "size")}, label=label)
            except Exception as error:
                audit.failure("artifact_recheck", error, label=label)
        for name, item in provenance.get("inputs", {}).items():
            try:
                audit.check("verifier_input_unchanged", stable_file(item["path"]) == {key: item[key] for key in ("sha256", "size")}, input=name)
            except Exception as error:
                audit.failure("verifier_input_recheck", error, input=name)
        resolution = provenance.get("generated_resolution")
        if resolution is not None:
            for name, expected in resolution["files"].items():
                try:
                    audit.check("resolution_evidence_unchanged", stable_file(validation / "dependencies" / name) == expected, file=name)
                except Exception as error:
                    audit.failure("resolution_evidence_recheck", error, file=name)
            try:
                audit.check("generated_lock_unchanged", stable_file(repo / "MODULE.bazel.lock") == resolution["files"]["MODULE.bazel.lock"])
            except Exception as error:
                audit.failure("generated_lock_recheck", error)
        host_packages = provenance.get("environment", {}).get("host_packages")
        if host_packages is not None:
            try:
                audit.check("host_package_manifest_unchanged", stable_file(host_packages["path"]) == {key: host_packages[key] for key in ("sha256", "size")})
            except Exception as error:
                audit.failure("host_package_manifest_recheck", error)
        if audit.checks and all(check["passed"] for check in audit.checks):
            try:
                copied = copy_artifacts(outputs, aliases, output_dir)
            except Exception as error:
                audit.failure("artifact_copy", error)
        passed = bool(audit.checks) and all(check["passed"] for check in audit.checks)
        observation_summary = {
            "count": len(audit.metadata_observations),
            "make_difference_count": sum(item["comparisons"].get("make", {}).get("matches") is False for item in audit.metadata_observations),
            "inherited_reference_difference_count": sum(comparison.get("matches") is False for item in audit.metadata_observations for name, comparison in item["comparisons"].items() if name.startswith("inherited")),
            "gates_pass_fail": False,
        }
        report = {
            "schema_version": 1, "status": "passed" if passed else "failed", "started_at": started,
            "finished_at": datetime.now(timezone.utc).isoformat(),
            "summary": {"checks": len(audit.checks), "failed_checks": sum(not check["passed"] for check in audit.checks), "elf_pairs": len(audit.pairs), "metadata_observations": observation_summary},
            "checks": audit.checks, "metadata_observations": audit.metadata_observations,
            "archives": audit.archives, "pairs": audit.pairs, "gdb": audit.gdb,
            "provenance": provenance, "copied_artifacts": copied,
            "limitations": [
                "This verifier consumes existing cquery-selected outputs and does not build them. The caller must retain the preceding successful build evidence for the same configuration.",
                "The source snapshot covers the verification window. This script alone does not prove when the tar bytes were built.",
                "Package inputs can differ from canonical raw ELFs; the verifier checks each selected runtime/debug archive pair.",
                "DT_NEEDED and runtime search tags are recorded. Installed loader closure and service behavior are separate runtime checks.",
                "Inherited metadata and Make comparisons are observations and never affect package status.",
            ],
        }
        write_json(report_path, report)
        if passed:
            try:
                successful_provenance = {
                    "schema_version": 1, "status": "passed", "verified_at": report["finished_at"],
                    "architecture": args.architecture, "github_event_revision": args.event_revision,
                    "source_snapshot": provenance["source_after"], "inputs": provenance["inputs"],
                    "declarations": provenance["declarations"],
                    "resolved_dependencies": provenance["resolved_dependencies"],
                    "generated_resolution": provenance["generated_resolution"],
                    "environment": provenance["environment"],
                    "bazel": provenance["bazel"], "public_outputs": provenance["public_outputs"],
                    "debug_aliases": aliases, "artifacts": copied,
                    "validation_report": {"path": "validation/package-report.json", **stable_file(report_path)},
                    "metadata_observations": observation_summary,
                }
                write_json(output_dir / "provenance.json", successful_provenance)
            except Exception as error:
                audit.failure("provenance_write", error)
                report["status"] = "failed"
                report["finished_at"] = datetime.now(timezone.utc).isoformat()
                report["summary"]["checks"] = len(audit.checks)
                report["summary"]["failed_checks"] = sum(not check["passed"] for check in audit.checks)
                write_json(report_path, report)
    print(json.dumps({"status": report["status"], "report": str(report_path), **report["summary"]}, sort_keys=True))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
