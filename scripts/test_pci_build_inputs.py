"""Run the production native Make graph for committed and dirty PCI overlays.

Only the FD compiler is stubbed. The actual prerequisites, configuration stamp
and source metadata helper determine whether it is invoked again.
"""
import os
from pathlib import Path
import subprocess
import time
import unittest
from unittest.mock import patch
import test_firmware_reconfiguration as firmware
from reconstruction_common import show_file
ROOT = Path(__file__).resolve().parents[1]


class PciBuildInputsTests(unittest.TestCase):

    def test_committed_and_dirty_native_inputs_rebuild_bl33(self):
        for version in ('202208', '202608'):
            ref = f'source/unofficial/1.3.2/edk2-stable{version}'
            with self.subTest(ref=ref), patch.dict(os.environ, SOURCE_TEST_REF=ref):
                self.probe(ref)

    def probe(self, ref):
        case = firmware.FirmwareReconfigurationTests()
        case.setUp()
        root = case.root
        pci = 'custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/Pci/PciBusDxe/'
        try:
            for relative in ('src/scripts/resolve_build_metadata.sh', pci + 'PciEnumerator.c', pci + 'PciBusDxe.inf'):
                dest = root / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(show_file(ROOT, ref, relative))
            (root / 'src/scripts/resolve_build_metadata.sh').chmod(493)
            source = (root / 'src/Makefile').read_text()
            marker = 'Build/%/$(UEFI_TARGET)_' + case.toolchain + '/FV/SKY1_BL33_UEFI.fd:'
            begin = source.index(marker)
            body = source.index('\n\t', begin)
            end = source.index('\n.PHONY: refresh-microsoft', body)
            # Keep production prerequisites and the complete forced config-stamp recipe.
            source = source[:body] + '\n\t@mkdir -p "$(@D)"\n\t@cp "$(REPO_ROOT)/' + pci + 'PciEnumerator.c" "$@"\n\t@echo compiled >> "$(REPO_ROOT)/calls"\n' + source[end:]
            (root / 'src/Makefile').write_text(source)
            probe = root / 'probe.mk'
            probe.write_text('.PHONY: probe-inputs\nprobe-inputs:\n\t@printf "%s\\n" $(PATCHED_EDK2_SOURCE_INPUTS) $(CUSTOM_EDK2_BUILD_INPUTS)\n')
            args = {k: v for k, v in case.base.items() if k not in ('BUILD_METADATA_SCRIPT', 'SOURCE_COMMIT_HASH')}
            common = [case.make, '--no-print-directory', '-f', 'Makefile', '-f', str(probe), *(f'{k}={v}' for k, v in args.items())]

            def run(cmd, cwd=root):
                p = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True, timeout=40)
                if p.returncode:
                    raise RuntimeError(p.stdout + p.stderr)
                return p.stdout

            inputs = run(common + ['probe-inputs'], root / 'src').splitlines()
            for value in inputs:
                p = Path(value)
                if not p.is_absolute():
                    p = root / 'src' / p
                assert p.resolve().is_relative_to(root.resolve()), str(p)
                if not p.exists():
                    p.parent.mkdir(parents=True, exist_ok=True)
                    p.write_bytes(b'fixture dependency\n')
            (root / '.gitignore').write_text('src/Build/\ntmp/\ncalls\n')
            run(['git', 'init', '-q'])
            run(['git', 'config', 'user.name', 'PCI dependency probe'])
            run(['git', 'config', 'user.email', 'probe@example.invalid'])
            run(['git', 'add', '.'])
            run(['git', 'commit', '-qm', 'source preimage'])
            fd = root / 'src/Build/O6' / ('RELEASE_' + case.toolchain) / 'FV/SKY1_BL33_UEFI.fd'
            target = str(fd.relative_to(root / 'src'))

            def build():
                skipped = [arg for value in inputs if '/SecureBootDefaults/' in value for arg in ('-o', value)]
                run(common + ['-o', 'preflight-O6', '-o', 'preflight-inputs', '-o', 'preflight-tools', '-o', 'preflight-build-metadata', '-o', 'check-microsoft-secure-boot-release', *skipped, target], root / 'src')
                stamp = fd.parent.parent / '.edk2-cix-build-config'
                cfg = dict((x.split('=', 1) for x in stamp.read_text().splitlines()))
                return {'calls': len((root / 'calls').read_text().splitlines()), 'source_hash': cfg['SOURCE_COMMIT_HASH'], 'stamp_mtime_ns': stamp.stat().st_mtime_ns, 'fd_sha256': __import__('hashlib').sha256(fd.read_bytes()).hexdigest()}

            results = [('initial', build()), ('unchanged', build())]
            c = root / (pci + 'PciEnumerator.c')
            c.write_bytes(c.read_bytes() + b'\n/* committed dependency probe */\n')
            run(['git', 'add', str(c)])
            run(['git', 'commit', '-qm', 'committed overlay edit'])
            results.append(('committed_c', build()))
            # Age every other dependency before editing only the PCI overlay.
            old = time.time() - 20
            for p in root.rglob('*'):
                if p.is_file() and '.git' not in p.parts:
                    os.utime(p, (old, old))
            os.utime(fd, (old + 5, old + 5))
            c.write_bytes(c.read_bytes() + b'\n/* uncommitted dependency probe */\n')
            results.append(('uncommitted_c', build()))
            inf = root / (pci + 'PciBusDxe.inf')
            inf.write_bytes(inf.read_bytes() + b'\n# uncommitted dependency probe\n')
            results.append(('uncommitted_inf', build()))
            self.assertEqual([row[1]['calls'] for row in results], [1, 1, 2, 3, 4], results)
        finally:
            case.doCleanups()


if __name__ == '__main__':
    unittest.main()
