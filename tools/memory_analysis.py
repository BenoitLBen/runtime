#!/usr/bin/python
"""Collection of helper functions for the HMatrix memory instrumentation.
"""

import numpy as np


def readMemoryFile(filename):
    """Return a numpy array containing the allocation times.

    Args:
      filename: File containing the allocations

    Returns:
      A numpy array containing [[alloc1_time, alloc1], ...].
    """
    try:
        # Pandas is much faster than mumpy at reading CSV files
        import pandas as pd
        return pd.read_csv(filename, delimiter = ' ', header=None).values
    except ImportError, e:
        f = file(filename, 'r')
        return np.genfromtxt(f, delimiter = ' ')


def consumptionTimeline(memory):
    """Compute the total memory consumption timeline.
    """
    result = np.copy(memory)
    result[:, 2] = np.cumsum(memory[:, 2])
    return result


def allocationsCount(memory):
    """Return the total number of allocations.
    """
    return np.count_nonzero(memory['allocations'] > 0)


