#!/usr/bin/env python3
"""Copy the Unix host sources into a separate Acorn c/h/s/o layout."""

import argparse
from pathlib import Path
import shutil


def export(root, destination):
    root = root.resolve()
    destination = destination.resolve()
    files = []
    for source in sorted(root.glob('*.c')):
        if source.name != 'b004link.c':
            files.append((source, Path('c') / source.stem))
    for source in sorted(root.glob('*.h')):
        if source.name != 'link-driver.h':
            files.append((source, Path('h') / source.stem))
    for source in sorted((root / 'riscos').glob('*')):
        if source.suffix in ('.c', '.s'):
            files.append((source, Path(source.suffix[1:]) / source.stem))
    files.append((root / 'riscos/Makefile', Path('Makefile')))

    # RISC OS 3.1 filing systems use names of at most ten characters.
    names = set()
    for source, relative in files:
        if len(relative.name) > 10:
            raise ValueError('Acorn filename exceeds ten characters: ' + str(relative))
        key = str(relative).lower()
        if key in names:
            raise ValueError('Acorn filename collision: ' + str(relative))
        names.add(key)
        if (destination / relative).resolve() == source.resolve():
            raise ValueError('Export would overwrite a source file: ' + str(source))

    # Export is a fresh snapshot, so stale files cannot enter a native build.
    # Never remove or overwrite an existing export or another directory.
    destination.mkdir(parents=True, exist_ok=False)
    for directory in ('c', 'h', 's', 'o'):
        (destination / directory).mkdir()
    for source, relative in files:
        shutil.copyfile(source, destination / relative)
    return len(files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', nargs='?', type=Path,
                        default=Path('build/riscos'),
                        help='new output directory (default: build/riscos)')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    try:
        count = export(root, args.destination)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
    print('Exported {} files to {}'.format(count, args.destination))


if __name__ == '__main__':
    main()
