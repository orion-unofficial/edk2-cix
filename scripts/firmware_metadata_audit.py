#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import re


MIN_TEXT_LENGTH = 4
BASE_BANNED_TEXT_PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    # Require a plausible basename. A bare four-byte ``.PDB`` sequence occurs
    # naturally in signed/compressed firmware and is not a debug-file path.
    (
        ".pdb",
        re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.-]*\.pdb\b", re.IGNORECASE),
    ),
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
CODEVIEW_SIGNATURES: tuple[bytes, ...] = (b"NB10", b"RSDS")
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
TEMP_PATH_ENV_VARS: tuple[str, ...] = ("TMPDIR", "TMP", "TEMP", "TEMPDIR", "XDG_RUNTIME_DIR")


@dataclass(frozen=True)
class AuditFinding:
    path: str
    text: str
    reasons: tuple[str, ...]


def _normalize_temp_path(value: str) -> str | None:
    normalized = value.strip()
    if not normalized:
        return None
    if re.match(r"^[A-Za-z]:[\\/]", normalized):
        return normalized.rstrip("\\/")
    if normalized.startswith("/"):
        return normalized.rstrip("/")
    return None


def _dynamic_temp_path_patterns() -> tuple[tuple[str, re.Pattern[str]], ...]:
    patterns: list[tuple[str, re.Pattern[str]]] = []
    seen: set[str] = set()
    for label, value in (
        ("/tmp/", "/tmp"),
        ("/var/tmp/", "/var/tmp"),
        *((f"{env_name} path", os.environ.get(env_name, "")) for env_name in TEMP_PATH_ENV_VARS),
    ):
        normalized = _normalize_temp_path(value)
        if not normalized or normalized in seen:
            continue
        seen.add(normalized)
        patterns.append(
            (
                label,
                re.compile(re.escape(normalized) + r"(?:[\\/]|$)", re.IGNORECASE),
            )
        )
    return tuple(patterns)


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


def _codeview_signature_reasons(data: bytes) -> tuple[str, ...]:
    reasons = [
        signature.decode("ascii")
        for signature in CODEVIEW_SIGNATURES
        if signature in data
    ]
    return tuple(reasons)


def _match_reasons(
    text: str,
    banned_text_patterns: tuple[tuple[str, re.Pattern[str]], ...],
) -> tuple[str, ...]:
    reasons = [
        label
        for label, pattern in banned_text_patterns
        if pattern.search(text)
    ]
    if WINDOWS_DRIVE_PATH_PATTERN.search(text) and not any(
        pattern.search(text) for pattern in BENIGN_WINDOWS_PATH_PATTERNS
    ):
        reasons.append("Windows drive path")
    return tuple(reasons)


def audit_targets(targets: list[tuple[str, Path]]) -> list[AuditFinding]:
    findings: list[AuditFinding] = []
    banned_text_patterns = BASE_BANNED_TEXT_PATTERNS + _dynamic_temp_path_patterns()
    for display_path, path in targets:
        if not path.is_file():
            continue
        data = path.read_bytes()
        signature_reasons = _codeview_signature_reasons(data)
        matched_signatures: set[str] = set()
        seen: set[str] = set()
        strings: list[str] = []
        for text in _iter_ascii_strings(data) + _iter_utf16le_strings(data):
            if text in seen:
                continue
            seen.add(text)
            strings.append(text)
        for text in strings:
            reasons = list(_match_reasons(text, banned_text_patterns))
            for signature_reason in signature_reasons:
                if signature_reason in text:
                    reasons.append(signature_reason)
                    matched_signatures.add(signature_reason)
            if reasons:
                findings.append(AuditFinding(display_path, text, tuple(dict.fromkeys(reasons))))
        for signature_reason in signature_reasons:
            if signature_reason not in matched_signatures:
                findings.append(AuditFinding(display_path, signature_reason, (signature_reason,)))
    return findings


def format_findings(findings: list[AuditFinding]) -> list[str]:
    return [
        f"{finding.path}: {finding.text} [{', '.join(finding.reasons)}]"
        for finding in findings
    ]
