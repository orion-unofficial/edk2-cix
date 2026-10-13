# Reviewed source conflict fixtures

These are exact raw vendor conflict blobs. The three conflicted files,
replacement reset implementation and their INF/DSC consumers are source-review
fixtures. Preserve imported INF bytes and their CRLF endings.

The identity scanner permits the original copyright email in the DSC fixture
only while its complete bytes and attribution line match the reviewed hashes.
Changes or copies require a new review; other identity checks still apply.

`radxa13_source_conflicts.py` verifies whole-file hashes before transforming the
four editable files. Tests also compile the resolved reset implementation with
host stubs, execute its reset paths, and link the actual capsule global
declarations for both INF source lists. These checks do not qualify a firmware
build, PSCI implementation, EC hardware response or capsule operation on a board.

The exact complete-tree journal regression additionally runs when
`RADXA13_REVIEW_REPO` and `RADXA13_REVIEW_JOURNAL` point to an isolated reviewed
repository and receipt. This verifies all untouched tree entries as well as the
four resolved files; the normal fixture tests require no firmware source clone.
