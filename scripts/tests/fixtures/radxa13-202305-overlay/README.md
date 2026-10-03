# Reviewed 202305 / Radxa 1.3.1 overlay conflict

The two conflict blobs come from the preserved overlay handoff commit
`52d320e14e45bc36600e1c1a64adc03a277fc713`, complete tree
`7b703a212e9e88535ddc83a8d3432457c801f21a`. The `.overlay`, `.old` and `.new`
files are exact imported Git blobs from source input
`3dc9402a7bd042c843cdbf0c4e99e73b6871f88e` and destination vendor port
`02d1b4a387a048db91183d4b8aa9034c5240439a`. Preserve their CRLF bytes.

The original handoff's right-side labels end in an unrelated INF path because
its source-porting formatter used the final mapping from an earlier loop.
The merge data use the correct destination blobs. Tests reproduce both
conflicts from the three exact source blobs and disregard labels only.

The resolution retains custom Windows makefile backslash escaping while
adopting the new BaseTools `CFLAGS` variable. It retains the custom mixed
X509/EFI signature-database provisioning and RSA public-key cleanup while
adopting upstream's `__func__` diagnostic identifier. Host tests execute the
actual resolved fetch helpers, checking mixed payload bytes and cleanup on
malformed input and allocation/conversion errors. They do not qualify a
firmware build, deployed variables, Secure Boot enforcement or board behavior.

The accepted complete handoff tree and selected input identities are pinned
in `radxa13_source_conflicts.py`; only these two regular files may change.
Optional journal tests require `RADXA13_OVERLAY_REPO` and
`RADXA13_OVERLAY_JOURNAL` pointing to an isolated candidate repository and its
reviewed receipt. Unknown release pairs and changed inputs require review.
