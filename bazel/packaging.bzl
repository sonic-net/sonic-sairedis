"""Helpers for creating packaging artifacts."""

load("@bazel_lib//lib:expand_template.bzl", "expand_template")
load("@sonic_build_infra//tar:sonic_deploy_tar.bzl", "sonic_deploy_tar")

def mtree_from_lines(name, contents, data, **kwargs):
    """Creates a named mtree file target from a list of mtree specification lines.

    Args:
        name: name of the target; output file will be `[name].txt`.
        contents: list of mtree specification lines. Values may use $(location)
            for files listed in data.
        data: list of file labels referenced via $(location) in contents.
        **kwargs: additional arguments forwarded to expand_template.
    """
    expand_template(
        name = name,
        out = name + ".txt",
        data = data,
        substitutions = {
            "{content}": "\n".join(contents),
        },
        template = ["#mtree", "{content}", ""],
        **kwargs
    )

def multiarch_deploy_tar(name, binaries, srcs = [], mtree = [], visibility = None):
    """Splits packaged ELFs and selects the native Trixie install path.

    sonic_deploy_tar requires literal paths, so each supported architecture has
    its own target. Callers can use {multiarch} in the binary and mtree paths.
    The public runtime and .debug_symbols labels select the same architecture.

    Args:
        name: Name of the public runtime target and prefix for its symbol target.
        binaries: Mapping from mtree file specifications to linked ELF targets.
        srcs: Additional package inputs referenced by the mtree lines.
        mtree: Additional package entries, optionally containing {multiarch}.
        visibility: Visibility of the public targets.
    """
    runtime_targets = {}
    symbol_targets = {}
    for cpu, multiarch in [
        ("x86_64", "x86_64-linux-gnu"),
        ("aarch64", "aarch64-linux-gnu"),
    ]:
        target = name + "_" + cpu
        sonic_deploy_tar(
            name = target,
            binaries = {
                path.replace("{multiarch}", multiarch): binary
                for path, binary in binaries.items()
            },
            srcs = srcs,
            mtree = [line.replace("{multiarch}", multiarch) for line in mtree],
            force_debug_build = True,
            target_compatible_with = ["@platforms//cpu:" + cpu],
            visibility = visibility or ["//visibility:private"],
        )
        constraint = "@platforms//cpu:" + cpu
        runtime_targets[constraint] = ":" + target
        symbol_targets[constraint] = ":" + target + ".debug_symbols"

    native.alias(
        name = name,
        actual = select(runtime_targets),
        visibility = visibility,
    )
    native.alias(
        name = name + ".debug_symbols",
        actual = select(symbol_targets),
        visibility = visibility,
    )
