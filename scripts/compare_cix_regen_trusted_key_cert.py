#!/usr/bin/env python3

import argparse
import datetime as dt
import platform
import pathlib
import re
import subprocess
import sys
import tempfile

from vendor_tool_resolver import resolve_vendor_tool


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE_TOOL_DIR = REPO_ROOT / "src/tools/cix_regen_trusted_key_cert"
VENDOR_TOOL_REPO_RELPATH = "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/X86_64/cix_regen_trusted_key_cert"
DEFAULT_PUBLIC_KEY = REPO_ROOT / "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/Keys/oem_publickey.pem"
DEFAULT_PRIVATE_KEY = REPO_ROOT / "src/edk2-non-osi/Platform/CIX/Sky1/PackageTool/Keys/oem_privatekey.pem"
CUSTOM_OID = "1.3.6.1.4.1.4128.2100.303"


def host_arch_tag() -> str:
    machine = platform.machine()
    if machine in {"arm64", "aarch64"}:
        return "aarch64"
    if machine in {"amd64", "x86_64"}:
        return "x86_64"
    return machine


SOURCE_TOOL = SOURCE_TOOL_DIR / "build" / host_arch_tag() / "cix_regen_trusted_key_cert"


def run(cmd, *, input_text=None):
    result = subprocess.run(
        cmd,
        input=input_text,
        text=input_text is not None,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(str(x) for x in cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result


def build_source_tool():
    run(["make", "-C", str(SOURCE_TOOL_DIR), f"HOST_ARCH={host_arch_tag()}", "clean", "all"])


def pem_pubkey_to_der(path: pathlib.Path) -> bytes:
    return subprocess.check_output(
        ["openssl", "pkey", "-pubin", "-in", str(path), "-pubout", "-outform", "DER"]
    )


def cert_subject_pub_der(cert_path: pathlib.Path) -> bytes:
    pub_pem = subprocess.check_output(
        ["openssl", "x509", "-inform", "DER", "-in", str(cert_path), "-pubkey", "-noout"]
    )
    return subprocess.check_output(
        ["openssl", "pkey", "-pubin", "-pubout", "-outform", "DER"],
        input=pub_pem,
    )


def cert_text(cert_path: pathlib.Path) -> str:
    return subprocess.check_output(
        ["openssl", "x509", "-inform", "DER", "-in", str(cert_path), "-text", "-noout"],
        text=True,
    )


def cert_dates(cert_path: pathlib.Path) -> tuple[dt.datetime, dt.datetime]:
    output = subprocess.check_output(
        ["openssl", "x509", "-inform", "DER", "-in", str(cert_path), "-dates", "-noout"],
        text=True,
    ).splitlines()
    values = {}
    for line in output:
        key, value = line.split("=", 1)
        values[key] = dt.datetime.strptime(value, "%b %d %H:%M:%S %Y %Z")
    return values["notBefore"], values["notAfter"]


def extract_custom_extension_der(cert_path: pathlib.Path) -> bytes:
    output = subprocess.check_output(
        ["openssl", "asn1parse", "-inform", "DER", "-in", str(cert_path)],
        text=True,
    ).splitlines()
    seen_oid = False
    for line in output:
        if CUSTOM_OID in line:
            seen_oid = True
            continue
        if seen_oid and "[HEX DUMP]:" in line:
            hex_dump = line.split("[HEX DUMP]:", 1)[1].strip()
            return bytes.fromhex(hex_dump)
    raise RuntimeError(f"extension {CUSTOM_OID} not found in {cert_path}")


def pem_to_pub_der_from_private(path: pathlib.Path) -> bytes:
    return subprocess.check_output(
        ["openssl", "pkey", "-in", str(path), "-pubout", "-outform", "DER"]
    )


def extract_subject_pub_pem(cert_path: pathlib.Path, out_path: pathlib.Path) -> None:
    data = subprocess.check_output(
        ["openssl", "x509", "-inform", "DER", "-in", str(cert_path), "-pubkey", "-noout"]
    )
    out_path.write_bytes(data)


def der_public_key_to_pem_bytes(der: bytes) -> bytes:
    return subprocess.check_output(
        ["openssl", "pkey", "-pubin", "-inform", "DER", "-outform", "PEM"],
        input=der,
    )


def assert_pss_sha256(cert_info: str) -> None:
    required = [
        "Signature Algorithm: rsassaPss",
        "Hash Algorithm: sha256",
        "Mask Algorithm: mgf1 with sha256",
        "Salt Length: 0x20",
        "X509v3 Basic Constraints:",
        "CA:FALSE",
    ]
    for item in required:
        if item not in cert_info:
            raise AssertionError(f"missing expected certificate text: {item}")
    issuer_variants = (
        "Issuer: CN=Trusted Key Certificate",
        "Issuer: CN = Trusted Key Certificate",
    )
    subject_variants = (
        "Subject: CN=Trusted Key Certificate",
        "Subject: CN = Trusted Key Certificate",
    )
    if not any(item in cert_info for item in issuer_variants):
        raise AssertionError("missing expected trusted-key issuer text")
    if not any(item in cert_info for item in subject_variants):
        raise AssertionError("missing expected trusted-key subject text")


def extract_extension_value(cert_info: str, label: str) -> str:
    lines = cert_info.splitlines()
    for index, line in enumerate(lines):
        if label not in line:
            continue
        values: list[str] = []
        for following in lines[index + 1 :]:
            stripped = following.strip()
            if not stripped:
                break
            if following.startswith("            ") or following.startswith("                "):
                values.append(stripped)
                continue
            break
        if not values:
            raise AssertionError(f"extension {label} has no value text")
        return " ".join(values)
    raise AssertionError(f"extension {label} not found")


def compare_generate(vendor_cert: pathlib.Path, source_cert: pathlib.Path, public_key: pathlib.Path, private_key: pathlib.Path) -> None:
    oem_pub_der = pem_pubkey_to_der(public_key)
    sign_pub_der = pem_to_pub_der_from_private(private_key)

    vendor_text = cert_text(vendor_cert)
    source_text = cert_text(source_cert)
    assert_pss_sha256(vendor_text)
    assert_pss_sha256(source_text)
    if extract_extension_value(source_text, "X509v3 Subject Key Identifier:") != extract_extension_value(
        vendor_text, "X509v3 Subject Key Identifier:"
    ):
        raise AssertionError("source cert subject key identifier does not match vendor output")
    if extract_extension_value(source_text, "X509v3 Authority Key Identifier:") != extract_extension_value(
        vendor_text, "X509v3 Authority Key Identifier:"
    ):
        raise AssertionError("source cert authority key identifier does not match vendor output")

    if cert_subject_pub_der(vendor_cert) != sign_pub_der:
        raise AssertionError("vendor cert subject public key does not match sign key")
    if cert_subject_pub_der(source_cert) != sign_pub_der:
        raise AssertionError("source cert subject public key does not match sign key")
    if extract_custom_extension_der(vendor_cert) != oem_pub_der:
        raise AssertionError("vendor cert custom extension does not match OEM public key")
    if extract_custom_extension_der(source_cert) != oem_pub_der:
        raise AssertionError("source cert custom extension does not match OEM public key")

    vendor_start, vendor_end = cert_dates(vendor_cert)
    source_start, source_end = cert_dates(source_cert)
    if vendor_end - vendor_start != dt.timedelta(days=365 * 20):
        raise AssertionError("vendor cert validity window is not 20*365 days")
    if source_end - source_start != dt.timedelta(days=365 * 20):
        raise AssertionError("source cert validity window is not 20*365 days")


def compare_extract(source_tool: pathlib.Path, sample_cert: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory(prefix="cix-regen-verify-") as td:
        td_path = pathlib.Path(td)
        signer_pub = td_path / "signer.pem"
        expected_oem_pub = td_path / "expected-oem.pem"
        extract_subject_pub_pem(sample_cert, signer_pub)
        expected_oem_pub.write_bytes(der_public_key_to_pem_bytes(extract_custom_extension_der(sample_cert)))

        result = run([str(source_tool), "-p", str(signer_pub), "-v", str(sample_cert), "-o", str(expected_oem_pub)])
        if result.stdout != "X509_verify successful\n":
            raise AssertionError("source verify mode did not reproduce the vendor success transcript")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Compare the source cix_regen_trusted_key_cert implementation against "
            "a vendor binary, optionally materialized from another branch."
        )
    )
    parser.add_argument("--public-key", type=pathlib.Path, default=DEFAULT_PUBLIC_KEY)
    parser.add_argument("--private-key", type=pathlib.Path, default=DEFAULT_PRIVATE_KEY)
    parser.add_argument(
        "--vendor-tool",
        type=pathlib.Path,
        help="Optional path to an external vendor cix_regen_trusted_key_cert binary",
    )
    parser.add_argument(
        "--vendor-ref",
        action="append",
        default=[],
        help=(
            "Git ref to search for the bundled vendor binary when --vendor-tool "
            "is omitted; may be repeated"
        ),
    )
    parser.add_argument("--source-tool", type=pathlib.Path, default=SOURCE_TOOL)
    parser.add_argument("--sample-cert", type=pathlib.Path, action="append", default=[], help="Optional DER certs for verify-mode validation")
    parser.add_argument("--skip-build", action="store_true", help="Do not rebuild the source tool before testing")
    parser.add_argument("--skip-vendor", action="store_true", help="Skip the generate-mode vendor comparison and only validate the source tool")
    args = parser.parse_args()

    run_vendor_compare = not args.skip_vendor

    required_paths = [args.public_key, args.private_key]

    for path in required_paths:
        if not path.exists():
            parser.error(f"missing required input: {path}")

    if not args.skip_build:
        build_source_tool()
    elif not args.source_tool.exists():
        parser.error(f"missing source tool: {args.source_tool}")

    if run_vendor_compare:
        refs = tuple(args.vendor_ref) if args.vendor_ref else None
        with resolve_vendor_tool(
            explicit_path=args.vendor_tool,
            repo_relpath=VENDOR_TOOL_REPO_RELPATH,
            refs=refs or ("main-monorepo-upstream", "main"),
        ) as vendor_tool:
            with tempfile.TemporaryDirectory(prefix="cix-regen-generate-") as td:
                td_path = pathlib.Path(td)
                vendor_cert = td_path / "vendor.crt"
                source_cert = td_path / "source.crt"

                run([str(vendor_tool), "-p", str(args.public_key), "-s", str(args.private_key), "-o", str(vendor_cert)])
                run([str(args.source_tool), "-p", str(args.public_key), "-s", str(args.private_key), "-o", str(source_cert)])
                compare_generate(vendor_cert, source_cert, args.public_key, args.private_key)

    for sample_cert in args.sample_cert:
        if not sample_cert.exists():
            parser.error(f"sample cert not found: {sample_cert}")
        compare_extract(args.source_tool, sample_cert)

    print("cix_regen_trusted_key_cert comparison passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
