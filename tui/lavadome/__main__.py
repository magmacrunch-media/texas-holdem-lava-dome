"""``python -m lavadome`` — play Texas Hold'Em Lava Dome in a terminal."""

from __future__ import annotations

import argparse


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="lavadome",
        description="Texas Hold'Em Lava Dome - terminal version.",
    )
    parser.add_argument(
        "--play", action="store_true",
        help="skip the title screen and descend straight into a run",
    )
    parser.add_argument(
        "--seed", type=int, default=None,
        help="fix the shuffle, for a reproducible run",
    )
    parser.add_argument(
        "--ascii", action="store_true", dest="ascii_only",
        help="draw with plain ASCII instead of block, arrow and suit "
             "glyphs. Detected automatically from the terminal's "
             "encoding; this forces it, for a font that lacks the "
             "pictures. MAGMACRUNCH_ASCII=1 says the same for every "
             "cabinet at once.",
    )
    args = parser.parse_args()

    # Imported here, not at module scope, so --help works without the engine
    # or its terminal extra installed.
    from lavadome.app import run

    run(args.seed, args.ascii_only, args.play)


if __name__ == "__main__":
    main()
