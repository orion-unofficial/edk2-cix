#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


MIN_TEXT_LENGTH = 4
BANNED_TEXT_PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("NB10", re.compile(r"NB10", re.IGNORECASE)),
    ("RSDS", re.compile(r"RSDS", re.IGNORECASE)),
    (".pdb", re.compile(r"\.pdb\b", re.IGNORECASE)),
    ("/tmp/", re.compile(r"/tmp/", re.IGNORECASE)),
    ("/Users/", re.compile(r"/Users/", re.IGNORECASE)),
    ("/workspaces/", re.compile(r"/workspaces/", re.IGNORECASE)),
    (
        "Jenkins workspace",
        re.compile(
            r"/(?:data/devops/jenkins|var/lib/jenkins|home/jenkins/agent)/workspace/",
            re.IGNORECASE,
        ),
    ),
    ("GitHub Actions workspace", re.compile(r"(?:/home/runner/work/|/__w/|/github/workspace/)", re.IGNORECASE)),
    ("GitLab workspace", re.compile(r"/builds/", re.IGNORECASE)),
)
WINDOWS_DRIVE_PATH_PATTERN = re.compile(
    r"(?<![A-Za-z0-9])"
    r"[A-Za-z]:[\\/]"
    r"(?:[^\\/:*?\"<>|\r\n]{2,}[\\/])+"
    r"[^\\/:*?\"<>|\r\n]{1,}",
    re.IGNORECASE,
)
BENIGN_WINDOWS_PATH_PATTERNS: tuple[re.Pattern[str], ...] = (
    # ShellPkg bakes in a sample command line; it is documentation text rather
    # than leaked build metadata, so keep the audit focused on real breadcrumbs.
    re.compile(r"Shell>\s+set\s+-v\s+EFI_SOURCE\s+c:\\project\\EFI1\.1\b", re.IGNORECASE),
)


@dataclass(frozen=True)
class AuditFinding:
    path: str
    text: str
    reasons: tuple[str, ...]


def _iter_ascii_strings(data: bytes) -> list[str]:
    strings: list[str] = []
    current = bytearray()
    for byte in data:
        if 0x20 <= byte <= 0x7E:
            current.append(byte)
            continue
        if len(current) >= MIN_TEXT_LENGTH:
            strings.append(current.decode("ascii"))
        current.clear()
    if len(current) >= MIN_TEXT_LENGTH:
        strings.append(current.decode("ascii"))
    return strings


def _iter_utf16le_strings(data: bytes) -> list[str]:
    strings: list[str] = []
    for offset in (0, 1):
        current = bytearray()
        index = offset
        while index + 1 < len(data):
            char = data[index]
            nul = data[index + 1]
            if nul == 0 and 0x20 <= char <= 0x7E:
                current.append(char)
            else:
                if len(current) >= MIN_TEXT_LENGTH:
                    strings.append(current.decode("ascii"))
                current.clear()
            index += 2
        if len(current) >= MIN_TEXT_LENGTH:
            strings.append(current.decode("ascii"))
    return strings


def _iter_text_candidates(path: Path) -> list[str]:
    data = path.read_bytes()
    seen: set[str] = set()
    strings: list[str] = []
    for text in _iter_ascii_strings(data) + _iter_utf16le_strings(data):
        if text in seen:
            continue
        seen.add(text)
        strings.append(text)
    return strings


def _match_reasons(text: str) -> tuple[str, ...]:
    reasons = [label for label, pattern in BANNED_TEXT_PATTERNS if pattern.search(text)]
    if WINDOWS_DRIVE_PATH_PATTERN.search(text) and not any(
        pattern.search(text) for pattern in BENIGN_WINDOWS_PATH_PATTERNS
    ):
        reasons.append("Windows drive path")
    return tuple(reasons)


def audit_targets(targets: list[tuple[str, Path]]) -> list[AuditFinding]:
    findings: list[AuditFinding] = []
    for display_path, path in targets:
        if not path.is_file():
            continue
        for text in _iter_text_candidates(path):
            reasons = _match_reasons(text)
            if reasons:
                findings.append(AuditFinding(display_path, text, reasons))
    return findings


def format_findings(findings: list[AuditFinding]) -> list[str]:
    return [
        f"{finding.path}: {finding.text} [{', '.join(finding.reasons)}]"
        for finding in findings
    ]
