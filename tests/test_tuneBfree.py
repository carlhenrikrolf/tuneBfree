"""
Tests for tuneBfree

Uses the mtsespy Python wrapper available at
https://github.com/narenratan/mtsespy
"""
import subprocess
from pathlib import Path

import pytest

import mtsespy as mts
from mtsespy import scala_files_to_frequencies

REPO_DIR = Path(__file__).parents[1]

SCL_FILES = [
    str(x) for x in (REPO_DIR / "tests/scala_scale_archive/scl/").glob("*.scl")
]

mts.reinitialize()


@pytest.mark.parametrize("scl_file", SCL_FILES)
def test_cli(scl_file):
    """
    Test that running the CLI doesn't crash.

    Run CLI under an MTS-ESP master with tunings from different scala
    files. Check that the CLI process doesn't crash.
    """
    freqs = scala_files_to_frequencies(scl_file)
    with mts.Master():
        mts.set_note_tunings(freqs)
        subprocess.check_output(REPO_DIR / "build/tuneBfree", env={"NO_JACK": "TRUE"})
