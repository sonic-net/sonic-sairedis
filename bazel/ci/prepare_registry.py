#!/usr/bin/env python3
"""Use the maintained registry with controlled build settings and local caches."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess

MAIN = "https://raw.githubusercontent.com/securely1g/sonic-bazel-registry/main"
BCR = "https://bcr.bazel.build/"


def prepare(workspace, home, output, revision, bazel):
    declared = (workspace / ".bazelrc").read_text()
    registries = re.findall(r"--registry=(\S+)", declared)
    if registries != [MAIN, BCR]:
        raise ValueError("Expected exactly maintained SONiC main and BCR")
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Observed registry revision must be a full Git commit")
    if not Path(bazel).is_absolute() or not os.access(bazel, os.X_OK):
        raise ValueError("Expected the original absolute Bazel executable")
    # Keep the tracked registry endpoints and source-owner declarations intact.
    # The alternate rc isolates build settings while retaining local caches.
    lines = declared.splitlines()
    allowed = {"--disk_cache", "--repository_cache", "--distdir", "--downloader_config",
               "--experimental_repository_cache_hardlinks", "--experimental_downloader_config"}
    for path in (home / ".bazelrc", workspace / ".bazelrc.user"):
        if not path.is_file():
            continue
        for line in path.read_text().splitlines():
            tokens = shlex.split(line, comments=True)
            if not tokens or tokens[0] not in ("common", "build", "test"):
                continue
            for index, token in enumerate(tokens[1:], 1):
                option = token.split("=", 1)[0]
                if option not in allowed:
                    continue
                if "=" not in token and option != "--experimental_repository_cache_hardlinks":
                    if index + 1 == len(tokens):
                        raise ValueError("Missing cache option value")
                    token += "=" + tokens[index + 1]
                lines.append(tokens[0] + " " + shlex.quote(token))
    output.mkdir(parents=True, exist_ok=True)
    effective = output / "effective.bazelrc"
    effective.write_text("\n".join(lines) + "\n")
    (output / "declared.bazelrc").write_text(declared)
    binary = output / "bin/bazel"
    binary.parent.mkdir(exist_ok=True)
    # Bazelisk handles standalone --version before Bazel startup options.
    # It does not load a registry or evaluate workspace configuration.
    binary.write_text("#!/bin/sh\n" +
                      'if [ "$#" -eq 1 ] && [ "$1" = "--version" ]; then\n  exec ' +
                      shlex.quote(bazel) + ' "$@"\nfi\nexec ' + shlex.quote(bazel) +
                      " --nosystem_rc --nohome_rc --noworkspace_rc --bazelrc=" +
                      shlex.quote(str(effective)) + ' "$@"\n')
    binary.chmod(0o755)
    (output / "registry-inputs.json").write_text(json.dumps({
        "declared_registry": MAIN, "registry": MAIN, "observed_revision": revision,
        "draft_validation": False, "source_overrides": False,
        "declared_rc_sha256": hashlib.sha256(declared.encode()).hexdigest(),
        "effective_rc_sha256": hashlib.sha256(effective.read_bytes()).hexdigest(),
    }, indent=2) + "\n")
    return binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--bazel", default=shutil.which("bazel") or "")
    args = parser.parse_args()
    ref = "refs/heads/main"
    advertised = subprocess.check_output([
        "git", "ls-remote", "https://github.com/securely1g/sonic-bazel-registry.git", ref,
    ], text=True).split()
    if len(advertised) != 2 or advertised[1] != ref:
        raise ValueError("Expected one maintained registry main revision")
    binary = prepare(Path.cwd(), Path.home(), args.output.resolve(), advertised[0], args.bazel)
    if os.environ.get("GITHUB_PATH"):
        with Path(os.environ["GITHUB_PATH"]).open("a") as output:
            print(binary.parent, file=output)


if __name__ == "__main__":
    main()
