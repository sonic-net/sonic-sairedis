"""Run the tracked sairedis stub generator from declared SAI headers."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


_SHELL_READ = "    $DATA = `cat $optionSaiDir/inc/sai*.h $optionSaiDir/experimental/sai*.h`;"
_DECLARED_READ = """    my @headers = (sort(glob(\"$optionSaiDir/inc/sai*.h\")), sort(glob(\"$optionSaiDir/experimental/sai*.h\")));
    $DATA = \"\";
    for my $header (@headers)
    {
        open(my $input, \"<\", $header) or die \"cannot read $header: $!\";
        local $/;
        $DATA .= <$input>;
        close($input) or die \"cannot close $header: $!\";
    }"""


def main() -> None:
    parser = argparse.ArgumentParser()
    for name in ("manifest", "perl", "stub", "class-name", "namespace", "out"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--perl-option", action="append", default=[])
    args = parser.parse_args()
    inputs = json.loads(Path(args.manifest).read_text())
    sources = [(Path(entry["source"]).resolve(), entry["destination"]) for entry in inputs]
    perl = str(Path(args.perl).resolve())
    source = Path(args.stub).read_text()
    if source.count(_SHELL_READ) != 1:
        raise RuntimeError("stub.pl input reader changed; update the declared-input adapter")
    source = source.replace(_SHELL_READ, _DECLARED_READ)
    output = Path(args.out).resolve()
    with tempfile.TemporaryDirectory(prefix="sai-stub-") as temporary:
        root = Path(temporary) / "SAI"
        for input_file, destination in sources:
            target = root / destination
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(input_file, target)
        stub = Path(temporary) / "stub.pl"
        stub.write_text(source)
        output.parent.mkdir(parents=True, exist_ok=True)
        environment = {"LANG": "C", "LC_ALL": "C", "PATH": "", "TMPDIR": temporary}
        subprocess.run([
            perl, *args.perl_option, str(stub), "-d", str(root), "-c", args.class_name,
            "-n", args.namespace, "-f", str(output), "-s", "stub",
        ], cwd=temporary, check=True, env=environment)


if __name__ == "__main__":
    main()
