"""Evaluate full production layouts, including the metrics used for review."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from preview import model, metrics

ROOT = Path(__file__).resolve().parent


class Layouts(unittest.TestCase):
    def test_crossing_overlap_and_touch_are_distinct(self):
        def graph(*parts):
            return dict(width=20, height=20, edges=[dict(points=p) for p in parts])
        result = metrics(graph([(0, 5), (10, 5)], [(5, 0), (5, 10)]))
        self.assertEqual((result['crossings'], result['overlaps']), (1, 0))
        result = metrics(graph([(0, 5), (10, 5)], [(5, 5), (15, 5)]))
        self.assertEqual((result['crossings'], result['overlaps']), (0, 1))
        result = metrics(graph([(0, 5), (5, 5)], [(5, 5), (10, 5)]))
        self.assertEqual((result['crossings'], result['overlaps']), (0, 0))
        # Multiple comparisons must not mutate the next segment's orientation.
        result = metrics(graph([(0, 5), (10, 5)], [(3, 0), (3, 10)], [(7, 0), (7, 10)]))
        self.assertEqual(result['crossings'], 2)

    def test_example_quality(self):
        with tempfile.TemporaryDirectory() as work:
            binary = Path(work)/'layout'
            subprocess.run(['cc', '-std=c90', '-pedantic-errors', '-Wall', '-Wextra', '-Werror',
                            str(ROOT/'test_view.c'), *[str(ROOT/name) for name in
                            ('graph.c', 'quality.c', 'examples.c', 'present.c')], '-o', str(binary)], check=True)
            for example in range(8):
                with self.subTest(example=example):
                    auto = metrics(model(binary, example, 0))
                    previous = metrics(model(binary, example, 2))
                    self.assertEqual(auto['overlaps'], 0)
                    self.assertLessEqual(auto['crossings'], previous['crossings'])
                    self.assertLessEqual(auto['bends'], previous['bends'])
                    self.assertLessEqual(auto['area'], previous['area'])
                    if example in (0, 1, 2, 3, 4, 6):
                        self.assertEqual(auto['crossings'], 0)
                    if example in (0, 1, 2, 3):
                        self.assertEqual(auto['bends'], 0)
