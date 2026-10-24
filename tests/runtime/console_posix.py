"""Compare configured POSIX terminal attributes without discarding raw evidence."""
import sys
import termios

def same_termios(before, after):
    # XNU TIOCSETA adds PENDIN on a raw -> canonical transition. Leave it set:
    # it preserves pending input. Every configured flag/cc/speed still matches.
    difference = before[3] ^ after[3]
    if sys.platform == 'darwin':
        difference &= ~termios.PENDIN
    return before[:3] == after[:3] and not difference and before[4:] == after[4:]

def mode_record(mode):
    return dict(iflag=mode[0], oflag=mode[1], cflag=mode[2], lflag=mode[3],
                ispeed=mode[4], ospeed=mode[5],
                cc=[unit.hex() if isinstance(unit, bytes) else unit for unit in mode[6]])
