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

`//:library_runtime_packages.json` exports just `libsairedis` and
`libsaimetadata` using those same original controls and source TARs. Its graph
does not require Syncd or VS SAI payloads. The full
`//:runtime_packages.json` combines that library interface with the Syncd/VS
package records, preserving the existing consumer interface.

Build either public metadata target with `bazel build`. Native CI builds the
full target alongside the corresponding runtime TARs and retains both JSON
outputs.

For VS with `--//:enable_dashsai=True`, supply both the existing `--//:dash_sai`
SDK flag and `--//:dash_runtime_metadata=@sonic_dash//:runtime_packages.json`.
The metadata input describes the same source-built DASH library selected for
linking. The default VS build does not require the optional DASH module.

`//syncd:syncd_pkg` includes the init script and its service links:
`etc/rc0.d/K01syncd`, `etc/rc6.d/K01syncd`, and `etc/rcS.d/S01syncd` point to
`../init.d/syncd`. These root-owned links match the script's start runlevel `S`
and stop runlevels `0` and `6`. They are also included when DASH is enabled,
so OCI consumers can install the TAR without recreating the links. Building
the TAR does not run package installation scripts or start a service.

## Review validation inputs

The tracked `.bazelrc` retains maintained SONiC main and BCR. This review's CI
uses the immutable prerequisite catalog
`a6b4e2e529e2635e9419393e3dccb652408aba25` through Bazel's alternate rc
mechanism, preserving the original build options and local caches. The catalog
contains the selected Common `.8-4359814`, shared infrastructure `.26-2cfc059`
and libnl `3.7.0.sonic.7` entries; the latter includes its SDK timestamp
normalization. Distroless `.16` is published. The generated registry receipt
records the selected catalog and both declared/effective rc hashes. No source
repository override is used.

The seven service-free tests run their ELF children through Common's declared
runtime, loader and patchelf interface. The test binary that dynamically links
Sairedis declares the two existing library TARs alongside Common’s runtime.
The original `saiasiccmp` shell fixture
keeps its command arguments, fixture files and exit status while each ELF child
gets its own runtime scratch directory. Its host Bash process does not inherit
the target library path. The existing manual Perl/Redis tests retain their
legacy launcher and remain outside this service-free acceptance scope.

For this review branch, CodeQL prepares only dependencies and generated sources
before tracer initialization. The successful preparation build's BEP supplies
the execution root; the trace stage then compiles the original complete owner
inventory. Finalized compilation records and archived source bytes must match
all 358 sources and 366 ownership pairs. This validates actual extraction,
separately from the configured source reachability checks.
