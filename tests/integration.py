#!/usr/bin/env python3
"""Exercise the real server against an isolated, disposable PostgreSQL cluster.

Usage: python3 tests/integration.py build/tiny_bin_server
Set TINYBIN_BROWSER_TESTS=1 to also run the React browser tests.
"""
import contextlib
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.parse
import uuid

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / 'build/tiny_bin_server'
def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def wait_for_http(port, path, process):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError('Test service exited before becoming ready')
        try:
            conn = http.client.HTTPConnection('127.0.0.1', port, timeout=.3)
            conn.request('GET', path)
            response = conn.getresponse()
            response.read()
            conn.close()
            if response.status == 200:
                return
        except (OSError, http.client.HTTPException):
            pass
        time.sleep(.05)
    raise RuntimeError('Test service did not become ready')


def stop_process(process):
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


class ApplicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.resources = contextlib.ExitStack()
        cls.addClassCleanup(cls.resources.close)
        cls.directory = Path(cls.resources.enter_context(tempfile.TemporaryDirectory(prefix='tinybin-test-')))
        cls.log = cls.resources.enter_context((cls.directory / 'services.log').open('w+'))
        pg_bin = Path(subprocess.check_output(['pg_config', '--bindir'], text=True).strip())
        cls.pg_ctl = str(pg_bin / 'pg_ctl')
        cls.data = cls.directory / 'data'
        subprocess.run([str(pg_bin / 'initdb'), '-D', str(cls.data), '-A', 'trust',
                        '--no-locale', '--encoding=UTF8', '-U', 'tinybin'], check=True, stdout=cls.log, stderr=cls.log)
        # Only a private Unix socket; no PostgreSQL TCP listener or existing DB is used.
        subprocess.run([cls.pg_ctl, '-D', str(cls.data), '-l', str(cls.directory / 'postgres.log'),
                        '-o', f"-k {cls.directory} -c listen_addresses=''", '-w', 'start'],
                       check=True, stdout=cls.log, stderr=cls.log)
        cls.resources.callback(lambda: subprocess.run(
            [cls.pg_ctl, '-D', str(cls.data), '-m', 'immediate', '-w', 'stop'], stdout=cls.log, stderr=cls.log))
        cls.port = free_port()
        cls.env = {**os.environ, 'DATABASE_URL': f'host={cls.directory} dbname=postgres user=tinybin connect_timeout=2',
                   'HOST': '127.0.0.1', 'PORT': str(cls.port), 'NUM_WORKERS': '2',
                   'PUBLIC_BASE_URL': 'http://localhost:5173'}
        subprocess.run(
            ['psql', f"--dbname={cls.env['DATABASE_URL']}", '-X', '-v', 'ON_ERROR_STOP=1',
             '-f', str(ROOT / 'src/sql/up.sql')],
            env=cls.env, check=True, stdout=cls.log, stderr=cls.log)
        cls.server = None
        cls.resources.callback(lambda: stop_process(cls.server))
        cls.start_server()

    @classmethod
    def start_server(cls):
        cls.server = subprocess.Popen([str(BINARY)], env=cls.env, stdout=cls.log, stderr=cls.log)
        try:
            wait_for_http(cls.port, '/readyz', cls.server)
        except Exception:
            cls.log.flush()
            print((cls.directory / 'services.log').read_text())
            raise

    def call(self, method, path='/api/v1/pastes', body=None, headers=None):
        headers = dict(headers or {})
        if isinstance(body, dict):
            body = json.dumps(body, ensure_ascii=False).encode('utf-8')
            headers.setdefault('Content-Type', 'application/json')
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=10)
        try:
            conn.request(method, path, body=body, headers=headers)
            response = conn.getresponse()
            payload = response.read()
            return response.status, dict(response.getheaders()), json.loads(payload) if payload and response.getheader('Content-Type', '').startswith('application/json') else payload
        finally:
            conn.close()

    def create(self, text='Hello TinyBin'):
        status, headers, paste = self.call('POST', body={'content': text})
        self.assertEqual(status, 201, paste)
        self.assertEqual(headers['Location'], paste['url'])
        self.assertEqual(uuid.UUID(paste['id']).version, 4)
        return paste

    def test_create_get_and_persistence(self):
        content = '<script>alert("test")</script>\nπ 🙂\n\tquotes: " \' \\ & < >'
        paste = self.create(content)
        path = '/api/v1/pastes?id=' + paste['id']
        self.assertEqual(self.call('GET', path)[2]['content'], content)
        stop_process(self.server)
        self.start_server()
        status, _, loaded = self.call('GET', path)
        self.assertEqual(status, 200)
        self.assertEqual(loaded['content'], content)
        self.assertEqual(loaded['id'], paste['id'])
        self.assertEqual(loaded['created_at'], paste['created_at'])
        self.assertEqual(self.call('PUT', path, {'content': 'Updated text'})[0], 405)
        self.assertEqual(self.call('DELETE', path)[0], 405)

    def test_validation_and_boundaries(self):
        for value in ('', None, 1, [], {}, 'nul\0byte'):
            self.assertEqual(self.call('POST', body={'content': value})[0], 400)
        for body in (b'{', b'[]', b'{}', b'{"content":"\xff"}', b'{"content":"x"}\0junk', b'{"content":"\\ud800"}'):
            self.assertEqual(self.call('POST', body=body, headers={'Content-Type': 'application/json'})[0], 400)
        self.assertEqual(self.call('POST', body=b'text')[0], 415)
        self.assertEqual(self.call('POST', body={'content': 'a' * 10241})[0], 413)
        self.assertEqual(self.call('POST', body={'content': '🙂' * 2561})[0], 413)
        paste = self.create('🙂' * 2560)
        path = '/api/v1/pastes?id=' + paste['id']
        self.assertEqual(self.call('GET', path)[2]['content'], '🙂' * 2560)

    def test_routes_and_readiness(self):
        self.assertEqual(self.call('GET', '/healthz')[0], 200)
        self.assertEqual(self.call('GET', '/readyz')[0], 200)
        self.assertEqual(self.call('GET', '/missing')[0], 404)
        for value in ('not-a-uuid', '', str(uuid.uuid4()) + '/extra', "' OR 1=1 --"):
            self.assertEqual(self.call('GET', '/api/v1/pastes?id=' + urllib.parse.quote(value))[0], 400)
        self.assertEqual(self.call('PATCH', '/api/v1/pastes?id=' + str(uuid.uuid4()))[0], 405)
        self.assertEqual(self.call('GET', '/api/v1/pastes')[0], 400)

    def test_fragmentation_and_pipelining(self):
        request = b'GET /healthz HTTP/1.1\r\nHost: localhost\r\n\r\n'
        with socket.create_connection(('127.0.0.1', self.port), timeout=5) as sock:
            for byte in request:
                sock.sendall(bytes([byte]))
            sock.sendall(request)
            sock.shutdown(socket.SHUT_WR)
            received = b''
            while chunk := sock.recv(4096):
                received += chunk
        self.assertEqual(received.count(b'HTTP/1.1 200 OK'), 2)
        self.assertEqual(received.count(b'\r\n\r\nok\n'), 2)

    def test_configuration_errors(self):
        for change in ({'DATABASE_URL': ''}, {'PORT': '65536'}, {'NUM_WORKERS': '0'},
                       {'HOST': 'invalid'}, {'PUBLIC_BASE_URL': 'javascript:alert(1)'}):
            result = subprocess.run([str(BINARY)], env={**self.env, **change}, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 1)
            self.assertNotIn(self.env['DATABASE_URL'].encode(), result.stderr)

    @unittest.skipUnless(os.getenv('TINYBIN_BROWSER_TESTS') == '1', 'Set TINYBIN_BROWSER_TESTS=1 for browser tests')
    def test_browser(self):
        client = ROOT / 'src/client/tiny-bin'
        frontend_port = free_port()
        env = {**os.environ, 'BACKEND_URL': f'http://127.0.0.1:{self.port}',
               'FRONTEND_HOST': '127.0.0.1', 'FRONTEND_PORT': str(frontend_port),
               'TEST_BASE_URL': f'http://127.0.0.1:{frontend_port}'}
        frontend = subprocess.Popen(['node', 'node_modules/vite/bin/vite.js'], cwd=client, env=env,
                                    stdout=self.log, stderr=self.log)
        try:
            wait_for_http(frontend_port, '/', frontend)
            subprocess.run(['npm', 'run', 'test:e2e'], cwd=client, env=env, check=True)
        finally:
            stop_process(frontend)

    def test_database_unavailable(self):
        # A missing application table must produce a bounded JSON 503, while health stays live.
        env = self.env
        subprocess.run(['psql', '--dbname=' + self.env['DATABASE_URL'], '-X', '-v', 'ON_ERROR_STOP=1', '-c', 'ALTER TABLE tiny_bin.pastes RENAME TO pastes_offline'],
                       env=env, stdout=self.log, stderr=self.log, check=True)
        try:
            self.assertEqual(self.call('GET', '/healthz')[0], 200)
            self.assertEqual(self.call('GET', '/readyz')[0], 503)
            self.assertEqual(self.call('POST', body={'content': 'x'})[0], 503)
        finally:
            subprocess.run(['psql', '--dbname=' + self.env['DATABASE_URL'], '-X', '-v', 'ON_ERROR_STOP=1', '-c', 'ALTER TABLE tiny_bin.pastes_offline RENAME TO pastes'],
                           env=env, stdout=self.log, stderr=self.log, check=True)


if __name__ == '__main__':
    unittest.main(verbosity=2)
