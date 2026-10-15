#!/usr/bin/env python3
"""
Build the compact MakeHuman body data pack used by the Seamly2D 3D workspace.

Reads a MakeHuman 1.x checkout (https://github.com/makehumancommunity/makehuman)
and writes the CC0 assets the body fitter needs into one zlib compressed file:

  * the base mesh body (group "body": vertices and quads)
  * the macro targets for every age: universal, ethnic, height, breast
  * the measure targets (measure-*-decr / -incr)
  * the modeling targets the fitter uses for lengths and landmark heights
  * "seamly/shoulders-up|down", made here from the default skeleton's skinning
    weights: every vertex moves up (or down) by how much the clavicle and arm
    bones move it, so the shoulder slope can change without a pose

The assets are CC0 1.0 (see LICENSE.ASSETS.md in the MakeHuman repository).

Usage:
    python3 build_makehuman_pack.py <makehuman checkout> <output file>

Format (little endian, after qUncompress):
    char[4]  magic "SMHB"
    uint32   version (2)
    float32  delta scale D (deltas are stored as round(delta * D))
    uint32   vertex count V, float32[3V] positions in decimeters (MakeHuman units)
    uint32   quad count Q, uint16[4Q] vertex indices
    uint32   target count T, then per target:
        uint16 name length, utf-8 name ("macrodetails/universal-female-young-...")
        uint32 entry count N, uint16[N] vertex index gaps (index[i] - index[i - 1], ascending),
        int16[N] x deltas, int16[N] y deltas, int16[N] z deltas
"""

import json
import os
import struct
import sys
import zlib

DELTA_SCALE = 2048.0  # 1/2048 dm = 0.05 mm steps, +-16 dm range (baby bodies move far)
MAGIC = b'SMHB'
VERSION = 2


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


# Modeling targets used by the fitter, as (directory, target names).
MODELING_TARGETS = [
    ('armslegs', ['lowerlegs-height-decr', 'lowerlegs-height-incr',
                  'upperlegs-height-decr', 'upperlegs-height-incr',
                  'l-foot-scale-vert-decr', 'l-foot-scale-vert-incr',
                  'r-foot-scale-vert-decr', 'r-foot-scale-vert-incr',
                  'l-lowerleg-scale-vert-decr', 'l-lowerleg-scale-vert-incr',
                  'r-lowerleg-scale-vert-decr', 'r-lowerleg-scale-vert-incr',
                  'l-upperarm-fat-decr', 'l-upperarm-fat-incr', 'r-upperarm-fat-decr', 'r-upperarm-fat-incr',
                  'l-lowerarm-fat-decr', 'l-lowerarm-fat-incr', 'r-lowerarm-fat-decr', 'r-lowerarm-fat-incr']),
    ('hip', ['hip-scale-vert-decr', 'hip-scale-vert-incr', 'hip-waist-down', 'hip-waist-up']),
    ('torso', ['torso-scale-vert-decr', 'torso-scale-vert-incr']),
    ('breast', ['breast-trans-down', 'breast-trans-up']),
    ('neck', ['neck-scale-vert-decr', 'neck-scale-vert-incr']),
    ('head', ['head-scale-horiz-decr', 'head-scale-horiz-incr', 'head-scale-depth-decr', 'head-scale-depth-incr']),
]

SHOULDER_BONE_PREFIXES = ('clavicle', 'shoulder01', 'upperarm', 'lowerarm', 'wrist', 'metacarpal', 'finger')
SHOULDER_LIFT = 0.25  # decimeters for a full weight


def target_files(data_dir):
    targets = os.path.join(data_dir, 'targets')
    selected = []

    def add(subdir, prefix, predicate):
        directory = os.path.join(targets, subdir)
        for name in sorted(os.listdir(directory)):
            if name.endswith('.target') and predicate(name):
                selected.append((prefix + name[:-len('.target')], os.path.join(directory, name)))

    add('macrodetails', 'macrodetails/', lambda name: True)
    add('macrodetails/height', 'macrodetails-height/', lambda name: True)
    add('breast', 'breast/', lambda name: not name.startswith('breast-'))
    add('measure', 'measure/', lambda name: True)
    for directory, names in MODELING_TARGETS:
        wanted = set(n + '.target' for n in names)
        add(directory, directory + '/', lambda name, wanted=wanted: name in wanted)
        missing = wanted - set(os.listdir(os.path.join(targets, directory)))
        if missing:
            raise ValueError('missing targets: %s' % ', '.join(sorted(missing)))
    return selected


def shoulder_targets(data_dir, vertex_count):
    with open(os.path.join(data_dir, 'rigs', 'default_weights.mhw'), encoding='utf-8') as f:
        weights = json.load(f)['weights']

    total = {}
    for bone, entries in weights.items():
        if bone.startswith(SHOULDER_BONE_PREFIXES):
            for index, weight in entries:
                if index < vertex_count:
                    total[index] = total.get(index, 0.0) + weight

    up = []
    down = []
    for index in sorted(total):
        lift = int(round(min(total[index], 1.0) * SHOULDER_LIFT * DELTA_SCALE))
        if lift:
            up.append((index, [0, lift, 0]))
            down.append((index, [0, -lift, 0]))
    return [('seamly/shoulders-down', down), ('seamly/shoulders-up', up)]


def write_target(out, name, entries):
    # Index gaps and separate x, y, z planes compress much better than raw interleaved data.
    encoded = name.encode('utf-8')
    out += struct.pack('<H', len(encoded)) + encoded
    out += struct.pack('<I', len(entries))
    previous = 0
    gaps = []
    for index, _ in entries:
        gaps.append(index - previous)
        previous = index
    out += struct.pack('<%dH' % len(entries), *gaps)
    for axis in range(3):
        out += struct.pack('<%dh' % len(entries), *[e[1][axis] for e in entries])


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)

    data_dir = os.path.join(sys.argv[1], 'makehuman', 'data')
    positions, quads = read_body(os.path.join(data_dir, '3dobjs', 'base.obj'))

    out = bytearray()
    out += MAGIC
    out += struct.pack('<I', VERSION)
    out += struct.pack('<f', DELTA_SCALE)
    out += struct.pack('<I', len(positions))
    for p in positions:
        out += struct.pack('<3f', *p)
    out += struct.pack('<I', len(quads))
    for q in quads:
        out += struct.pack('<4H', *q)

    targets = [(name, read_target(path, len(positions))) for name, path in target_files(data_dir)]
    targets += shoulder_targets(data_dir, len(positions))

    out += struct.pack('<I', len(targets))
    total = 0
    for name, entries in targets:
        write_target(out, name, entries)
        total += len(entries)

    # qUncompress() format: 4 byte big endian uncompressed size, then a zlib stream.
    packed = struct.pack('>I', len(out)) + zlib.compress(bytes(out), 9)
    with open(sys.argv[2], 'wb') as output:
        output.write(packed)

    print('%d vertices, %d quads, %d targets, %d entries, %d bytes (%d uncompressed)'
          % (len(positions), len(quads), len(targets), total, len(packed), len(out)))


if __name__ == '__main__':
    main()
