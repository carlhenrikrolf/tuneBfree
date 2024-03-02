"""
Tests for tuneBfree

Not using mtsespy so can run even if mtsespy not available.
"""
import subprocess
from pathlib import Path
from sys import platform

REPO_DIR = Path(__file__).parents[1]


def test_cli_no_mts_master():
    """
    Test that running the CLI doesn't crash.

    Run CLI without an MTS-ESP master. Check that the CLI process doesn't crash.
    """
    extension = ".exe" if platform == "win32" else ""
    subprocess.check_output(
        REPO_DIR / f"build/tuneBfree{extension}", env={"NO_JACK": "TRUE"}
    )
