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
import re
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


def lock_path(root: Path, name: str) -> Path:
    if not name or Path(name).name != name or name in {'.', '..'}:
        raise ValueError('lock name must be a single path component')
    return root / (name + '.lock')


def inherited_fds() -> tuple[int, ...]:
    return tuple(dict.fromkeys(int(value) for value in os.environ.get('EDK2_CIX_LOCK_FDS', '').split(',')
                               if value.isascii() and value.isdigit() and 3 <= int(value) <= 2**31 - 1))


def inherited_lock(path: Path) -> bool:
    """An environment value identifies a descriptor, not a lock owner."""
    try:
        st = path.stat()
        for fd in inherited_fds():
            try:
                candidate = os.fstat(fd)
            except OSError:
                continue
            if (candidate.st_dev, candidate.st_ino) != (st.st_dev, st.st_ino):
                continue
            probe = os.open(path, os.O_RDWR)
            try:
                try:
                    fcntl.flock(probe, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    # Only the owning open file description can reacquire
                    # without blocking while an independent open conflicts.
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    return True
            finally:
                os.close(probe)
    except OSError:
        pass
    return False


def open_descriptors() -> tuple[int, ...]:
    # GNU Make before 4.4 can pass a pipe jobserver rather than a FIFO pathname.
    jobserver = re.search(r'(?:^|\s)--jobserver-(?:auth|fds)=(\d+),(\d+)(?:\s|$)',
                          os.environ.get('MAKEFLAGS', ''))
    candidates = (*inherited_fds(), *(map(int, jobserver.groups()) if jobserver else ()))
    result = []
    for fd in dict.fromkeys(candidates):
        try:
            os.fstat(fd)
            result.append(fd)
        except OSError:
            continue
    return tuple(result)


def run(command: list[str], root: Path, name: str) -> int:
    if not command:
        print('usage: scripts/with_repo_lock.sh <command> [args...]', file=sys.stderr)
        return 2
    path = lock_path(root, name)
    if inherited_lock(path):
        os.execvp(command[0], command)
    timeout = wait_seconds(os.environ.get('EDK2_CIX_BUILD_LOCK_WAIT', '0'))
    root.mkdir(parents=True, exist_ok=True)
    # Persistent file, deliberately never removed (including on normal exit).
    pass_fds = open_descriptors()
    fd = os.open(path, os.O_CREAT | os.O_RDWR, 0o600)
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
        lock_fds = [value for value in inherited_fds() if value in pass_fds]
        lock_fds.append(fd)
        environment = dict(os.environ, EDK2_CIX_LOCK_FDS=','.join(map(str, lock_fds)))
        environment.pop('EDK2_CIX_REPO_LOCK_HELD', None)
        child = subprocess.Popen(command, env=environment, pass_fds=(*pass_fds, fd))

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
        args = sys.argv[1:]
        default_root = Path(__file__).resolve().parents[1] / '.buildbox/locks'
        root = Path(os.environ.get('EDK2_CIX_BUILD_LOCK_ROOT', str(default_root)))
        name = os.environ.get('EDK2_CIX_BUILD_LOCK_NAME', 'repo-build')
        workspace = args[:1] == ['--workspace']
        if workspace:
            # Direct Make entrypoints always use this checkout's workspace.
            root, name = default_root, 'repo-build'
            args = args[1:]
            delegated = lock_path(root, 'repo-build-delegated')
            if Path('/build-infra/scripts/with_repo_lock.sh').is_file() and inherited_lock(delegated):
                name = 'repo-build-delegated'
        if args == ['--held']:
            status = 0 if inherited_lock(lock_path(root, name)) else 1
        else:
            status = run(args, root, name)
    except (OSError, ValueError) as error:
        print('[lock] ' + str(error), file=sys.stderr)
        status = 2
    raise SystemExit(status)


if __name__ == '__main__':
    main()
