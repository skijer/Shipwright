"""Compile actual whip release/upper-action functions with narrow engine fixtures."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/nei_item_stow'))
from run_ballchain_tests import flags
with tempfile.TemporaryDirectory(prefix='nei-whip-aim-') as td:
    binary = str(Path(td) / 'whip')
    subprocess.run(['cc', *flags(), '-ffunction-sections', '-fdata-sections',
                    str(ROOT / 'tests/nei_whip/aim_pose_test.c'), '-Wl,--gc-sections', '-lm', '-o', binary], check=True)
    subprocess.run([binary], check=True)
