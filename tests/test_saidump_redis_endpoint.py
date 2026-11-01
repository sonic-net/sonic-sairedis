"""Exercise the native RDB export dispatch without starting a DUT."""
from pathlib import Path
import sys
from types import ModuleType
from unittest.mock import Mock

import pytest


@pytest.mark.parametrize('port', ['0', '6380'])
def test_export_uses_native_socket_or_explicit_legacy_tcp(monkeypatch, port):
    import subprocess
    source = Path(__file__).resolve().parents[1] / 'syncd/scripts/saidump.sh'
    program = source.read_text().split("<<'PY'\n", 1)[1].split('\nPY', 1)[0]
    native = ModuleType('swsscommon.swsscommon')
    native.exportRedisRdb = Mock()
    package = ModuleType('swsscommon')
    monkeypatch.setitem(sys.modules, 'swsscommon', package)
    monkeypatch.setitem(sys.modules, 'swsscommon.swsscommon', native)
    monkeypatch.setattr(sys, 'argv', ['-', '127.0.0.2', port, '/run/redis/socket with spaces', '/tmp/export file.rdb'])
    execute = Mock()
    monkeypatch.setattr(subprocess, 'run', execute)
    exec(compile(program, str(source), 'exec'), {})
    if port == '0':
        native.exportRedisRdb.assert_called_once_with('/run/redis/socket with spaces', '/tmp/export file.rdb', 60000)
        execute.assert_not_called()
    else:
        native.exportRedisRdb.assert_not_called()
        assert execute.call_args.args[0] == ['redis-cli', '-h', '127.0.0.2', '-p', '6380', '--rdb', '/tmp/export file.rdb']
        assert execute.call_args.kwargs['check'] is True
