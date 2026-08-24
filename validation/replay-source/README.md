# Historical replay source oracles

The `1.2.1` through `1.3.1` directories contain exact high-risk vendor source
blobs from the corresponding
`source/vendor/radxa/<version>/edk2-stable202208` ref. The replay build renders
that complete release-specific vendor ref; these copies are independently
checkable provenance oracles rather than EDK2 shadow packages. The recorded
flash layout lets the input recorder extract the correct PM payload after the
`1.3.x` header added image type `9` and moved several regions.

The shared build-infrastructure overlay had converted vendor `Printf` ASL to
syntax accepted by ACPICA 20260408. That was appropriate for maintained custom
builds, but it changed the compiled BL33 and led the old replay workflow to
substitute the published BL33 at packaging time. Exact replay now renders the
matching complete vendor source with ACPICA 20200925, rebuilds BL33, and
compares the rebuilt bytes against the recorded release hash.

Do not edit these files independently. Refresh them mechanically from the
matching immutable vendor refs and verify their Git blob IDs when adding a new
recorded vendor release.

Recorded source blob IDs:

| Release | `Dsdt-AcpiRam.asl` | `Dsdt-Dpu.asl` | `Dsdt-ScmiMailbox.asl` | `spi_flash_config_all.json` |
| --- | --- | --- | --- | --- |
| 1.2.1 | `27b23b7a0f5d` | `378aa9972b4e` | `316821c8661e` | `a1f4478ec01d` |
| 1.2.2 | `27b23b7a0f5d` | `378aa9972b4e` | `316821c8661e` | `a1f4478ec01d` |
| 1.2.3 | `27b23b7a0f5d` | `378aa9972b4e` | `316821c8661e` | `a1f4478ec01d` |
| 1.2.4 | `27b23b7a0f5d` | `378aa9972b4e` | `316821c8661e` | `a1f4478ec01d` |
| 1.3.0 | `27b23b7a0f5d` | `1c181d6aa414` | `316821c8661e` | `09469c6bfa1f` |
| 1.3.1 | `27b23b7a0f5d` | `74a8154ef678` | `316821c8661e` | `5265e2799660` |
