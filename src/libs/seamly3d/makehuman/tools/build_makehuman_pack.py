#!/usr/bin/env python3
"""
Build the compact MakeHuman body data pack used by the Seamly2D 3D workspace.

Reads a MakeHuman 1.x checkout (https://github.com/makehumancommunity/makehuman)
and writes the CC0 assets the body fitter needs into one zlib compressed file:

  * the base mesh body (group "body": vertices and quads)
  * adult ("young") macro targets: universal, ethnic, height, breast
  * the measure targets (measure-*-decr / -incr)

The assets are CC0 1.0 (see LICENSE.ASSETS.md in the MakeHuman repository).

Usage:
    python3 build_makehuman_pack.py <makehuman checkout> <output file>

Format (little endian, after qUncompress):
    char[4]  magic "SMHB"
    uint32   version (1)
    uint32   vertex count V, float32[3V] positions in decimeters (MakeHuman units)
    uint32   quad count Q, uint16[4Q] vertex indices
    uint32   target count T, then per target:
        uint16 name length, utf-8 name ("macrodetails/universal-female-young-...")
        uint32 entry count N, uint16[N] vertex indices, int16[3N] deltas in 1/4096 decimeter
"""

import os
import struct
import sys
import zlib

DELTA_SCALE = 4096.0
MAGIC = b'SMHB'
VERSION = 1


def read_body(obj_path):
    positions = []
    quads = []
    group = None
    with open(obj_path, encoding='utf-8') as obj:
        for line in obj:
            parts = line.split()
            if not parts:
                continue
            if parts[0] == 'v':
                positions.append(tuple(float(p) for p in parts[1:4]))
            elif parts[0] == 'g':
                group = parts[1] if len(parts) > 1 else None
            elif parts[0] == 'f' and group == 'body':
                face = [int(p.split('/')[0]) - 1 for p in parts[1:]]
                if len(face) != 4:
                    raise ValueError('body faces are expected to be quads')
                quads.append(face)

    used = max(max(q) for q in quads) + 1
    if any(min(q) < 0 for q in quads):
        raise ValueError('negative face index')
    return positions[:used], quads


def read_target(path, vertex_count):
    entries = []
    with open(path, encoding='utf-8') as target:
        for line in target:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            index = int(parts[0])
            if index >= vertex_count:
                continue  # helper geometry, not part of the body
            delta = [float(p) for p in parts[1:4]]
            quantized = [int(round(d * DELTA_SCALE)) for d in delta]
            if any(abs(q) > 32767 for q in quantized):
                raise ValueError('delta out of range in %s' % path)
            if any(quantized):
                entries.append((index, quantized))
    entries.sort()
    return entries


def target_files(data_dir):
    targets = os.path.join(data_dir, 'targets')
    selected = []

    def add(subdir, prefix, predicate):
        directory = os.path.join(targets, subdir)
        for name in sorted(os.listdir(directory)):
            if name.endswith('.target') and predicate(name):
                selected.append((prefix + name[:-len('.target')], os.path.join(directory, name)))

    is_adult = lambda name: '-young' in name
    add('macrodetails', 'macrodetails/', is_adult)
    add('macrodetails/height', 'macrodetails-height/', is_adult)
    add('breast', 'breast/', lambda name: is_adult(name) and not name.startswith('breast-'))
    add('measure', 'measure/', lambda name: True)
    return selected


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    data_dir = os.path.join(sys.argv[1], 'makehuman', 'data')
    positions, quads = read_body(os.path.join(data_dir, '3dobjs', 'base.obj'))

    out = bytearray()
    out += MAGIC
    out += struct.pack('<I', VERSION)
    out += struct.pack('<I', len(positions))
    for p in positions:
        out += struct.pack('<3f', *p)
    out += struct.pack('<I', len(quads))
    for q in quads:
        out += struct.pack('<4H', *q)

    targets = target_files(data_dir)
    out += struct.pack('<I', len(targets))
    total = 0
    for name, path in targets:
        entries = read_target(path, len(positions))
        encoded = name.encode('utf-8')
        out += struct.pack('<H', len(encoded)) + encoded
        out += struct.pack('<I', len(entries))
        out += struct.pack('<%dH' % len(entries), *[e[0] for e in entries])
        out += struct.pack('<%dh' % (3 * len(entries)), *[c for e in entries for c in e[1]])
        total += len(entries)

    # qUncompress() format: 4 byte big endian uncompressed size, then a zlib stream.
    packed = struct.pack('>I', len(out)) + zlib.compress(bytes(out), 9)
    with open(sys.argv[2], 'wb') as output:
        output.write(packed)

    print('%d vertices, %d quads, %d targets, %d entries, %d bytes (%d uncompressed)'
          % (len(positions), len(quads), len(targets), total, len(packed), len(out)))


if __name__ == '__main__':
    main()
