# Generated runtime package metadata

`//:runtime_packages.json` is generated from this repository's existing
`debian/control` and `debian/changelog`, the package-to-TAR mapping in
`BUILD.bazel`, and the exact source-built runtime payloads. Change package names,
descriptions and explicit dependency policy in the Debian files; there is no
second checked-in copy of those fields.

The shared `runtime_packages` rule computes shared-library dependencies from the
selected payloads and their declared source/APT dependency metadata. Its output
also carries the package owner's public runtime target. The consuming OCI rule
checks that target, hashes the actual TAR and reads the selected module version
for source provenance. Architecture and feature selection follow the Bazel
configuration. Unresolved dependency substitutions fail the build.

The rule generates metadata and library dependency information only. It does
not build Debian packages, run package installation scripts, or change the
legacy Make package recipes.

Build the metadata with `bazel build //:runtime_packages.json`. Native CI builds
this target alongside the corresponding runtime TARs.

For VS with `--//:enable_dashsai=True`, supply both the existing `--//:dash_sai`
SDK flag and `--//:dash_runtime_metadata=@sonic_dash//:runtime_packages.json`.
The metadata input describes the same source-built DASH library selected for
linking. The default VS build does not require the optional DASH module.
