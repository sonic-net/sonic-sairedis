#!/usr/bin/env python3
"""Derive a deterministic PEP 440 version from an intact PTF checkout."""

import argparse
import pathlib
import re
import subprocess
import sys


DESCRIBE_RE = re.compile(
    r"^v?(?P<version>[0-9]+(?:\.[0-9]+)*)-"
    r"(?P<distance>[0-9]+)-g(?P<revision>[0-9a-f]+)$"
)
VERSION_RE = re.compile(r"^[0-9]+(?:\.[0-9]+)*$")


def git(ptf_dir, *args):
    result = subprocess.run(
        ["git", "-C", str(ptf_dir), *args],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout.strip()


def version_from_describe(description):
    match = DESCRIBE_RE.fullmatch(description)
    if not match:
        raise ValueError(f"unsupported git describe output: {description}")

    version = match.group("version")
    distance = int(match.group("distance"))
    if distance == 0:
        return version
    return f"{version}.post{distance}+g{match.group('revision')}"


def fallback_version(ptf_dir, revision):
    version_file = pathlib.Path(ptf_dir) / "Version.txt"
    if version_file.is_file():
        base_version = version_file.read_text(encoding="utf-8").strip()
        if not VERSION_RE.fullmatch(base_version):
            raise ValueError(f"invalid PTF Version.txt value: {base_version}")
        return f"{base_version}+g{revision}"

    commit_count = git(ptf_dir, "rev-list", "--count", "HEAD")
    return f"0.0.post{commit_count}+g{revision}"


def derive_version(ptf_dir):
    if git(ptf_dir, "status", "--porcelain"):
        raise ValueError(f"PTF checkout is dirty: {ptf_dir}")

    revision = git(ptf_dir, "rev-parse", "--short=7", "HEAD")
    try:
        description = git(
            ptf_dir,
            "describe",
            "--tags",
            "--long",
            "--match",
            "v[0-9]*",
            "HEAD",
        )
    except subprocess.CalledProcessError:
        return fallback_version(ptf_dir, revision)
    return version_from_describe(description)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ptf_dir", type=pathlib.Path)
    args = parser.parse_args(argv)

    try:
        print(derive_version(args.ptf_dir))
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
