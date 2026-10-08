#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import unittest


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

import ccache_stats


class CcacheStatsTests(unittest.TestCase):
    def test_parse_print_stats_accepts_bookworm_ccache_output(self) -> None:
        self.assertEqual(
            ccache_stats.parse_print_stats(
                "\n".join(
                    [
                        "cache_miss\t10587",
                        "direct_cache_hit\t45921",
                        "preprocessed_cache_hit\t17170",
                        "cache_size_kibibyte\t466344",
                    ]
                )
            ),
            {
                "cache_miss": 10587,
                "direct_cache_hit": 45921,
                "preprocessed_cache_hit": 17170,
                "cache_size_kibibyte": 466344,
            },
        )

    def test_parse_print_stats_ignores_unexpected_lines(self) -> None:
        self.assertEqual(
            ccache_stats.parse_print_stats(
                "cache_miss\t2\nmalformed\ncache_size_kibibyte not-a-number\n"
            ),
            {"cache_miss": 2},
        )


if __name__ == "__main__":
    unittest.main()
