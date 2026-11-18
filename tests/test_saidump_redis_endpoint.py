"""Run saidump's shell flow with recorded CLI arguments and temporary output."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

import pytest


@pytest.fixture
def saidump_runner(tmp_path):
    output = tmp_path / "redis directory with spaces"
    output.mkdir()
    metadata = tmp_path / "database_config.json"
    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    log = tmp_path / "commands.jsonl"
    helper = "#!" + sys.executable + "\n" + '''
import json
import os
import sys
from pathlib import Path
command = Path(sys.argv[0]).name
if command != 'logger':
    with open(os.environ['SAIDUMP_TEST_LOG'], 'a') as log:
        log.write(json.dumps([command, *sys.argv[1:]]) + '\\n')
if command == 'sonic-db-cli':
    if os.environ.get('SAIDUMP_TEST_FAIL'):
        raise SystemExit(17)
    destination = sys.argv[sys.argv.index('--rdb') + 1]
    Path(destination).write_bytes(b'test snapshot')
elif command == 'rdb-cli':
    print('{}')
elif command == 'redis-cli':
    raise AssertionError('saidump must use sonic-db-cli for RDB export')
'''
    for command in ["sonic-db-cli", "rdb-cli", "saidump", "logger", "redis-cli"]:
        path = bin_dir / command
        path.write_text(helper)
        path.chmod(0o755)
    source = Path(__file__).resolve().parents[1] / "syncd/scripts/saidump.sh"
    script = tmp_path / "saidump.sh"
    script.write_text(source.read_text().replace(
        'local filepath="/var/run/redis/sonic-db/database_config.json"',
        "local filepath=" + shlex.quote(str(metadata))))

    def run(port, fail=False):
        metadata.write_text(json.dumps({
            "INSTANCES": {"redis": {
                "hostname": "127.0.0.2", "port": port,
                "unix_socket_path": str(output / "redis.sock"),
            }},
            "DATABASES": {"ASIC_DB": {"id": 1, "instance": "redis", "separator": ":"}},
        }))
        environment = {**os.environ, "PATH": str(bin_dir) + os.pathsep + os.environ["PATH"],
                       "SAIDUMP_TEST_LOG": str(log)}
        if fail:
            environment["SAIDUMP_TEST_FAIL"] = "1"
        result = subprocess.run(["bash", str(script)], env=environment,
                                capture_output=True, text=True, timeout=5)
        commands = [json.loads(line) for line in log.read_text().splitlines()]
        return result, commands, output

    return run


@pytest.mark.parametrize("port", [0, 6380])
def test_rdb_export_selects_unix_or_legacy_tcp(saidump_runner, port):
    result, commands, output = saidump_runner(port)
    assert result.returncode == 0, result.stderr
    transport = ["-s", "--authenticate"] if port == 0 else []
    assert commands == [
        ["sonic-db-cli", *transport, "--rdb", str(output / "dump.rdb"), "ASIC_DB"],
        ["rdb-cli", str(output / "dump.rdb"), "json"],
        ["saidump", "-r", str(output / "dump.json"), "-m", "100"],
    ]
    assert not (output / "dump.rdb").exists()
    assert not (output / "dump.json").exists()


@pytest.mark.parametrize("port", [0, 6380])
def test_rdb_export_failure_stops_saidump(saidump_runner, port):
    result, commands, _ = saidump_runner(port, fail=True)
    assert result.returncode != 0
    assert len(commands) == 1
    assert commands[0][0] == "sonic-db-cli"
