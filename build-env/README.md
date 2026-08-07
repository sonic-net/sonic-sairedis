# build-env/

Declarative build-environment configuration for sonic-sairedis, consumed by the
shared [`buildenv_setup`](https://github.com/sonic-net/sonic-swss-common/tree/master/ci)
tool (which lives in **sonic-swss-common**; sonic-sairedis is a consumer). This
is the single source of truth for "how to build this repo".

## Contents

| Path | Purpose | Cascades downstream? |
|------|---------|----------------------|
| `packages/base.yaml` | apt/pip packages needed to build **and link against** libsairedis (dbus/pcsc/hiredis/swig/zmq/autoconf-archive, arm libnl, redis + config) | **Yes** |
| `packages/tooling.yaml` | Build-stage-only tooling (docbook/aspell for man pages, rsyslog; sswsyncd dir) | No |
| `upstream-artifacts.yaml` | upstream DEBs: libswsscommon (sonic-swss-common), libnl+libyang3 (common-libs), vpp | Yes |
| `configure-redis-for-tests.sh` | redis test-config (no `notify-keyspace-events` — see F11), run via `post_install` | (travels with base.yaml) |
| `build.sh` | canonical build (`autogen` + `dpkg-buildpackage -Psyncd,vs,nopython2`), used by CI **and** local dev; `ASAN=true` for the asan build | — |
| `Dockerfile`, `compose.yaml` | local-dev image (CI does not use these) | — |

## How CI uses it

`.azure-pipelines/build-template.yml`, inside `container: sonic-slave-*`, clones
sonic-swss-common to get the tool and runs:

```bash
PYTHONPATH=/tmp/sw-common/ci python3 -m buildenv_setup \
    --repo-dir $(Build.SourcesDirectory) --scope build \
    --arch <arch> --debian-version <deb> --branch $(BUILD_BRANCH)
ASAN=<asan> ./build-env/build.sh
```

`buildenv_setup` installs the apt/pip packages, downloads + installs the
libswsscommon / libnl / libyang3 / vpp DEBs (from `upstream-artifacts.yaml`), and
runs the redis / sswsyncd / rsyslog post-install hooks. Then `build.sh` builds
the package.

## Local development

```bash
cd build-env
DEBIAN_VERSION=bookworm docker compose run --rm build   # build .debs
DEBIAN_VERSION=bookworm docker compose run --rm shell   # poke around
```

The image bakes the dependency setup; your source is mounted live at
`/workspace`, so ordinary edits don't require an image rebuild. Parity with CI
covers **building and C++ unit tests** (`make check`); the full VS/DVS test suite
needs CI-like infrastructure (KVM/privileged/nested-docker).

> **PoC note:** the tool currently lives on sonic-swss-common's `lawlee/ci-unify`
> branch, so the Dockerfile/CI clone is pinned there (`SWSS_COMMON_REF`), and the
> upstream artifacts + sonic-slave image are pinned to `master`. These TEMPORARY
> pins are removed once the change merges.
