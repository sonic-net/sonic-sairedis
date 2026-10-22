# Runtime package metadata

`//:runtime_packages.json` describes the Debian package identities represented by
this module's source-built runtime TARs in SONiC Trixie images. An image consumes
the metadata with the corresponding TAR and the selected `MODULE.bazel`. For
example, it can record the library's version and dependencies without copying
those facts into its own package inventory.

The schema is `1`; `module` names the Bazel source module, and `packages` maps
Debian package names to `runtime_target` (a label relative to this repository)
and `control_fields`. Keep these fields aligned with the source package's
version, runtime dependencies and selected features when changing the producer.
The image supplies its required paths/modes separately.

There is deliberately no frozen source commit, machine architecture, installed
size, conffile checksum or installed status in this file. The consuming rule
checks the actual TAR target and payload hash and reads the selected module
version to record source provenance. It determines the target architecture and
installation state from the build and image. A module version with a commit
suffix identifies that source commit; a plain version is not an exact Git SHA.

This metadata does not create a DEB, run maintainer scripts or install a package.
The image owner must still preserve and validate required installation behavior.
The supported consumer configuration is SONiC Trixie on AMD64 and ARM64.
