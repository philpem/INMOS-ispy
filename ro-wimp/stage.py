#!/usr/bin/env python3
"""Refresh the reusable NFS wimp build; record source hashes and a build job."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from export import export


def stage(share, run_id, ccres):
    if not re.fullmatch('[a-z0-9]{1,6}', run_id):
        raise ValueError('Run ID must be one to six lower-case letters/digits')
    logs = share / 'logs'
    manifest = logs / (run_id+'src')
    if any(logs.glob(run_id+'*')):
        raise ValueError('Run ID already used; choose a fresh ID for the logs')
    destination = share / 'wimp'
    logs.mkdir(parents=True, exist_ok=True)
    hashes = {}
    with tempfile.TemporaryDirectory() as scratch:
        fresh = Path(scratch) / 'export'
        export(fresh, ccres)
        shutil.copytree(fresh, destination, dirs_exist_ok=True)
        for source in sorted(fresh.rglob('*')):
            if source.is_file():
                relative = source.relative_to(fresh)
                data = source.read_bytes()
                if (destination / relative).read_bytes() != data:
                    raise OSError('Copied bytes differ: ' + str(relative))
                hashes[str(relative)] = hashlib.sha256(data).hexdigest()
        # Force native recompilation; never leave an old runnable binary after
        # a failed build. Only delete this build's known generated outputs.
        stems = {path.name for directory in ('c', 's')
                 if (fresh / directory).exists()
                 for path in (fresh / directory).iterdir()}
        for output in (destination / 'o').iterdir():
            if output.is_file() and output.name.split(',')[0] in stems:
                output.unlink()
        for output in (destination / '!Ispy').iterdir():
            if output.is_file() and output.name.split(',')[0] == '!RunImage':
                output.unlink()
    manifest.write_text(json.dumps({'directory': 'wimp', 'sha256': hashes}, indent=2)+'\n')
    (logs / (run_id+'run,feb')).write_text(
        'Dir NFS::Home.$.ispy.wimp\n'
        'Spool NFS::Home.$.ispy.logs.'+run_id+'out\n'
        'amu\nShow Sys$ReturnCode\nSpool\n'
        'Create NFS::Home.$.ispy.logs.'+run_id+'done 0\n')
    print('Refreshed and byte-verified', destination)
    print('Build job:', run_id+'run', '; manifest:', manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_id', help='unique short ID for logs, not a build directory')
    parser.add_argument('--share', type=Path, default=Path('/mnt/nfs-riscos/ispy'))
    parser.add_argument('--ccres', default='ccres')
    args = parser.parse_args()
    try:
        stage(args.share, args.run_id, args.ccres)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error)+'\n')


if __name__ == '__main__':
    main()
