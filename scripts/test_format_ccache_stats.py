#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

import format_ccache_stats


class FormatCcacheStatsTests(unittest.TestCase):
    def test_load_stats_ignores_buildbox_preamble(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "stats.log"
            path.write_text(
                "[buildbox] Running python3 in container\n"
                '{"cache_miss": 2, "direct_cache_hit": 3, "cache_size_kibibyte": 1024}\n',
                encoding="utf-8",
            )

            self.assertEqual(
                format_ccache_stats.load_stats(path),
                {
                    "cache_miss": 2,
                    "direct_cache_hit": 3,
                    "cache_size_kibibyte": 1024,
                },
            )

    def test_human_summary_is_concise(self) -> None:
        self.assertEqual(
            format_ccache_stats.human_summary(
                {
                    "cache_miss": 2,
                    "direct_cache_hit": 3,
                    "preprocessed_cache_hit": 1,
                    "files_in_cache": 7,
                    "cache_size_kibibyte": 2048,
                }
            ),
            "[ccache] 6 cacheable, 4 hits, 2 misses, 66.7% hit rate, 7 files, 2.0 MiB cache",
        )


if __name__ == "__main__":
    unittest.main()
