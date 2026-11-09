#!/usr/bin/env python3
"""Hold a persistent-inode POSIX advisory lock for a build command.

Never unlink the lock file: contenders must always lock the same inode. Metadata
is diagnostic only and is written *after* flock, so interrupted publication is
safe. The command inherits the descriptor, retaining exclusion if this wrapper
is killed while the command is still running. Supported on macOS and Linux.
"""
from __future__ import annotations

import errno
import fcntl
import json
import os
from pathlib import Path
import shlex
import signal
import socket
import subprocess
import sys
import time


def wait_seconds(value: str) -> int:
    if value.lower() in {'', '0', 'false', 'no'}:
        return 0
    if value.lower() in {'1', 'true', 'yes'}:
        return -1
    if value.isascii() and value.isdigit():
        return int(value)
    raise ValueError('Unsupported EDK2_CIX_BUILD_LOCK_WAIT; use 0, 1, or a number of seconds.')


def legacy_busy(directory: Path, break_stale: bool) -> bool:
    """Respect retained mkdir locks; incomplete ownership is never stale proof.

    Abandoned legacy directories are left intact. A new wrapper never deletes a
    path that an older process might have replaced after an ownership check.
    """
    if not directory.exists():
        return False
    try:
        owner = dict(line.split('=', 1) for line in (directory / 'owner').read_text().splitlines()
                     if '=' in line)
        pid = int(owner['pid'])
        if not break_stale or owner['host'] != socket.gethostname() or pid <= 0:
            return True
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except (OSError, ValueError, KeyError):
        return True
    return True


def write_owner(fd: int, command: list[str]) -> None:
    owner = dict(pid=os.getpid(), host=socket.gethostname(), cwd=os.getcwd(),
                 command=shlex.join(command), started=time.time())
    os.lseek(fd, 0, os.SEEK_SET)
    os.ftruncate(fd, 0)
    os.write(fd, (json.dumps(owner) + '\n').encode())


def run(command: list[str]) -> int:
    if not command:
        print('usage: scripts/with_repo_lock.sh <command> [args...]', file=sys.stderr)
        return 2
    if os.environ.get('EDK2_CIX_REPO_LOCK_HELD') == '1':
        os.execvp(command[0], command)
    default_root = Path(__file__).resolve().parents[1] / '.buildbox/locks'
    root = Path(os.environ.get('EDK2_CIX_BUILD_LOCK_ROOT', str(default_root)))
    name = os.environ.get('EDK2_CIX_BUILD_LOCK_NAME', 'repo-build')
    if not name or Path(name).name != name or name in {'.', '..'}:
        raise ValueError('lock name must be a single path component')
    timeout = wait_seconds(os.environ.get('EDK2_CIX_BUILD_LOCK_WAIT', '0'))
    root.mkdir(parents=True, exist_ok=True)
    # Persistent file, deliberately never removed (including on normal exit).
    fd = os.open(root / (name + '.lock'), os.O_CREAT | os.O_RDWR, 0o600)
    start = time.monotonic()
    announced = False
    try:
        while True:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                if not legacy_busy(root / (name + '.lock.d'),
                                   os.environ.get('EDK2_CIX_BUILD_LOCK_BREAK_STALE', '1') == '1'):
                    break
                fcntl.flock(fd, fcntl.LOCK_UN)
            except OSError as error:
                if error.errno not in {errno.EAGAIN, errno.EACCES}:
                    raise
            if not announced:
                print('[lock] Another build-related command is already using this checkout.', file=sys.stderr)
                announced = True
            if timeout == 0:
                print('[lock] Refusing to run in parallel.', file=sys.stderr)
                return 1
            if timeout > 0 and time.monotonic() - start >= timeout:
                print(f'[lock] Timed out waiting {timeout}s for the active build lock.', file=sys.stderr)
                return 1
            time.sleep(0.1)
        write_owner(fd, command)
        environment = dict(os.environ, EDK2_CIX_REPO_LOCK_HELD='1', EDK2_CIX_REPO_LOCK_ROOT=str(root))
        child = subprocess.Popen(command, env=environment, pass_fds=(fd,))

        def forward(signum, _frame):
            child.send_signal(signum)

        for signum in (signal.SIGHUP, signal.SIGINT, signal.SIGTERM, signal.SIGQUIT):
            signal.signal(signum, forward)
        status = child.wait()
        return status if status >= 0 else 128 - status
    finally:
        # Close, never LOCK_UN: an inherited descriptor may still protect a
        # surviving command. No pathname cleanup can delete a replacement lock.
        os.close(fd)


def main() -> None:
    try:
        status = run(sys.argv[1:])
    except (OSError, ValueError) as error:
        print('[lock] ' + str(error), file=sys.stderr)
        status = 2
    raise SystemExit(status)


if __name__ == '__main__':
    main()
