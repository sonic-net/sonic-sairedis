"""Check the native RDB export arguments without starting a DUT."""

from pathlib import Path
import sys
from types import ModuleType
from unittest.mock import Mock

import pytest


@pytest.mark.parametrize('port', ['0', '6380'])
@pytest.mark.parametrize('password', [None, 'fixture-secret', b'fixture-secret'])
def test_export_preserves_endpoint_and_keeps_password_off_argv(monkeypatch, port, password):
    import subprocess

    source = Path(__file__).resolve().parents[1] / 'syncd/scripts/saidump.sh'
    program = source.read_text().split("<<'PY'\n", 1)[1].split('\nPY', 1)[0]
    auth = ModuleType('sonic_py_common.redis_auth')
    credentials = {} if password is None else {'username': 'writer', 'password': password}
    auth.connection_kwargs = Mock(return_value=credentials)
    package = ModuleType('sonic_py_common')
    monkeypatch.setitem(sys.modules, 'sonic_py_common', package)
    monkeypatch.setitem(sys.modules, 'sonic_py_common.redis_auth', auth)
    monkeypatch.setattr(sys, 'argv', ['-', '127.0.0.2', port, '/run/redis/socket with spaces', '/tmp/export file.rdb'])
    execute = Mock()
    monkeypatch.setattr(subprocess, 'run', execute)
    exec(compile(program, str(source), 'exec'), {})
    command = execute.call_args.args[0]
    options = execute.call_args.kwargs
    assert command[-2:] == ['--rdb', '/tmp/export file.rdb']
    assert options['check'] is True
    if port == '0':
        assert command[:3] == ['redis-cli', '-s', '/run/redis/socket with spaces']
        auth.connection_kwargs.assert_called_once_with(unix_socket_path='/run/redis/socket with spaces')
        if password is not None:
            assert command[3:6] == ['--user', 'writer', '--askpass']
            assert options['input'] == b'fixture-secret\n'
            assert 'fixture-secret' not in ' '.join(command)
        else:
            assert '--askpass' not in command and options['input'] is None
    else:
        assert command[:5] == ['redis-cli', '-h', '127.0.0.2', '-p', '6380']
        auth.connection_kwargs.assert_not_called()
        assert '--askpass' not in command and options['input'] is None
