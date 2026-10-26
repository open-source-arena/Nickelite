#!/usr/bin/env python3
"""Embed user-storage resources in a mod library."""
from pathlib import Path
import sys


def main():
    output, *sources = map(Path, sys.argv[1:])
    lines = ['/* Generated from mod resources. */', '#include "resources.h"']
    names = set()
    entries = []
    for index, source in enumerate(sources):
        name = source.name
        if name in names or not all(c.isascii() and (c.isalnum() or c in '._-') for c in name):
            raise SystemExit('Invalid or duplicate resource name: ' + name)
        names.add(name)
        data = source.read_bytes()
        symbol = 'nh_resource_' + str(index)
        lines.append('static const unsigned char ' + symbol + '[] = {')
        for start in range(0, len(data), 16):
            lines.append('    ' + ', '.join(str(byte) for byte in data[start:start + 16]) + ',')
        lines.append('    0};')
        entries.append('    {"' + name + '", ' + symbol + ', ' + str(len(data)) + '},')
    lines += ['static const struct nh_resource nh_embedded_resources[] = {',
              *entries, '    {NULL, NULL, 0}};', '']
    output.write_text('\n'.join(lines))


if __name__ == '__main__':
    main()
