# Reviewed source conflict fixtures

These are exact raw Git blobs from the preserved conflict tree
`c13a964250a98fca4a2b44af6adf0fb0a08c355f`, rooted at conflict handoff commit
`cb253b431548bc24eeec064732c0a6926ae0b1e1`. The three conflicted files, replacement
reset implementation and their INF/DSC consumers are retained as source-review
fixtures. Preserve imported INF bytes and their CRLF endings.

`radxa13_source_conflicts.py` verifies whole-file hashes before transforming the
four editable files. Tests also compile the resolved reset implementation with
host stubs, execute its reset paths, and link the actual capsule global
declarations for both INF source lists. These checks do not qualify a firmware
build, PSCI implementation, EC hardware response or capsule operation on a board.

The exact complete-tree journal regression additionally runs when
`RADXA13_REVIEW_REPO` and `RADXA13_REVIEW_JOURNAL` point to an isolated reviewed
repository and receipt. This verifies all untouched tree entries as well as the
four resolved files; the normal fixture tests require no firmware source clone.
