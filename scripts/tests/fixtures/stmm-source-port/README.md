# StandaloneMM legacy preparation fixtures

These are byte-for-byte copies of `CixStandaloneMmPkg/LegacyBootInfo.h` and
`CixStandaloneMmPkg/Patches/LegacyDepexReadOnly.patch` from reviewed component
commit `dd9c47397b670f072e08fb49cb36ed3ed4149b39`. The patch is the existing
mechanically generated backport of EDK2 commit
`2ddae5df31789853040f4c5261bb85e2f010c4a7`. Preserve its mixed line endings.
The tests bind both fixture hashes and the exact patched source postimage.

These fixtures allow host preparation regressions using published maintained
firmware source pins without fetching unpublished candidate objects. They do
not add either file to the maintained component, change its identity, or make
native compilation eligible. Component descendant integration remains required.

| Fixture | SHA-256 |
| --- | --- |
| `LegacyBootInfo.h` | `a7e7ee25035a09602681758f5dcbfdff49390edad83752e704dce6a28701a816` |
| `LegacyDepexReadOnly.patch` | `211c6af55216dde005fb17749e059579d0baea0abb1d04b9a522db88b4e1b85a` |
