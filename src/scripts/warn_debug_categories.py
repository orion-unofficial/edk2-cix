#!/usr/bin/env python3
"""Report selected categories without labelled sites in compiler dependencies."""

import argparse
import json
from pathlib import Path
import re
import shlex
import sys

from debug_build_policy import debug_bits


def audit(build_dir: Path, header: Path, mask: int) -> dict:
    bits = debug_bits(header.read_text())
    paths = set()
    dependencies = list(build_dir.rglob("*.obj.deps"))
    for dep in dependencies:
        body = dep.read_text().partition(":")[2].replace("\\\n", " ")
        paths.update(Path(p).resolve() for p in shlex.split(body)
                     if Path(p).is_absolute() and Path(p).suffix in {".c", ".h", ".cpp"})
    expressions, definitions, missing = [], {}, []
    for path in sorted(paths):
        try:
            text = path.read_text(errors="replace")
        except OSError:
            missing.append(str(path))
            continue
        text = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*',
                      lambda m: " " if m[0].startswith(("/*", "//")) else m[0], text, flags=re.S)
        for name, value in re.findall(r"(?m)^\s*#\s*define\s+(\w+)\s+([^\n]+)", text):
            definitions.setdefault(name, set()).update(re.findall(r"\b\w+\b", value))
        for match in re.finditer(r"\b(?:DEBUG\s*\(\s*\(|Debug(?:V|B)?Print\s*\()\s*([^,]{1,200}),", text):
            if not match[1].lstrip().startswith("IN "):
                expressions.append(match[1].strip())
    used = set()
    for expression in expressions:
        pending = set(re.findall(r"\b\w+\b", expression))
        seen = set()
        while pending:
            token = pending.pop()
            if token in seen:
                continue
            seen.add(token)
            category = token.replace("EFI_D_", "DEBUG_", 1)
            if category in bits:
                used.add(category)
            else:
                pending.update(definitions.get(token, ()))
    # Incomplete evidence must not produce a confident absence claim.
    absent = sorted(name for name, bit in bits.items() if mask & bit and name not in used)
    return {"mask": f"0x{mask:08x}", "dependency_files": len(dependencies),
            "source_header_files": len(paths), "unreadable": missing,
            "categories_without_labelled_sites": absent if dependencies and not missing else [],
            "limits": "Source call sites from actual compiler dependencies; inactive preprocessor branches "
                      "are included. Dynamic/all-level calls and precompiled vendor modules can still emit "
                      "output. This report does not prove zero runtime output or zero size impact."}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--mask", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.build_dir, args.header, args.mask)
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    absent = report["categories_without_labelled_sites"]
    if absent:
        print("[debug-mask] WARNING: no category-specific source diagnostics found for " + ", ".join(absent)
              + "; bits retained. Dynamic/all-level or vendor-binary logging may still apply.", file=sys.stderr)
    elif not report["dependency_files"] or report["unreadable"]:
        print("[debug-mask] WARNING: dependency audit incomplete; unused categories could not be determined.", file=sys.stderr)


if __name__ == "__main__":
    main()
