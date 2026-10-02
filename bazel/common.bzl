"""Common build settings and small helpers for the optional Bazel build."""

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_import.bzl", "cc_import")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//bazel:packaging.bzl", "mtree_from_lines")

# Keep these flags aligned with configure.ac. The registered GCC toolchain
# supports -Wcast-align=strict, which configure probes before adding it.
CXXFLAGS_COMMON = [
    "-ansi",
    "-fPIC",
    "-pipe",
    "-std=c++14",
    "-Wall",
    "-Wcast-align",
    "-Wcast-qual",
    "-Wconversion",
    "-Wdisabled-optimization",
    "-Werror",
    "-Wextra",
    "-Wfloat-equal",
    "-Wformat=2",
    "-Wformat-nonliteral",
    "-Wformat-security",
    "-Wformat-y2k",
    "-Wimport",
    "-Winit-self",
    "-Wno-inline",
    "-Winvalid-pch",
    "-Wmissing-field-initializers",
    "-Wmissing-format-attribute",
    "-Wmissing-include-dirs",
    "-Wmissing-noreturn",
    "-Wno-aggregate-return",
    "-Wno-padded",
    "-Wno-switch-enum",
    "-Wno-unused-parameter",
    "-Wpacked",
    "-Wpointer-arith",
    "-Wredundant-decls",
    "-Wshadow",
    "-Wstack-protector",
    "-Wstrict-aliasing=3",
    "-Wswitch",
    "-Wswitch-default",
    "-Wunreachable-code",
    "-Wunused",
    "-Wvariadic-macros",
    "-Wwrite-strings",
    "-Wno-switch-default",
    "-Wconversion",
    "-Wno-psabi",
    "-Wcast-align=strict",
    # Distro CcInfo providers include optional directories from package
    # manifests. Some are absent in a particular architecture's payload.
    "-Wno-missing-include-dirs",
]

DBGFLAGS = select({
    "//:debug_enabled": ["-ggdb", "-DDEBUG"],
    "//conditions:default": ["-g"],
})

CODE_COVERAGE_CPPFLAGS = select({
    "//:coverage_enabled": ["-DNDEBUG"],
    "//conditions:default": [],
})

CODE_COVERAGE_CFLAGS = select({
    "//:coverage_enabled": ["-O0", "-fprofile-arcs", "-ftest-coverage"],
    "//conditions:default": [],
})

CODE_COVERAGE_CXXFLAGS = CODE_COVERAGE_CFLAGS

CODE_COVERAGE_LINKOPTS = select({
    "//:coverage_enabled": ["--coverage"],
    "//conditions:default": [],
})

PRODUCTION_COPTS = DBGFLAGS + CXXFLAGS_COMMON + CODE_COVERAGE_CPPFLAGS + CODE_COVERAGE_CXXFLAGS
TEST_COPTS = DBGFLAGS + CXXFLAGS_COMMON

CFLAGS_ASAN = select({
    "//:asan_enabled": [
        "-fsanitize=address",
        "-DASAN_ENABLED",
        "-ggdb",
        "-fno-omit-frame-pointer",
        "-U_FORTIFY_SOURCE",
        "-Wno-maybe-uninitialized",
    ],
    "//conditions:default": [],
})

ASAN_CORE_COPTS = select({
    "//:asan_enabled": ["-DASAN_ENABLED"],
    "//conditions:default": [],
})

LDFLAGS_ASAN = select({
    "//:asan_enabled": ["-lasan"],
    "//conditions:default": [],
})

RPC_COPTS = select({
    "//:rpcserver_enabled": ["-DSAITHRIFT=yes"],
    "//conditions:default": [],
})

RPC_DEPS = select({
    "//:rpcserver_enabled": ["//:rpcserver"],
    "//conditions:default": [],
})

VPP_COPTS = select({
    "//:vpp_enabled": ["-DUSE_VPP"],
    "//conditions:default": [],
})

VPP_DEPS = select({
    "//:vpp_enabled": ["//:vpp_sdk"],
    "//conditions:default": [],
})

VPP_COMPATIBLE = select({
    "//:vpp_enabled": [],
    "//conditions:default": ["@platforms//:incompatible"],
})

GTEST_DEPS = [
    "@com_google_googletest//:gtest",
    "@com_google_googletest//:gtest_main",
]

def _unavailable_cc_dependency_impl(ctx):
    fail(ctx.attr.message)

unavailable_cc_dependency = rule(
    implementation = _unavailable_cc_dependency_impl,
    attrs = {"message": attr.string(mandatory = True)},
    provides = [CcInfo],
)

def _sairedis_config_impl(ctx):
    output = ctx.actions.declare_file("config.h")
    ctx.actions.run(
        executable = ctx.executable._tool,
        arguments = [output.path, ctx.info_file.path, ctx.attr.platform],
        inputs = [ctx.info_file],
        outputs = [output],
        mnemonic = "SairedisConfig",
        progress_message = "Generating sairedis config.h",
    )
    return [DefaultInfo(files = depset([output]))]

sairedis_config = rule(
    implementation = _sairedis_config_impl,
    attrs = {
        "platform": attr.string(default = "generic"),
        "_tool": attr.label(
            default = "//:create_config",
            executable = True,
            cfg = "exec",
        ),
    },
)

def sairedis_shared_library(name, soname, consumer_name, deps, consumer_deps, linkopts = []):
    """Links a named DSO and exposes a CcInfo consumer that imports that DSO.

    consumer_deps must contain headers and runtime dependencies, never the
    component's static core. This prevents consumers from embedding another
    copy of the metadata globals or a second SAI C entry implementation.

    Args:
        name: Public alias for the linked shared library.
        soname: Runtime filename and ELF SONAME of the shared library.
        consumer_name: Name of the CcInfo target used by dynamic consumers.
        deps: Inputs linked into the shared library.
        consumer_deps: Header and runtime dependencies exposed to consumers.
        linkopts: Additional options for the shared-library link.
    """
    cc_binary(
        name = soname,
        deps = deps,
        linkopts = ["-Wl,-soname," + soname] + CODE_COVERAGE_LINKOPTS + linkopts,
        linkshared = True,
        linkstatic = True,
    )
    native.alias(name = name, actual = ":" + soname)
    cc_import(
        name = consumer_name + "_import",
        shared_library = ":" + soname,
    )
    cc_library(
        name = consumer_name,
        deps = [":" + consumer_name + "_import"] + consumer_deps,
    )

def multiarch_mtree(name, contents, data):
    """Creates the existing tar mtree interface for each supported CPU.

    Args:
        name: Prefix for the architecture-specific mtree targets.
        contents: Mtree lines, optionally containing a {multiarch} placeholder.
        data: Labels referenced by location expressions in contents.

    Returns:
        A select expression choosing the mtree target for the target CPU.
    """
    architectures = {
        "x86_64": "x86_64-linux-gnu",
        "aarch64": "aarch64-linux-gnu",
    }
    choices = {}
    for cpu, multiarch in architectures.items():
        target = name + "_" + cpu + "_mtree"
        mtree_from_lines(
            name = target,
            contents = [line.replace("{multiarch}", multiarch) for line in contents],
            data = data,
        )
        choices["@platforms//cpu:" + cpu] = ":" + target
    return select(choices, no_match_error = "sairedis Bazel packages currently support x86_64 and aarch64 targets")

def _working_directory_test_impl(ctx):
    runfiles = ctx.runfiles(files = [ctx.executable.program] + ctx.files.data)
    runfiles = runfiles.merge(ctx.attr.program[DefaultInfo].default_runfiles)
    for target in ctx.attr.data:
        runfiles = runfiles.merge(target[DefaultInfo].default_runfiles)

    # Imported DSOs retain paths relative to their original build location, and
    # ELF RUNPATH is not transitive. Use this test's declared runtime closure
    # for both direct and indirect dependencies, independent of host packages.
    library_dirs = sorted(depset([
        file.short_path.rsplit("/", 1)[0]
        for file in runfiles.files.to_list()
        if file.basename.endswith(".so") or ".so." in file.basename
    ]).to_list())

    # short_path is rooted at the main runfiles directory, while the requested
    # working directory belongs to the repository that declares this test.
    repository_prefix = "../" + ctx.label.repo_name + "/" if ctx.label.repo_name else ""
    script = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(
        output = script,
        content = """#!/bin/bash
set -euo pipefail
runfiles_root="${TEST_SRCDIR}/${TEST_WORKSPACE}"
export LD_LIBRARY_PATH="%s"
cd "${runfiles_root}/%s"
exec "${runfiles_root}/%s" "$@"
""" % (":".join(["${runfiles_root}/" + path for path in library_dirs]), repository_prefix + ctx.attr.working_directory, ctx.executable.program.short_path),
        is_executable = True,
    )
    return [DefaultInfo(executable = script, runfiles = runfiles)]

working_directory_test = rule(
    implementation = _working_directory_test_impl,
    attrs = {
        "program": attr.label(mandatory = True, executable = True, allow_files = True, cfg = "target"),
        "data": attr.label_list(allow_files = True),
        "working_directory": attr.string(mandatory = True),
    },
    test = True,
)

def _sairedis_swig_impl(ctx):
    staged_source = ctx.actions.declare_file(ctx.label.name + "_stage/pyext/pysairedis.i")
    staged_metadata = ctx.actions.declare_file(ctx.label.name + "_stage/SAI/meta/saiswig.i")
    ctx.actions.expand_template(template = ctx.file.src, output = staged_source, substitutions = {})
    ctx.actions.expand_template(template = ctx.file.sai_swig, output = staged_metadata, substitutions = {})

    swig_roots = [file.dirname for file in ctx.files.swig_library if file.basename == "swig.swg"]
    if len(swig_roots) != 1:
        fail("SWIG library must contain exactly one swig.swg")

    args = ctx.actions.args()
    args.add_all(["-Wall", "-c++", "-python", "-keyword"])
    if ctx.attr.wordsize64:
        args.add("-DSWIGWORDSIZE64")
    args.add("-o", ctx.outputs.wrapper.path)
    args.add("-outdir", ctx.outputs.python.dirname)

    header_sets = []
    include_dirs = {}
    for dep in ctx.attr.deps:
        context = dep[CcInfo].compilation_context
        header_sets.append(context.headers)
        for directory in context.includes.to_list() + context.quote_includes.to_list() + context.system_includes.to_list() + context.external_includes.to_list():
            include_dirs[directory] = True
    for directory in include_dirs:
        args.add("-I" + directory)
    args.add(staged_source.path)

    ctx.actions.run(
        executable = ctx.attr.swig[DefaultInfo].files_to_run,
        arguments = [args],
        inputs = depset(
            [ctx.file.src, ctx.file.sai_swig, staged_source, staged_metadata] + ctx.files.swig_library,
            transitive = header_sets,
        ),
        outputs = [ctx.outputs.wrapper, ctx.outputs.python],
        env = {"SWIG_LIB": swig_roots[0]},
        mnemonic = "SairedisSwig",
        progress_message = "Generating Python 3 sairedis bindings",
    )
    return [DefaultInfo(files = depset([ctx.outputs.wrapper, ctx.outputs.python]))]

sairedis_swig = rule(
    implementation = _sairedis_swig_impl,
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = [".i"]),
        "sai_swig": attr.label(mandatory = True, allow_single_file = [".i"]),
        "deps": attr.label_list(providers = [CcInfo]),
        "swig": attr.label(default = "@swig//:swig", executable = True, cfg = "exec"),
        "swig_library": attr.label(default = "@swig//:lib_python", allow_files = True, cfg = "exec"),
        "wordsize64": attr.bool(default = True),
        "wrapper": attr.output(mandatory = True),
        "python": attr.output(mandatory = True),
    },
)
