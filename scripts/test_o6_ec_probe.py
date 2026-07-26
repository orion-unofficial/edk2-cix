#!/usr/bin/env python3

from pathlib import Path
import tempfile
import unittest

from ec.o6_ec_probe import collect_thermal_zones


class O6EcProbeTest(unittest.TestCase):
    def test_thermal_zone_trip_point_provenance_and_empty_state(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            empty = root / "thermal_zone0"
            empty.mkdir()
            (empty / "type").write_text("empty-zone\n", encoding="utf-8")

            populated = root / "thermal_zone1"
            populated.mkdir()
            (populated / "type").write_text("cpu-zone\n", encoding="utf-8")
            (populated / "trip_point_0_temp").write_text("95000\n", encoding="utf-8")
            (populated / "trip_point_0_type").write_text("critical\n", encoding="utf-8")

            zones = collect_thermal_zones(root)

        self.assertEqual(len(zones), 2)
        self.assertEqual(zones[0]["trip_point_source"], "sysfs")
        self.assertEqual(zones[0]["trip_point_count"], 0)
        self.assertEqual(zones[0]["trip_points"], {})
        self.assertEqual(zones[1]["trip_point_count"], 2)
        self.assertEqual(zones[1]["trip_points"]["trip_point_0_temp"], "95000")
        self.assertEqual(zones[1]["trip_points"]["trip_point_0_type"], "critical")


if __name__ == "__main__":
    unittest.main()
