#!/usr/bin/env python3
"""Audit the traced build and bind actual extracted C++ files to owner inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_source_reachability import normalize_main_label

QUERY = """import cpp
from Compilation compilation, File source
where source = compilation.getAFileCompiled() and
  not compilation.buildModeNone() and compilation.normalTermination()
select source.getAbsolutePath()
"""


def audit(graph, preparation=False):
    targets = {str(item["id"]): item["label"] for item in graph.get("targets", [])}
    fragments = {str(item["id"]): item for item in graph.get("pathFragments", [])}

    def path(identifier):
        node = fragments[str(identifier)]
        return (path(node["parentId"]) + "/" if node.get("parentId") else "") + node["label"]

    artifacts = {str(item["id"]): path(item["pathFragmentId"]) for item in graph.get("artifacts", [])}
    actions = graph.get("actions", [])
    if not actions:
        raise AssertionError("The configured action graph is empty")
    owned = []
    for action in actions:
        label = targets[str(action["targetId"])]
        outputs = [artifacts[str(identifier)] for identifier in action.get("outputIds", [])]
        if any(name.endswith((".deb", ".udeb", ".ddeb")) for name in outputs):
            raise AssertionError("DEB-producing output: " + label)
        command = " ".join(action.get("arguments", []))
        if re.search(r"(?:^|[\s/;])(?:dpkg-buildpackage|dpkg-binary|pkg_deb|build_deb)(?:[\s;]|$)", command):
            raise AssertionError("Debian packaging wrapper: " + label)
        if re.search(r"(?:^|[\s/;])dpkg-deb(?:[\s;]|$)", command) and re.search(r"(?:^|\s)(?:--build|-b)(?:\s|$)", command):
            raise AssertionError("Debian packaging wrapper: " + label)
        if action.get("mnemonic") == "CppCompile" and normalize_main_label(label):
            owned.append(label)
    if preparation and owned:
        raise AssertionError("Preparation compiles Sairedis before tracing: " + ", ".join(sorted(set(owned))))
    return {"status": "passed", "actions": len(actions), "outputs": len(artifacts),
            "owner_cpp_actions": len(owned), "preparation": preparation, "deb_outputs": 0}


def execution_root_from_events(events, output_base):
    records = [json.loads(line) for line in events.read_text().splitlines() if line.strip()]
    roots = {row["workspaceInfo"]["localExecRoot"] for row in records if "workspaceInfo" in row}
    finished = [row["finished"] for row in records if "finished" in row]
    if len(roots) != 1 or len(finished) != 1:
        raise AssertionError("Expected one successful preparation build and execution root")
    result = finished[0]
    code = result.get("exitCode", {})
    # Protobuf JSON omits numeric zero; actual Bazel 8.5.1 reports SUCCESS
    # without a code field. Require both positive status fields as well.
    if result.get("overallSuccess") is not True or code.get("name") != "SUCCESS" or code.get("code", 0) != 0:
        raise AssertionError("Expected one successful preparation build and execution root")
    value = roots.pop()
    root = Path(value)
    expected_parent = output_base.resolve() / "execroot"
    if not value or not root.is_absolute() or not root.is_dir() or root.resolve().parent != expected_parent:
        raise AssertionError("Preparation execution root is outside its declared output base")
    return root.resolve()


def snapshot(report, execution_root):
    if report.get("status") != "pass":
        raise AssertionError("The reviewed source reachability check did not pass")
    records = {}
    for row in report["component_actions"]:
        if row["configuration"]["is_tool"]:
            continue
        relative = row["primary_source"]
        lexical = execution_root / relative
        actual = lexical.resolve(strict=True)
        if not actual.is_file():
            raise AssertionError("Expected a regular compiled input: " + relative)
        records[relative] = {"normalized_source": row["normalized_source"],
                             "paths": sorted({str(lexical), str(actual)}),
                             "sha256": hashlib.sha256(actual.read_bytes()).hexdigest()}
    if len({row["normalized_source"] for row in records.values()}) != report["expected_count"]:
        raise AssertionError("Compiled source snapshot does not cover the reviewed inventory")
    return {"status": "passed", "expected_source_count": report["expected_count"],
            "owned_pair_count": report["ownership"]["expected_pair_count"], "sources": records}


def verify_extraction(expected, rows, source_zip):
    extracted = {row[0] for row in rows}
    checked = {}
    with zipfile.ZipFile(source_zip) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise AssertionError("Duplicate names in extracted source archive")
        for relative, record in expected["sources"].items():
            matches = extracted.intersection(record["paths"])
            if not matches:
                raise AssertionError("No actual extracted compilation for " + relative)
            for path in matches:
                member = path.lstrip("/")
                if member not in names or hashlib.sha256(archive.read(member)).hexdigest() != record["sha256"]:
                    raise AssertionError("Extracted source bytes differ: " + relative)
            checked[relative] = {**record, "extracted_paths": sorted(matches)}
    return {"status": "passed", "evidence_kind": "finalized_cpp_compilations_and_exact_source_bytes",
            "expected_source_count": expected["expected_source_count"],
            "owned_pair_count_from_aquery": expected["owned_pair_count"], "sources": checked}


def extract(database, expected, output):
    # Use the initialized action's pinned bundle, as in WPA's owner verifier.
    selected = Path(os.environ["CODEQL_DIST"]) / "codeql" if os.environ.get("CODEQL_DIST") else Path(shutil.which("codeql") or "")
    cli = selected.resolve()
    if not cli.is_file():
        raise AssertionError("The initialized CodeQL CLI is unavailable")
    version = json.loads(subprocess.check_output([str(cli), "version", "--format=json"], text=True))
    if version.get("version") != "2.27.0":
        raise AssertionError("CodeQL differs from the pinned 2.27.0 bundle")
    packs = json.loads(subprocess.check_output([str(cli), "resolve", "qlpacks", "--format=json"], text=True))
    candidates = [Path(path).resolve() for path in packs.get("codeql/cpp-all", [])]
    versions = {re.search(r"^version: (\S+)$", (path / "qlpack.yml").read_text(), re.M).group(1)
                for path in candidates if path.is_relative_to(cli.parent)}
    if versions != {"12.1.0"}:
        raise AssertionError("Expected cpp-all 12.1.0 from the pinned bundle")
    query_dir = output.parent / "query"
    query_dir.mkdir(parents=True, exist_ok=True)
    (query_dir / "qlpack.yml").write_text("name: sonic/sairedis-coverage\nversion: 0.0.0\ndependencies:\n  codeql/cpp-all: 12.1.0\n")
    query = query_dir / "compilations.ql"
    query.write_text(QUERY)
    result = query_dir / "compilations.bqrs"
    subprocess.run([str(cli), "query", "run", "--database=" + str(database),
                    "--search-path=" + str(cli.parent), "--threads=2", "--ram=2048",
                    "--output=" + str(result), str(query)], check=True)
    decoded = json.loads(subprocess.check_output([str(cli), "bqrs", "decode", "--format=json", str(result)], text=True))
    (query_dir / "compilations.json").write_text(json.dumps(decoded, indent=2) + "\n")
    return {**verify_extraction(expected, decoded["#select"]["tuples"], database / "src.zip"),
            "codeql": version["version"], "cpp_all": "12.1.0"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("audit", "snapshot", "coverage"))
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--preparation", action="store_true")
    parser.add_argument("--execution-root", type=Path)
    parser.add_argument("--database", type=Path)
    parser.add_argument("--build-events", type=Path)
    parser.add_argument("--output-base", type=Path)
    args = parser.parse_args()
    data = json.loads(args.input.read_text())
    if args.mode == "audit":
        result = audit(data, args.preparation)
    elif args.mode == "snapshot":
        if args.build_events:
            if args.execution_root or not args.output_base:
                parser.error("Build events require only their explicit output base")
            root = execution_root_from_events(args.build_events, args.output_base)
            result = {**snapshot(data, root), "execution_root": str(root),
                      "preparation_bep_sha256": hashlib.sha256(args.build_events.read_bytes()).hexdigest()}
        else:
            result = snapshot(data, args.execution_root)
    else:
        result = extract(args.database, data, args.output)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
