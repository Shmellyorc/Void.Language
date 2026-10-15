"""Fail-closed audit of this probe's pinned LLVM MIR format, not production API.

Production integration must use MachineFrameInfo and AllocaInst identities. Text
parsing here supports only the fixed, single-function experiment's machine output.
"""
import re

BUDGET = 65536


def _integer(text, name):
    values = re.findall(r'^  ' + name + r':\s+(-?\d+)\s*$', text, re.M)
    if len(values) != 1:
        raise ValueError('missing/ambiguous machine field ' + name)
    return int(values[0])


def inspect(text, homes, entry_area_bytes):
    if entry_area_bytes < 0:
        raise ValueError('unproved entry-area extent')
    frame = _integer(text, 'stackSize')
    outgoing = _integer(text, 'maxCallFrameSize')
    if frame < 0 or outgoing < 0 or outgoing > frame:
        raise ValueError('unsupported frame/call area')
    if _integer(text, 'maxAlignment') > 16:
        raise ValueError('unproved stack realignment')
    for flag in ('hasOpaqueSPAdjustment', 'hasVarSizedObjects', 'hasTailCall'):
        if re.search(r'^  ' + flag + r':\s+true', text, re.M):
            raise ValueError('dynamic/opaque stack adjustment')
    objects = []
    for section in ('fixedStack', 'stack'):
        match = re.search(r'^' + section + r':\s*\n(.*?)(?=^[a-zA-Z]|\Z)', text, re.M | re.S)
        if not match:
            if re.search(r'^' + section + r':\s*\[\s*\]', text, re.M):
                continue
            raise ValueError('missing machine object section ' + section)
        for record in re.findall(r'\{([^}]+)\}', match.group(1), re.S):
            fields = dict(re.findall(r'([\w-]+):\s*([^,\n]+)', record))
            if fields.get('type') not in ('default', 'spill-slot'):
                raise ValueError('unsupported machine object kind')
            objects.append(dict(section=section, name=fields.get('name', "''").strip("'\""),
                                kind=fields['type'], size=int(fields['size']),
                                offset=int(fields['offset']), alignment=int(fields['alignment'])))
    known, unknown, incoming, spills = [], [], [], []
    seen = set()
    for obj in objects:
        if obj['size'] < 0 or obj['alignment'] <= 0 or obj['alignment'] & (obj['alignment'] - 1):
            raise ValueError('unsupported object extent/alignment')
        if obj['section'] == 'fixedStack' and obj['kind'] == 'default':
            incoming.append(obj)
        elif obj['kind'] == 'spill-slot':
            spills.append(obj)
        elif obj['name'] in homes:
            name = obj['name']
            if name in seen or obj['size'] != homes[name]['bytes'] or obj['alignment'] < homes[name]['alignment']:
                raise ValueError('ambiguous/inexact planned object ' + name)
            seen.add(name)
            known.append(obj)
        else:
            unknown.append(obj)
    for i, first in enumerate(known):
        for second in known[i + 1:]:
            if max(first['offset'], second['offset']) < min(first['offset'] + first['size'], second['offset'] + second['size']):
                raise ValueError('overlapping planned homes not proved')
    represented = sum(obj['size'] for obj in known)
    if represented > frame:
        raise ValueError('planned objects exceed measured frame')
    # Target i32 layout=4/4 and stack alignment=16 are asserted by a separate
    # target-compiled layout witness. No host sizeof or measured fixed allowance.
    cursor = 0
    for extent in homes.values():
        a = extent['alignment']
        if a <= 0 or a & (a - 1) or extent['bytes'] < 0:
            raise ValueError('invalid preoptimization home')
        cursor = (cursor + a - 1) & -a
        cursor += extent['bytes']
    pre = (cursor + 15) & -16
    # F includes outgoing area, callee saves, spills and frame padding already.
    # Count the original explicit plan, then only F minus its represented homes.
    # Missing optimized homes earn no credit against the original plan.
    residual = frame - represented
    incoming_bytes = sum(obj['size'] for obj in incoming)
    covered = not unknown and not incoming
    total = ((cursor + residual + entry_area_bytes + 15) & -16) if covered else None
    return dict(frame_bytes=frame, entry_area_bytes=entry_area_bytes, outgoing_bytes=outgoing, preoptimization_bytes=pre,
                represented_home_bytes=represented, measured_nonhome_bytes=residual,
                incoming_slot_bytes=incoming_bytes, incoming_slots=incoming, spills=spills,
                unknown_homes=unknown, missing_homes=sorted(set(homes) - seen),
                conservative_total_bytes=total, within_budget=(total <= BUDGET if covered else None),
                covered=covered)


def require_covered(report):
    # These signatures fit in registers. Incoming slots would need a separate
    # ownership/extent proof, rather than accepting a scalar sum as a general ABI.
    if not report['covered']:
        raise ValueError('unaccounted machine home or incoming call storage')
