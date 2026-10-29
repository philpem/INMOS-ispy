#!/usr/bin/env python3
"""Create a fresh native WIMP build tree; never overwrite a previous build."""
import argparse
from pathlib import Path
import shutil
import subprocess
from sprites import build as build_sprites


def export(destination, ccres):
    source = Path(__file__).resolve().parent
    if destination.exists():
        raise ValueError('Destination already exists: ' + str(destination))
    destination.mkdir(parents=True)
    for directory in ('c', 'h', 's', 'o', '!Ispy'):
        (destination / directory).mkdir()
    for extension in ('c', 'h'):
        for path in sorted(source.glob('*.' + extension)):
            if path.name.startswith('test_'):
                continue
            if len(path.stem) > 10:
                raise ValueError('Native filename exceeds ten characters: ' + path.name)
            shutil.copyfile(path, destination / extension / path.stem)
    shutil.copyfile(source / 'Makefile', destination / 'Makefile,fe1')
    root = source.parent
    for name in ('inmos', 'linkio', 'checklib', 'cklib', 'iserver', 'type16',
                 'type32', 'check16', 'check32', 'c00416', 'c00432', 'boot',
                 'size16', 'size32'):
        shutil.copyfile(root / (name+'.h'), destination / 'h' / name)
    for original, name in ((root/'check.c', 'discover'),
                           (root/'cklib.c', 'support'),
                           (root/'riscos/ro_link.c', 'driver')):
        shutil.copyfile(original, destination / 'h' / name)
    shutil.copyfile(root/'riscos/tmblink.s', destination/'s/tmblink')
    app = destination / '!Ispy'
    for suffix, vga in (('', False), ('22', True)):
        (app / ('!Sprites'+suffix+',ff9')).write_bytes(build_sprites(vga))
        (app / ('Sprites'+suffix+',ff9')).write_bytes(build_sprites(vga, private=True))
    subprocess.run([ccres, str(source / 'windows.ccres'),
                    str(app / 'Templates,fec')], check=True)
    (app / '!Boot,feb').write_text(
        'Set Ispy$Dir <Obey$Dir>\nIconSprites <Obey$Dir>.!Sprites\n')
    (app / '!Run,feb').write_text(
        'Set Ispy$Dir <Obey$Dir>\n'
        'IconSprites <Obey$Dir>.!Sprites\n'
        'WimpSlot -min 640K -max 640K\n'
        'Run <Ispy$Dir>.!RunImage %*0\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--ccres', default='ccres')
    args = parser.parse_args()
    try:
        export(args.destination, args.ccres)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
    print('Exported WIMP sources and application resources to', args.destination)


if __name__ == '__main__':
    main()
