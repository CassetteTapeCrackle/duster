"""Self-checks for the knob search in tools/fit_kick.py (numpy only, no renders)."""

import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from fit_kick import Param, search  # noqa: E402

BOWL = [Param("a", -12, 12, 0, 0), Param("b", 0, 60, 36, 0), Param("c", 1, 40, 8, 0),
        Param("d", 1000, 20000, 20000, 0), Param("e", 0, 100, 0, 0)]
BOWL_MIN = np.array([3.0, 21.0, 3.7, 6365.0, 22.0])
BOWL_WEIGHT = np.array([1.0, 3.0, 10.0, 2.0, 5.0])


def bowl(population):
    span = np.array([p.hi - p.lo for p in BOWL])
    return np.sum(BOWL_WEIGHT * ((population - BOWL_MIN) / span) ** 2, axis=1)


class SearchTests(unittest.TestCase):
    def test_search_finds_a_quadratic_minimum(self):
        x0 = np.array([p.default for p in BOWL])
        best, _ = search(bowl, BOWL, x0, seed=1, budget=1000)
        span = np.array([p.hi - p.lo for p in BOWL])
        np.testing.assert_array_less(np.abs(best - BOWL_MIN) / span, 0.01)

    def test_search_is_repeatable(self):
        x0 = np.array([p.default for p in BOWL])
        first = search(bowl, BOWL, x0, seed=7, budget=300)
        second = search(bowl, BOWL, x0, seed=7, budget=300)
        np.testing.assert_array_equal(first[0], second[0])
        self.assertEqual(first[1], second[1])

    def test_search_respects_bounds_and_steps(self):
        params = [Param("bits", 2, 16, 16, 1), Param("snap", 1, 40, 8, 0.1), Param("tone", 1000, 20000, 20000, 1),
                  Param("drive", 0, 100, 0, 0)]
        seen = []

        def score(population):
            seen.append(population.copy())
            return np.sum((population - np.array([2.0, 40.0, 1000.0, 100.0])) ** 2, axis=1)

        search(score, params, np.array([p.default for p in params]), seed=3, budget=400)
        candidates = np.concatenate(seen)
        lo, hi = np.array([p.lo for p in params]), np.array([p.hi for p in params])
        self.assertTrue(np.all((candidates >= lo) & (candidates <= hi)))
        for i, p in enumerate(params):
            if p.step:
                k = candidates[:, i] / p.step
                np.testing.assert_allclose(k, np.round(k), atol=1e-9)


if __name__ == "__main__":
    unittest.main()
