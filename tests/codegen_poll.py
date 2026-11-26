#!/usr/bin/env python3
"""Waiting loops in the translator's output: which loops tell the runtime about their turns (spin_turn).

A waiting loop is a branch back over a straight line that only reads memory and works on general registers, with calls of
functions of the same kind in it (recomp/codegen.cpp, poll_loop); THPS4 waits in `do {} while (get_flag())`. At the end of
every turn that goes round again the translator writes a call of spin_turn with the registers a turn writes, and where the
loop is left it calls spin_leave; the runtime skips whole turns once they repeat exactly (runtime/src/hw.cpp). This test feeds the translator made-up programs
(no game data) and checks that the call is written for such loops and for nothing else: a store, a system call, a
multiplication, a branch inside the loop, a called function that stores or does not return straight away, a forward
branch, a loop that steps a register (a strlen, a delay loop: it never repeats); PS2_EE_POLL=0 never writes it. It also checks the register mask of the call.
"""
from pathlib import Path
import argparse
import os
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--recompiler', type=Path, default=root / 'build/tools/eerecomp')
args = parser.parse_args()
BASE = 0x100000

NOP = 0
JR_RA = 0x03E00008
SYSCALL = 0x0000000C
V0, V1, A0, T0 = 2, 3, 4, 8
def addiu(rt, rs, imm): return 0x24000000 | rs << 21 | rt << 16 | imm & 0xFFFF
def lui(rt, imm): return 0x3C000000 | rt << 16 | imm & 0xFFFF
def bne(rs, rt, off): return 0x14000000 | rs << 21 | rt << 16 | off & 0xFFFF
def beq(rs, rt, off): return 0x10000000 | rs << 21 | rt << 16 | off & 0xFFFF
def lw(rt, base, off=0): return 0x8C000000 | base << 21 | rt << 16 | off & 0xFFFF
def sw(rt, base, off=0): return 0xAC000000 | base << 21 | rt << 16 | off & 0xFFFF
def andi(rt, rs, imm): return 0x30000000 | rs << 21 | rt << 16 | imm & 0xFFFF
def mult(rs, rt): return rs << 21 | rt << 16 | 0x18
def jal(index): return 0x0C000000 | ((BASE + 4 * index) >> 2) & 0x03FFFFFF
END = [JR_RA, NOP]

def elf(words):
    data = bytearray(0x100 + 4 * len(words))
    data[:7] = b'\x7fELF\x01\x01\x01'
    struct.pack_into('<HHIIIIIHHHHHH', data, 16, 2, 8, 1, BASE, 52, 0, 0, 52, 32, 1, 40, 0, 0)
    struct.pack_into('<8I', data, 52, 1, 0x100, BASE, 0, 4 * len(words), 4 * len(words), 5, 4)
    struct.pack_into(f'<{len(words)}I', data, 0x100, *words)
    return bytes(data)

def translate(work, name, words, env=None):
    (work / f'{name}.elf').write_bytes(elf(words))
    out = work / f'{name}-out'
    result = subprocess.run([str(args.recompiler.resolve()), str(work / f'{name}.elf'), '--out', str(out)], capture_output=True, text=True,
                            env={**os.environ, **(env or {})})
    assert result.returncode == 0, result.stderr
    return '\n'.join(unit.read_text() for unit in sorted(out.glob('*unit*.cpp')))

def masks(text): return [int(m) for m in re.findall(r'spin_turn\(c, [^,]+, (\d+)u\)', text)]

def bits(*regs): return sum(1 << r for r in regs)

# the getter of THPS4's loop: lui v1, hi; jr ra; lw v0, lo(v1)
GETTER = [lui(V1, 0x10), JR_RA, lw(V0, V1, 0x40)]
# name, the program, the masks of the spin_turn calls expected
cases = [
    ('a load and a test', [lw(V0, A0), bne(V0, 0, -2), NOP] + END, [bits(V0)]),
    ('a load, a mask and a test, the load in the delay slot', [andi(T0, V0, 4), bne(T0, 0, -2), lw(V0, A0)] + END, [bits(V0, T0)]),
    # THPS4: loop: jal get; nop; bne v0, $0, loop; nop; jr ra; nop; get: ...
    ('a call of a getter', [jal(6), NOP, bne(V0, 0, -3), NOP] + END + GETTER, [bits(V0, V1, 31)]),
    ('a store in the loop', [lw(V0, A0), sw(V0, A0, 4), bne(V0, 0, -3), NOP] + END, []),
    ('a store in the delay slot', [lw(V0, A0), bne(V0, 0, -2), sw(V0, A0, 4)] + END, []),
    ('a system call in the loop', [lw(V0, A0), SYSCALL, bne(V0, 0, -3), NOP] + END, []),
    ('a multiplication in the loop', [lw(V0, A0), mult(V0, V0), bne(V0, 0, -3), NOP] + END, []),
    ('a branch inside the loop', [lw(V0, A0), beq(V0, A0, 1), NOP, bne(V0, 0, -4), NOP] + END, []),
    ('a getter that stores', [jal(6), NOP, bne(V0, 0, -3), NOP] + END + [sw(V0, A0), JR_RA, lw(V0, A0)], []),
    ('a getter that calls on', [jal(6), NOP, bne(V0, 0, -3), NOP] + END + [jal(10), NOP] + END + GETTER, []),
    ('a forward branch', [lw(V0, A0), bne(V0, 0, 1), NOP] + END + END, []),
    # a register the turn steps and writes nowhere else differs after every turn: never a waiting loop
    ('a walk through memory (strlen)', [lw(V0, A0), bne(V0, 0, -2), addiu(A0, A0, 4)] + END, []),
    ('a delay loop (codegen_countdown)', [addiu(V0, V0, -1), bne(V0, 0, -2), NOP] + END, []),
    ('a step taken back in the same turn', [addiu(V1, V1, 1), addiu(V1, V1, -1), lw(V0, A0), bne(V0, 0, -4), NOP] + END, [bits(V0, V1)]),
    ('a stepped register loaded again', [addiu(A0, A0, 4), lw(A0, V1), bne(A0, 0, -3), NOP] + END, [bits(A0)]),
]

with tempfile.TemporaryDirectory(prefix='ps2-poll-test-') as temp:
    work = Path(temp)
    problems = []
    for number, (name, words, expected) in enumerate(cases):
        text = translate(work, f'case{number}', words)
        got = masks(text)
        if got != expected:
            problems.append(f'{name}: spin_turn masks {got}, expected {expected}')
        # where the loop is left the next turn is a first one again (a strlen called again on the same string is no loop turning)
        if text.count('spin_leave();') != len(got):
            problems.append(f'{name}: {text.count("spin_leave();")} resets where the loop is left, expected {len(got)}')
        if masks(translate(work, f'off{number}', words, env={'PS2_EE_POLL': '0'})):
            problems.append(f'{name}: PS2_EE_POLL=0 still writes spin_turn')

for problem in problems:
    print('FAIL', problem)
print('ok' if not problems else f'FAILED: {len(problems)}')
raise SystemExit(1 if problems else 0)
