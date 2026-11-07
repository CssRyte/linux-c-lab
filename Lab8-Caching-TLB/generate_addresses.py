
"""Generate a reproducible address trace with locality and occasional random access."""

import random

rng = random.Random(42)

COUNT = 2300
HOT_PAGES = 12
TOTAL_PAGES = 600

# More than 256 distinct pages can cause replacement.

with open("addresses.txt", "w", encoding="utf-8") as output:
    for i in range(COUNT):

        if i < 300:
            page = i

        elif rng.random() < 0.85:
            page = rng.randrange(HOT_PAGES)

        else:
            page = rng.randrange(TOTAL_PAGES)

        offset = rng.randrange(4096)

        output.write(
            f"0x{(page << 12) | offset:08X}\n"
        )

print(
    f"Wrote {COUNT} hexadecimal virtual addresses to addresses.txt"
)
