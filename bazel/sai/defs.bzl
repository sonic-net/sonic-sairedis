"""Declared sairedis API entry-stub actions."""

_PERL_TOOLCHAIN = "@rules_perl//perl:exec_toolchain_type"

def _source_manifest(ctx, files, marker, suffix):
    if not marker.path.endswith(suffix):
        fail("marker %s does not end with %s" % (marker.path, suffix))
    root = marker.path[:-len(suffix)]
    entries = []
    for source in files:
        if not source.path.startswith(root + "/"):
            fail("source %s is outside %s" % (source.path, root))
        entries.append({"source": source.path, "destination": source.path[len(root) + 1:]})
    manifest = ctx.actions.declare_file(ctx.label.name + ".inputs.json")
    ctx.actions.write(manifest, json.encode(entries))
    return manifest

def _sai_stub_impl(ctx):
    python = ctx.toolchains["@rules_python//python:toolchain_type"].py3_runtime
    if python == None or python.interpreter == None:
        fail("sairedis entry stubs require a declared Python 3 interpreter")
    perl = ctx.toolchains[_PERL_TOOLCHAIN].perl_runtime
    manifest = _source_manifest(ctx, ctx.files.srcs, ctx.file.marker, "/inc/sai.h")
    args = ctx.actions.args()
    args.add(ctx.file._generator)
    args.add("--manifest", manifest)
    args.add("--perl", perl.interpreter)
    args.add_all(perl.perlopt, before_each = "--perl-option")
    args.add("--stub", ctx.file.stub)
    args.add("--class-name", ctx.attr.class_name)
    args.add("--namespace", ctx.attr.namespace)
    args.add("--out", ctx.outputs.out)
    ctx.actions.run(
        executable = python.interpreter,
        arguments = [args],
        inputs = ctx.files.srcs + [ctx.file.stub, manifest],
        tools = depset([python.interpreter, perl.interpreter, ctx.file._generator], transitive = [python.files, perl.runtime]),
        outputs = [ctx.outputs.out],
        env = {"LANG": "C", "LC_ALL": "C", "PYTHONHASHSEED": "0"},
        mnemonic = "GenerateSaiStub",
        progress_message = "Generating %{label}",
    )
    return [DefaultInfo(files = depset([ctx.outputs.out]))]

sai_stub = rule(
    implementation = _sai_stub_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "marker": attr.label(allow_single_file = True, mandatory = True),
        "stub": attr.label(allow_single_file = True, mandatory = True),
        "class_name": attr.string(mandatory = True),
        "namespace": attr.string(mandatory = True),
        "out": attr.output(mandatory = True),
        "_generator": attr.label(default = Label("//bazel/sai:generate_stub.py"), allow_single_file = True, cfg = "exec"),
    },
    toolchains = [_PERL_TOOLCHAIN, "@rules_python//python:toolchain_type"],
)
