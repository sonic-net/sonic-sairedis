#!/usr/bin/env python3
"""Check Make input drift and component source ownership from aquery JSON."""

import argparse
import collections
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import subprocess
import sys


C_SUFFIXES = (".c", ".cc", ".cpp", ".cxx", ".C")
MAIN_LABEL_PREFIXES = ("@@_main//", "@_main//", "@@//", "@//", "//")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON object key")
        result[key] = value
    return result


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def relative_path(value):
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    return not path.is_absolute() and ".." not in path.parts and str(path) == value


def validate_manifest(manifest):
    if manifest.get("schema_version") != 2:
        raise ValueError("unsupported source manifest schema")
    expected = manifest["expected_sources"]
    if expected != sorted(set(expected)) or not all(relative_path(path) for path in expected):
        raise ValueError("expected sources must be unique sorted relative paths")
    if len(expected) != manifest["reference"]["distinct_source_count"]:
        raise ValueError("source count does not match reference")
    generated = manifest["expected_generated_sources"]
    if generated != sorted(set(generated)) or not set(generated).issubset(expected):
        raise ValueError("invalid generated source inventory")
    for key in ("generated_bazel_bin_suffixes", "pinned_sai_support_sources"):
        mapping = manifest[key]
        if not all(relative_path(path) and relative_path(source) for path, source in mapping.items()):
            raise ValueError("source mappings must use relative paths")
        if not set(mapping.values()).issubset(expected):
            raise ValueError("source mapping is outside expected inventory")
    ownership = manifest["expected_label_sources"]
    pairs = set()
    for label, sources in ownership.items():
        if not label.startswith("//") or label.count(":") != 1:
            raise ValueError("expected ownership labels must use main-repository spelling")
        if sources != sorted(set(sources)) or not all(relative_path(path) for path in sources):
            raise ValueError("ownership sources must be unique sorted relative paths")
        pairs.update((label, source) for source in sources)
    if len(pairs) != manifest["reference"]["label_source_pair_count"]:
        raise ValueError("ownership pair count does not match reviewed reference")
    if {source for _, source in pairs} != set(expected):
        raise ValueError("ownership pair union differs from expected source inventory")
    review = manifest["ownership_review"]
    if review.get("status") != "reviewed" or set(review["owners"]) != set(ownership):
        raise ValueError("ownership review does not cover every expected label")
    if len(ownership) != manifest["reference"]["label_count"] or any(
        review["owners"][label]["expected_pair_count"] != len(sources)
        for label, sources in ownership.items()
    ):
        raise ValueError("ownership review counts do not match expected labels")
    encoded_pairs = json.dumps([list(pair) for pair in sorted(pairs)], ensure_ascii=True, separators=(",", ":")).encode()
    if sha256(encoded_pairs) != review["reviewed_pairs_sha256"]:
        raise ValueError("ownership pairs differ from the reviewed identity")
    guard = manifest["make_input_drift_guard"]
    makefiles = guard["tracked_makefile_am_sha256"]
    if len(makefiles) != guard["tracked_makefile_am_count"]:
        raise ValueError("Makefile count does not match drift manifest")
    if not all(relative_path(path) and PurePosixPath(path).name == "Makefile.am" for path in makefiles):
        raise ValueError("invalid tracked Makefile path")
    hashes = list(makefiles.values()) + [guard["configure_ac"]["sha256"]]
    if not all(isinstance(value, str) and len(value) == 64 and set(value) <= set("0123456789abcdef") for value in hashes):
        raise ValueError("invalid SHA256 in Make input drift manifest")
    if guard["configure_ac"]["path"] != "configure.ac" or guard["sai_gitlink"]["path"] != "SAI":
        raise ValueError("unexpected configure or SAI path")
    commit = guard["sai_gitlink"]["commit"]
    if not isinstance(commit, str) or len(commit) != 40 or not set(commit) <= set("0123456789abcdef"):
        raise ValueError("invalid SAI gitlink commit")
    return manifest


def git(repo_root, *args):
    return subprocess.run(
        ["git", "--no-optional-locks", "-c", "core.fsmonitor=false", *args],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    ).stdout


def tracked_entries(repo_root):
    entries = collections.defaultdict(list)
    for record in git(repo_root, "ls-files", "--stage", "-z").split(b"\0"):
        if not record:
            continue
        metadata, path_bytes = record.split(b"\t", 1)
        mode, object_id, stage = metadata.decode("ascii").split()
        path = path_bytes.decode("utf-8")
        entries[path].append({"mode": mode, "object_id": object_id, "stage": stage})
    return entries


def check_make_inputs(repo_root, guard):
    result = {
        "status": "fail",
        "refresh_required": True,
        "tracked_makefile_am_count": 0,
        "added_tracked_makefile_am": [],
        "removed_tracked_makefile_am": [],
        "content_mismatches": [],
        "entry_errors": [],
        "sai_gitlink": {"matches": False},
    }
    try:
        entries = tracked_entries(repo_root)
    except (OSError, subprocess.SubprocessError, UnicodeError, ValueError):
        result["entry_errors"].append({"error": "could not read main-repository Git index"})
        return result
    expected_makefiles = guard["tracked_makefile_am_sha256"]
    actual_makefiles = {path for path in entries if PurePosixPath(path).name == "Makefile.am"}
    result["tracked_makefile_am_count"] = len(actual_makefiles)
    result["added_tracked_makefile_am"] = sorted(actual_makefiles - set(expected_makefiles))
    result["removed_tracked_makefile_am"] = sorted(set(expected_makefiles) - actual_makefiles)
    expected_files = dict(expected_makefiles)
    expected_files[guard["configure_ac"]["path"]] = guard["configure_ac"]["sha256"]
    for path, expected_hash in sorted(expected_files.items()):
        index_entries = entries.get(path, [])
        if len(index_entries) != 1 or index_entries[0]["stage"] != "0" or index_entries[0]["mode"] not in ("100644", "100755"):
            result["entry_errors"].append({"path": path, "error": "expected one stage-0 regular-file index entry"})
            continue
        try:
            index_hash = sha256(git(repo_root, "cat-file", "blob", index_entries[0]["object_id"]))
            file_path = repo_root / path
            if not stat.S_ISREG(file_path.lstat().st_mode):
                raise OSError("not a regular file")
            working_hash = sha256(file_path.read_bytes())
        except (OSError, subprocess.SubprocessError):
            result["entry_errors"].append({"path": path, "error": "could not hash index and working-tree regular-file bytes"})
            continue
        if index_hash != expected_hash or working_hash != expected_hash:
            result["content_mismatches"].append({
                "path": path,
                "expected_sha256": expected_hash,
                "index_sha256": index_hash,
                "working_tree_sha256": working_hash,
            })
    sai = guard["sai_gitlink"]
    sai_entries = entries.get(sai["path"], [])
    actual_commit = None
    if len(sai_entries) == 1 and sai_entries[0]["stage"] == "0" and sai_entries[0]["mode"] == "160000":
        actual_commit = sai_entries[0]["object_id"]
    else:
        result["entry_errors"].append({"path": sai["path"], "error": "expected one stage-0 gitlink index entry"})
    result["sai_gitlink"] = {
        "path": sai["path"],
        "expected_commit": sai["commit"],
        "actual_commit": actual_commit,
        "matches": actual_commit == sai["commit"],
    }
    passed = not any(result[key] for key in (
        "added_tracked_makefile_am", "removed_tracked_makefile_am", "content_mismatches", "entry_errors"
    )) and result["sai_gitlink"]["matches"]
    result["status"] = "pass" if passed else "fail"
    result["refresh_required"] = not passed
    if not passed:
        result["required_action"] = "Review Make source ownership and refresh the source inventory after input changes; repair checkout errors before retrying."
    return result


def normalize_main_label(label):
    if not isinstance(label, str):
        return None
    for prefix in MAIN_LABEL_PREFIXES:
        if label.startswith(prefix):
            return "//" + label[len(prefix):]
    return None


def primary_source(action):
    arguments = action.get("arguments", [])
    if not isinstance(arguments, list) or not all(isinstance(value, str) for value in arguments):
        raise ValueError("compile arguments are unavailable")
    positions = [index for index, value in enumerate(arguments) if value == "-c"]
    if len(positions) != 1 or positions[0] + 1 >= len(arguments):
        raise ValueError("expected exactly one standalone -c source argument")
    source = arguments[positions[0] + 1].removeprefix("./")
    if not source.endswith(C_SUFFIXES):
        raise ValueError("primary source does not have a C/C++ suffix")
    if not relative_path(source):
        raise ValueError("primary source is not a normalized relative path")
    return source


def normalize_source(source, configuration, manifest):
    generated = manifest["generated_bazel_bin_suffixes"]
    support = manifest["pinned_sai_support_sources"]
    parts = source.split("/")
    if len(parts) >= 4 and parts[0] == "bazel-out" and parts[2] == "bin":
        suffix = "/".join(parts[3:])
        if suffix in generated:
            if parts[1] != configuration["mnemonic"]:
                raise ValueError("generated source configuration differs from compile configuration")
            return generated[suffix], "generated"
        return source, "unrecognized_generated"
    if source in support:
        return support[source], "pinned_sai_support"
    if source in manifest["expected_generated_sources"]:
        raise ValueError("expected generated source was compiled from a tracked path")
    if source in support.values():
        raise ValueError("SAI support source was not compiled from the pinned external archive")
    if source.startswith("external/"):
        raise ValueError("unclassified external source compiled by a main-repository target")
    return source, "tracked"


def compare(graph, manifest, drift):
    targets = {str(item["id"]): item["label"] for item in graph.get("targets", [])}
    configurations = {
        str(item["id"]): {
            "id": str(item["id"]),
            "mnemonic": item.get("mnemonic"),
            "checksum": item.get("checksum"),
            "is_tool": bool(item.get("isTool", False)),
        }
        for item in graph.get("configuration", [])
    }
    rows = []
    errors = []
    external = collections.Counter()
    cpp_actions = 0
    for action in graph.get("actions", []):
        if action.get("mnemonic") != "CppCompile":
            continue
        cpp_actions += 1
        reported_label = targets.get(str(action.get("targetId")))
        configuration = configurations.get(str(action.get("configurationId")))
        if reported_label is None or configuration is None or not configuration["mnemonic"]:
            errors.append({"error": "CppCompile action lacks target or configuration metadata"})
            continue
        label = normalize_main_label(reported_label)
        if label is None:
            external["exec" if configuration["is_tool"] else "target"] += 1
            continue
        row = {"label": label, "configuration": configuration}
        try:
            source = primary_source(action)
            normalized, kind = normalize_source(source, configuration, manifest)
            row.update(primary_source=source, normalized_source=normalized, source_kind=kind)
        except ValueError as error:
            row["error"] = str(error)
            errors.append(row)
            continue
        rows.append(row)

    target_rows = [row for row in rows if not row["configuration"]["is_tool"]]
    exec_rows = [row for row in rows if row["configuration"]["is_tool"]]
    target_sources = {row["normalized_source"] for row in target_rows}
    expected_sources = set(manifest["expected_sources"])
    missing = sorted(expected_sources - target_sources)
    unexpected = sorted(target_sources - expected_sources)
    source_counts = collections.Counter(row["normalized_source"] for row in target_rows)
    pair_counts = collections.Counter((row["label"], row["normalized_source"]) for row in target_rows)
    expected_pairs = {
        (label, source) for label, sources in manifest["expected_label_sources"].items() for source in sources
    }
    observed_pairs = set(pair_counts)
    ownership = {
        "expected_pair_count": len(expected_pairs),
        "observed_distinct_pair_count": len(observed_pairs),
        "missing_pairs": [{"label": label, "source": source} for label, source in sorted(expected_pairs - observed_pairs)],
        "unexpected_pairs": [{"label": label, "source": source} for label, source in sorted(observed_pairs - expected_pairs)],
        "duplicate_pairs": [
            {"label": label, "source": source, "action_count": count}
            for (label, source), count in sorted(pair_counts.items()) if count != 1
        ],
    }
    target_configurations = {
        row["configuration"]["id"]: row["configuration"] for row in target_rows
    }
    passed = not missing and not unexpected and not errors and drift["status"] == "pass" and not any(
        ownership[key] for key in ("missing_pairs", "unexpected_pairs", "duplicate_pairs")
    )
    return {
        "schema_version": 2,
        "evidence_kind": "configured_action_source_ownership_and_reachability",
        "status": "pass" if passed else "fail",
        "expected_count": len(expected_sources),
        "observed_target_distinct_count": len(target_sources),
        "missing": missing,
        "unexpected": unexpected,
        "ownership": ownership,
        "make_input_drift": drift,
        "extraction_or_provenance_errors": errors,
        "cpp_compile_action_count": cpp_actions,
        "external_dependency_action_counts_excluded": dict(sorted(external.items())),
        "component_target_action_count": len(target_rows),
        "component_exec_action_count_excluded": len(exec_rows),
        "component_target_configurations": sorted(target_configurations.values(), key=lambda item: (item["checksum"] or "", item["id"])),
        "configuration_scope": "Configurations are reported for diagnosis. This source check does not establish option or platform equivalence.",
        "component_target_duplicate_action_counts": {
            source: count for source, count in sorted(source_counts.items()) if count > 1
        },
        "component_actions": sorted(rows, key=lambda row: (
            row["configuration"]["is_tool"], row["normalized_source"], row["label"]
        )),
        "reference": manifest["reference"],
        "comparison_scope": manifest["comparison_scope"],
    }


def write_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path("."))
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest = validate_manifest(json.loads(args.manifest.read_text(), object_pairs_hook=unique_object))
        drift = check_make_inputs(args.repo_root, manifest["make_input_drift_guard"])
        graph = json.load(sys.stdin, object_pairs_hook=unique_object)
        result = compare(graph, manifest, drift)
    except (OSError, ValueError, KeyError, TypeError, UnicodeError) as error:
        result = {
            "schema_version": 2,
            "status": "error",
            "error": "Source reachability input or manifest could not be processed.",
            "error_type": type(error).__name__,
        }
    write_report(args.output, result)
    summary_keys = (
        "status", "error", "error_type", "expected_count", "observed_target_distinct_count",
        "missing", "unexpected", "ownership", "make_input_drift", "extraction_or_provenance_errors",
        "component_target_action_count", "component_exec_action_count_excluded",
        "external_dependency_action_counts_excluded", "component_target_configurations",
    )
    print(json.dumps({key: result[key] for key in summary_keys if key in result}, indent=2, sort_keys=True))
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
