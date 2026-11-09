"""Deterministic acquisition, lifetime and legacy-lock regression tests."""
import fcntl
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RepoLockTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='repo-lock-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.env = {key: value for key, value in os.environ.items() if not key.startswith('EDK2_CIX_')}
        self.env['EDK2_CIX_BUILD_LOCK_ROOT'] = str(self.root)

    def wait_file(self, path):
        deadline = time.monotonic() + 5
        while not path.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertTrue(path.exists(), str(path))

    def call(self, *command, **env):
        return subprocess.run([str(ROOT / 'scripts/with_repo_lock.sh'), *command],
                              env={**self.env, **env}, capture_output=True, text=True, timeout=8)

    def holder(self, before_metadata=False):
        ready, release = self.root / 'ready', self.root / 'release'
        ready.unlink(missing_ok=True)
        release.unlink(missing_ok=True)
        if before_metadata:
            # Pause at the exact historical race window, after exclusive
            # acquisition but before publishing any owner metadata.
            code = '''import repo_lock, time
from pathlib import Path
original = repo_lock.write_owner
def paused(fd, command):
    Path('ready').touch()
    while not Path('release').exists(): time.sleep(.01)
    original(fd, command)
repo_lock.write_owner = paused
raise SystemExit(repo_lock.run(['true']))
'''
            command = [sys.executable, '-c', code]
        else:
            code = "from pathlib import Path; import time; Path('ready').touch()\nwhile not Path('release').exists(): time.sleep(.01)"
            command = [str(ROOT / 'scripts/with_repo_lock.sh'), sys.executable, '-c', code]
        process = subprocess.Popen(command, cwd=self.root,
                                   env={**self.env, 'PYTHONPATH': str(ROOT / 'scripts')},
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.finish, process)
        self.wait_file(ready)
        return process

    def finish(self, process):
        (self.root / 'release').touch()
        process.communicate(timeout=5)

    def test_owner_publication_window_is_exclusive(self):
        process = self.holder(before_metadata=True)
        lock = self.root / 'repo-build.lock'
        inode = lock.stat().st_ino
        self.assertEqual(lock.read_bytes(), b'')
        self.assertEqual(self.call('true').returncode, 1)
        self.assertEqual(lock.stat().st_ino, inode)
        self.finish(process)
        self.assertEqual(process.returncode, 0)
        self.assertEqual(self.call('true').returncode, 0)
        self.assertEqual(lock.stat().st_ino, inode)

    def test_parallel_waiters_serialize_and_cleanup_keeps_inode(self):
        marker = self.root / 'critical'
        log = self.root / 'log'
        code = f'''import os, time
fd = os.open({str(marker)!r}, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
time.sleep(.02)
os.close(fd)
os.unlink({str(marker)!r})
with open({str(log)!r}, 'a') as f: f.write('done\\n')
'''
        processes = [subprocess.Popen([str(ROOT / 'scripts/with_repo_lock.sh'), sys.executable, '-c', code],
                                      env={**self.env, 'EDK2_CIX_BUILD_LOCK_WAIT': '1'},
                                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE) for _ in range(8)]
        for process in processes:
            _, error = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 0, error)
        self.assertEqual(log.read_text().splitlines(), ['done'] * 8)
        self.assertTrue((self.root / 'repo-build.lock').exists())

    def test_crash_before_metadata_releases_kernel_ownership(self):
        process = self.holder(before_metadata=True)
        process.kill()
        process.communicate(timeout=5)
        self.assertEqual(self.call('true').returncode, 0)

    def test_killed_wrapper_keeps_surviving_command_protected(self):
        process = self.holder()
        process.kill()
        self.assertEqual(self.call('true').returncode, 1)
        self.finish(process)
        self.assertEqual(self.call('true').returncode, 0)

    def test_cleanup_does_not_delete_a_replacement_lock(self):
        process = self.holder()
        path = self.root / 'repo-build.lock'
        path.rename(self.root / 'original')
        with path.open('w') as replacement:
            fcntl.flock(replacement, fcntl.LOCK_EX)
            inode = path.stat().st_ino
            self.finish(process)
            self.assertEqual(path.stat().st_ino, inode)
            self.assertEqual(self.call('true').returncode, 1)

    def test_legacy_live_foreign_incomplete_and_abandoned_owners(self):
        directory = self.root / 'repo-build.lock.d'
        directory.mkdir()
        self.assertEqual(self.call('true').returncode, 1)
        owner = directory / 'owner'
        owner.write_text(f'host={socket.gethostname()}\npid={os.getpid()}\n')
        self.assertEqual(self.call('true').returncode, 1)
        owner.write_text('host=other-host\npid=99999999\n')
        self.assertEqual(self.call('true').returncode, 1)
        owner.write_text(f'host={socket.gethostname()}\npid=99999999\n')
        self.assertEqual(self.call('true', EDK2_CIX_BUILD_LOCK_BREAK_STALE='0').returncode, 1)
        self.assertEqual(self.call('true').returncode, 0)
        self.assertTrue(owner.exists())  # No deletion of a possibly replaced path.

    def test_exit_status_and_signal_cleanup(self):
        self.assertEqual(self.call('sh', '-c', 'exit 17').returncode, 17)
        process = self.holder()
        process.send_signal(signal.SIGTERM)
        process.communicate(timeout=5)
        self.assertEqual(process.returncode, 143)
        self.assertEqual(self.call('true').returncode, 0)
        self.assertEqual(self.call('true', EDK2_CIX_BUILD_LOCK_WAIT='invalid').returncode, 2)


if __name__ == '__main__':
    unittest.main()
