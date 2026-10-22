#!/usr/bin/env python3
"""check_profiles.py - every exe row id of a tier-1 family has a hand file (design 6.1 item 2, ME-76,
design 10 W2 acceptance 5 "every 15/02-15/05 id has a hand file (CI check by id)"). Python 3 stdlib.

    python -I tools/check_profiles.py --tier 2 3 4 5     # W2: 15/02 MAC 600, 15/03 MAC 250 family, 15/04 MAC 2000 family, 15/05 Roboscan 518/812/218
    python -I tools/check_profiles.py --tier 1 10        # the W1 families (what exe_profiles_check.py --check already enforces)

The id lists are 15-builtin-profiles-index.md (the note's "profiles (exe ids)" column); hand ids come from
src/fixlib/hand/*.c the way tools/exe_profiles_check.py reads them (imported, so one parser). Exit 1 names
every missing id; exit 0 prints the per-tier counts. This is the CTest row `profiles_ids`.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exe_profiles_check  # noqa: E402  (hand_ids, the same regex CI uses)

TIERS = {   # 15-builtin-profiles-index.md
    1:  ("15/01 MAC 500 / Roboscan 918", [12, 31, 24, 30]),
    2:  ("15/02 MAC 600", [11, 67]),
    3:  ("15/03 MAC 250 family", [28, 47, 39, 105, 112, 133, 48, 118]),
    4:  ("15/04 MAC 2000 family", [68, 84, 87, 88, 117, 161, 162, 188, 189, 190, 191]),
    5:  ("15/05 Roboscan 518 / 812 / 218", [3, 58, 1, 52, 2, 57, 41, 36, 33]),
    6:  ("15/06 Atomic 3000", [80, 114, 115]),
    7:  ("15/07 MiniMAC", [54, 65, 53, 66, 69]),
    8:  ("15/08 RoboColor", [13, 59, 14, 60, 15, 61, 37, 40]),
    9:  ("15/09 MAC 550 / 575 / 700", [110, 155, 116, 140]),
    10: ("15/10 generics", exe_profiles_check.W1_IDS[4:]),
}


def main(argv):
    if len(argv) < 3 or argv[1] != "--tier":
        print(__doc__)
        return 2
    tiers = [int(a) for a in argv[2:]]
    hand = exe_profiles_check.hand_ids()
    bad = 0
    for t in tiers:
        name, ids = TIERS[t]
        missing = [i for i in ids if i not in hand]
        for i in missing:
            print("%s: exe id %d has no hand file under src/fixlib/hand/" % (name, i))
        bad += len(missing)
        print("check_profiles: tier %d %s: %d of %d ids have a hand file" % (t, name, len(ids) - len(missing), len(ids)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
