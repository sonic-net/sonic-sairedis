# Building sonic-sairedis with Bazel

This guide covers the native Trixie VS profile built with Bazel. The existing
Make build produces Debian packages and covers the broader SONiC build matrix.

## Build configuration

The Bazel profile uses Debian Trixie userspace, matching native execution and
target architectures, the virtual SAI backend, Python 3 bindings, VPP disabled,
and swss-common's default configuration with YANG enabled. The workflow defines
native AMD64 and ARM64 jobs for this profile.

Run Bazel inside the matching Trixie userspace. The target and host platform
flags in `.bazelrc` select the pinned toolchains and declare the execution
environment. The commands assume Bash and Git are available for the workspace
status script.

| Configuration | Bazel selection |
| --- | --- |
| Trixie AMD64, VS, Python 3, VPP disabled, YANG enabled | Default target and host platforms |
| Trixie ARM64, VS, Python 3, VPP disabled, YANG enabled | `--config=aarch64` in Trixie ARM64 userspace |

Bookworm, ARMHF, vendor SAI, RPC, DASH SAI, and VPP continue to use the existing
Make workflows. The Bazel graph records external dependency inputs for vendor
SAI, RPC, DASH SAI, and VPP; those configurations are outside the Bazel profile
described here.

The registry module selects swss-common commit
`5ee19a9375e667c0d507239927745de8fa29be07`, including the merged
[YANG support in PR #6](https://github.com/securely1g/sonic-swss-common/pull/6).
It also includes the [external-consumer test fix](https://github.com/securely1g/sonic-swss-common/pull/9).
It generates `cfg_schema.h` from declared YANG inputs, preserving the default
feature set used by the Make reference.

The registered sonic-build-infra version
`0.0.13-ff408bafc35ce0e3d7a0bd27803debdc2a921de6` supplies declared Aspell and
Doxygen launchers, the C++ `-nostdinc` correction, CPU-specific hardening, and shared runtime-path behavior for native
AMD64 and ARM64. There are no component-owned source overrides or toolchain
patches. SAI headers, metadata generation, and its pinned execution tools come
from the `sai` registry module, version `1.18.0-sonic.1`.

The maintained registry `main` branch in `.bazelrc` includes merged
[Common registration](https://github.com/securely1g/sonic-bazel-registry/pull/16),
[SAI registration](https://github.com/securely1g/sonic-bazel-registry/pull/17),
and [shared launcher registration](https://github.com/securely1g/sonic-bazel-registry/pull/27).
Common's selected merge commit has the same source tree as the prior snapshot;
SAI retains its API source and generated-output baselines. Source revisions and
archive integrity remain pinned while SAI uses the shared declared execution tools.

The optional `--@sonic_swss_common//tools/bazel:yang_modules=False` setting
omits swss-common's decorator and default value provider implementations.
The commands below keep YANG enabled by omitting that override.

The repository pins Bazel 8.5.1 in `.bazelversion`. Use Bazelisk to select that
version. The module versions in `MODULE.bazel` and registry selection in
`.bazelrc` provide the compiler, sysroot, native dependencies, and build tools.

## Dependency inputs and generated resolution

The root `MODULE.bazel.lock` is generated locally and ignored by Git. A clean
checkout starts without this file; the commands below use
`--lockfile_mode=update` to generate it as dependencies are resolved. This is
the repository's lockfile policy. Keep the checked-in module versions,
source revisions, integrity hashes, and shared toolchain and package snapshots
pinned when changing dependencies. Use the maintained registry `main` URL in
`.bazelrc` for dependency resolution.

Each native CI job is configured to retain its generated resolution in
`artifacts/validation/dependencies/`: `MODULE.bazel.lock`, `dependency-graph.json`,
and a `manifest.json` identifying the checked-out revision, architecture,
configuration, and evidence hashes. The graph covers the configured dependencies
of the required component, test, and package targets. These files record the resolution observed
by that job. The checked-in declarations describe the intended inputs; inspect
the generated evidence from the relevant run to confirm what was selected.
Archiving a lockfile does not make an otherwise mutable input reproducible.

## Build the component

From the repository root inside Trixie AMD64 userspace:

```sh
bazel build --lockfile_mode=update //pyext:python3_files //syncd:syncd
```

These targets request the metadata, Redis, and VS shared libraries, the Python
3 extension and module files, and the VS sync daemon. Test execution is a
separate step.

Useful individual targets include:

| Target | Output |
| --- | --- |
| `//meta:saimetadata` | Metadata library with SONAME `libsaimetadata.so.0` |
| `//meta:saimeta` | Meta library with SONAME `libsaimeta.so.0` |
| `//lib:sairedis` | Redis library with SONAME `libsairedis.so.0` |
| `//vslib:libsaivs_shared` | VS library with SONAME `libsaivs.so.0` |
| `//pyext:pysairedis_py3` | Python 3 native extension |
| `//syncd:syncd` | VS sync daemon |

C++ consumers should use the corresponding `saimetadata_shared`,
`saimeta_shared`, `sairedis_shared`, or `saivs_shared` target. These targets
provide headers and dynamic linkage. The build keeps generated SAI entry
points separate from the C++ cores because the Redis, VS, and proxy backends
export the same SAI C function names.

Build the larger `//:canonical_outputs` target for the diagnostic and test
binaries from the Make graph with VPP disabled, including the TestDash sources.
The workflow requires this aggregate and checks its configured source ownership
against 358 reviewed component source paths and 366 target/source pairs.

## Tests without external services

Run the following seven existing tests without external services:

```sh
bazel test --lockfile_mode=update --nocache_test_results --test_output=errors \
  //lib:tests_test \
  //tests:tests_test \
  //saiasiccmp:tests \
  //unittest/meta:tests \
  //unittest/proxylib:tests \
  //unittest/saidump:tests \
  //unittest/saisdkdump:tests
```

The existing `//vslib:tests` executable requires Redis and is outside this
seven-test set. Run it in an isolated environment with a disposable Redis
instance. Coverage of its kernel network setup requires the corresponding
network capabilities and successful setup operations; the process exit alone
does not establish that coverage when setup reports errors. The existing Azure
workflow retains the wider service, capability, ASAN, and downstream VS tests.

## Runtime, development, and debug archives

The Bazel package targets produce tar archives for later image assembly. Debian
packages still use Make.

| Component | Runtime archive | Development archive | Detached symbols |
| --- | --- | --- | --- |
| Redis SAI | `//lib:libsairedis_pkg` | `//lib:libsairedis_dev_pkg` | `//lib:libsairedis_pkg.debug_symbols` |
| Metadata and meta | `//meta:libsaimetadata_pkg` | `//meta:libsaimetadata_dev_pkg` | `//meta:libsaimetadata_pkg.debug_symbols` |
| Virtual SAI | `//vslib:libsaivs_pkg` | `//vslib:libsaivs_dev_pkg` | `//vslib:libsaivs_pkg.debug_symbols` |
| Python 3 binding | `//pyext:pysairedis_pkg` | — | `//pyext:pysairedis_pkg.debug_symbols` |
| VS syncd and helpers | `//syncd:syncd_pkg` | — | `//syncd:syncd_pkg.debug_symbols` |

For example, build the Redis runtime, headers, and symbols with:

```sh
bazel build --lockfile_mode=update \
  //lib:libsairedis_pkg \
  //lib:libsairedis_dev_pkg \
  //lib:libsairedis_pkg.debug_symbols
```

Use `cquery` to find an artifact in the selected configuration:

```sh
bazel cquery --lockfile_mode=update --output=files //lib:libsairedis_pkg
bazel cquery --lockfile_mode=update --output=files //lib:libsairedis_pkg.debug_symbols
```

The three development `.debug_symbols` labels alias their component's runtime
symbol archive. The 16 public package labels therefore map to 13 unique tar
files.

Each package uses a shared transition that adds `--copt=-g`,
`--strip=never`, and a linker build ID to the linked ELF used for packaging.
The packaging actions derive both a stripped runtime copy and a detached symbol
file from that same ELF. Symbols are stored under
`usr/lib/debug/.build-id/<two digits>/<remaining digits>.debug`, and the runtime
file carries a matching `.gnu_debuglink`. These packaging flags do not change
the default flags of a separately requested raw library target.

The package verifier requires all 14 expected runtime/debug pairs to have
matching build IDs and debuglink CRCs, and requires GDB to load the detached
libsairedis symbols and resolve `sai_api_initialize` to a source line. To inspect
an extracted pair, configure GDB's debug directory before loading the runtime
file:

```text
(gdb) set debug-file-directory /path/to/staging/usr/lib/debug
(gdb) file /path/to/staging/usr/lib/x86_64-linux-gnu/libsairedis.so.0.0.0
(gdb) info line sai_api_initialize
```

The symbol archives do not bundle source files. Source listing requires the
matching checkout at the recorded path or an appropriate GDB source-path
mapping.

The verifier reports inherited archive metadata separately from explicit
component requirements. Known differences from Make include inherited `0755`
modes for detached symbol files and 123 metadata/VS API headers, which Make
installs as `0644`. It also reports the actual UID/GID on debug archive root
directory entries, whose ownership the component does not explicitly set. Use
the artifact report for the values produced by each run; complete Make metadata
parity requires separate validation.

## Runtime lookup

The standalone `.bazelrc` enables ELF `RUNPATH`. Each component test launcher
sets `LD_LIBRARY_PATH` to the shared-library directories in that test's declared
Bazel runfiles, replacing any inherited value. This also resolves indirect
dependencies such as ZeroMQ's libsodium dependency: ELF `RUNPATH` alone is not
transitive, and imported component DSOs no longer occupy their original build
directories. The tests exercise the raw linked outputs without installing
component libraries on the runner or scanning host library directories.

Downstream Bazel roots use their own `.bazelrc` and launchers. Deployed package
lookup and a complete installed SONiC runtime require their own validation;
these component tests do not establish that an installed image boots.

## Continuous integration

The Bazel workflow defines `Component (AMD64)` on `ubuntu-24.04` and
`Component (ARM64)` on `ubuntu-24.04-arm`, each inside a pinned Debian Trixie
container. Each job is configured to verify its native architecture, check
Bazel formatting, build `//:canonical_outputs`, run the seven tests listed
above, check configured source ownership against the reviewed Make inventory,
and build all 16 public package labels. The package verifier checks their 13
unique tar outputs and the 14 runtime/debug pairs. The workflow keeps the
default YANG setting and verifies that its selected value is enabled.

The jobs generate the ignored root lockfile in update mode and retain the
resolved dependency graph and manifest for each architecture with the validation
artifacts. A clean checkout must build without a pre-existing lockfile, and
dependency resolution must leave tracked source files unchanged.

The source check requires 358 distinct component translation units and 366
reviewed target/source pairs. It reports configuration identities and excludes
external dependency actions from component coverage. Changes to Make source
declarations require a reviewed inventory update. This query checks configured
graph membership; it does not establish compiler tracing or CodeQL coverage.

The package verifier checks the declared component layout, ELF identity,
runtime/debug pairing, and GDB lookup. It reports inherited archive metadata
and differences from the Make reference separately. Its success does not claim
complete package metadata parity or deployed runtime closure.

When a run produces uploads, find them in the **Artifacts** section of its
Actions run page. In the names below, `<arch>` is `AMD64` or `ARM64`, and
`<workflow SHA>` is GitHub's `github.sha` value for that run.

| Artifact name | Contents | Upload behavior |
| --- | --- | --- |
| `sonic-sairedis-bazel-validation-<arch>-<workflow SHA>` | Available files from `artifacts/validation/`, including environment and validation reports and generated dependency evidence under `dependencies/` | Attempted on every job outcome; no matching files produce a warning |
| `sonic-sairedis-bazel-trixie-<arch>-<workflow SHA>` | The 13 unique tar files from `artifacts/packages/`, `artifacts/provenance.json`, and the matching `artifacts/validation/dependencies/` evidence | Runs after preceding steps succeed; no matching files are an error |

Use the job results and downloaded artifacts from the relevant Actions run to
confirm which required checks and outputs succeeded. The existing Azure and
CodeQL workflows continue to use Make.

## Build modes

The component flags mirror the corresponding `configure` choices:

| Flag | Effect |
| --- | --- |
| `--//:enable_debug` | Adds `-ggdb` and `DEBUG` |
| `--//:enable_coverage` | Adds the legacy coverage instrumentation and `NDEBUG` |
| `--//:enable_asan` | Applies the legacy syncd AddressSanitizer selection |

Debug and coverage are mutually exclusive, matching `configure.ac`. The ASAN
selection instruments the syncd executable sources and enables the syncd core
define, matching Make's instrumentation scope. These optional modes need
separate validation outside the default workflow profile.

## Generated SAI inputs

The Bazel build consumes an immutable SAI source archive matching the tracked
`SAI` gitlink. It declares metadata generation, the Redis/VS/proxy SAI entry
stubs, and the Python wrapper as build actions. Generated files stay in Bazel's
output tree.

The SAI registry module owns the attribute-version header and its source/tag
provenance, preserving the version filtering data that upstream `attrversion.sh`
obtains from Git history. Its source remains
`6dd738196ebb267be458eed0a00b687568455914`, matching the Make submodule. Update
the module and its provenance together whenever that source changes. The
component retains only its Redis/VS/proxy entry stubs and Python integration.

## Debian packages

For the VS and Python 3 profile, use the existing Make packaging flow:

```sh
./autogen.sh
dpkg-buildpackage -us -uc -b -Psyncd,vs,nopython2 -j"$(nproc)"
```

The build environment controls optional dependency detection such as VPP.
Use the repository's documented SONiC build environment for the required
distribution and target architecture.
