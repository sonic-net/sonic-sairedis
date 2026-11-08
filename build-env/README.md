# build-env/

Declarative build-environment configuration for sonic-sairedis, consumed by the
shared [`buildenv_setup`](https://github.com/sonic-net/sonic-swss-common/tree/master/ci)
tool in **sonic-swss-common**. This is the single source of truth for setting up
the repo's build dependencies.

## Contents

| Path | Purpose | Cascades downstream? |
|------|---------|----------------------|
| `packages/base.yaml` | apt packages needed to build/link libsairedis; arm libnl-cli; shared Redis test configuration | **Yes** |
| `packages/tooling.yaml` | Build-only tooling (docbook/aspell, rsyslog, sswsyncd runtime directory) | No |
| `upstream-artifacts.yaml` | common-libs (amd64 SONiC libnl + libyang), sonic-swss-common, and VPP DEBs | Yes |
| `build.sh` | canonical build (`autogen` + `dpkg-buildpackage -Psyncd,vs,nopython2`), used by CI and local dev; `ASAN=true` selects ASAN | — |
| `Dockerfile`, `compose.yaml` | local-dev image and commands (CI does not use these) | — |

The Redis script body is owned by sonic-swss-common at
`build-env/configure-redis-for-tests.sh`. Sairedis selects that same script for
its Build scope; `buildenv_setup` resolves it from the cascaded swss-common
bundle, so the setup is not duplicated.

## How CI uses it

`.azure-pipelines/build-template.yml`, inside `container: sonic-slave-*`, clones
sonic-swss-common to get the branch-versioned tool and runs:

```bash
PYTHONPATH=/tmp/sw-common/ci python3 -m buildenv_setup \
    --repo-dir $(Build.SourcesDirectory) --scope build \
    --arch <arch> --debian-version <deb> --branch $(BUILD_BRANCH)
ASAN=<asan> ./build-env/build.sh
```

`buildenv_setup` installs apt dependencies, resolves and installs the
common-libs / libswsscommon / VPP DEBs, walks swss-common's published
`build-env/` cascade, and runs the Redis / sswsyncd / rsyslog hooks. The VPP
install uses `VPP_INSTALL_SKIP_SYSCTL=1` for its maintainer scripts.

## Local development

```bash
cd build-env
DEBIAN_VERSION=bookworm docker compose run --rm build
DEBIAN_VERSION=bookworm docker compose run --rm shell
```

From the shell, run the same post-build unit-test sequence as CI:

```bash
setcap "cap_sys_time=eip" syncd/.libs/syncd_tests
setcap "cap_dac_override,cap_ipc_lock,cap_ipc_owner,cap_sys_time=eip" \
    unittest/syncd/.libs/tests
make check
```

The image bakes dependency setup while the source tree is mounted live at
`/workspace`, so ordinary edits do not require an image rebuild. Set
`SWSS_COMMON_REF` and `BUILD_BRANCH` when testing matching non-master branches.
The `build` service reproduces CI's dependency setup and package build. Use the
`shell` service to run CI's post-build `setcap` + `make check` sequence when
testing C++ changes. The full VS/DVS suite still needs CI-like
KVM/privileged/nested-docker infrastructure.
