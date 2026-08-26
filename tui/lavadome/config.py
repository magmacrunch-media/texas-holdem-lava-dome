"""The tuned numbers.

Transcribed from ``web/js/config.js``, which the repo's ``AGENTS.md`` names as
the source of truth for rules and tuning. Changing a number here is a gameplay
change and is not done until every version has it.

The depth labels and flavour lines are magmacrunch song titles — in-house
material, carried over as-is.
"""

from __future__ import annotations

# ── Chips ───────────────────────────────────────────────────────────

STARTING_CHIPS = 500
MIN_BET = 10
MAX_BET_MULTIPLIER = 10

# ── Ante schedule ───────────────────────────────────────────────────
# Paid at the start of each round; index is the round number, 0-based. Past the
# end of the schedule the last value repeats and climbs by the escalation rate.

ANTE_SCHEDULE = (
    10,   # Round 1  — Lithosphere
    10,   # Round 2
    20,   # Round 3  — Contemplate the Plate Tectonic
    20,   # Round 4
    30,   # Round 5  — Figure the Shoreline
    30,   # Round 6
    50,   # Round 7  — Penultimate Drop
    50,   # Round 8
    75,   # Round 9  — Pendant Stop
    75,   # Round 10
    100,  # Round 11 — Hazardous Metals in Ambient Air
    125,  # Round 12
    150,  # Round 13 — I would go up to the hot lava
    200,  # Round 14 — Millstone, 2063
    250,  # Round 15+ — All All & All
)
ANTE_ESCALATION_RATE = 25

# ── Depth labels ────────────────────────────────────────────────────

DOME_DEPTHS = (
    (1,  "I keep my cards close to my heart"),
    (2,  "Eager for second chances"),
    (3,  "Contemplate the Plate Tectonic"),
    (4,  "Contemplate the Plate Tectonic"),
    (5,  "Figure the Shoreline"),
    (6,  "Figure the Shoreline"),
    (7,  "Penultimate Drop"),
    (8,  "Penultimate Drop"),
    (9,  "Pendant Stop"),
    (10, "Pendant Stop"),
    (11, "Hazardous Metals in Ambient Air"),
    (12, "Hazardous Metals in Ambient Air"),
    (13, "I Would Go Up to the Hot Lava"),
    (14, "Millstone, 2063"),
    (15, "All All & All"),
)

# ── Threshold ───────────────────────────────────────────────────────
# The hand must score at least this to beat the dome. Climbs with the round.

DOME_BASE_THRESHOLD = 10    # round 1 — One Pair territory
DOME_THRESHOLD_SCALE = 5    # added per round

# ── Payouts ─────────────────────────────────────────────────────────
# Beat the dome and the bet comes back plus bet * multiplier.

PAYOUT_MULTIPLIERS = {
    "Royal Flush": 10,
    "Straight Flush": 6,
    "Four of a Kind": 4,
    "Full House": 3,
    "Flush": 2.5,
    "Straight": 2,
    "Three of a Kind": 1.5,
    "Two Pair": 1.25,
    "One Pair": 1,
    # Zero on purpose, and it is load-bearing: a high card scores 0 points, so
    # it cannot clear any threshold, and it would pay nothing if it did.
    "High Card": 0,
}

STREETS = ("hole", "flop", "turn", "river")

# ── Flavour ─────────────────────────────────────────────────────────

FLAVOR_BUST = (
    "The dome has claimed another soul.",
    "Bus full of time-traveling twenty-somethings — and you.",
    "What happened to you in all the confusion?",
    "The dome is not forgiving.",
    "Millstone, 2063. That's you now.",
)

FLAVOR_WIN_BIG = (
    "I keep my cards close to my heart.",
    "Maybe the instruments failed and maybe they didn't.",
    "This has always been true.",
    "Ocean of storms — you surfed it.",
    "Hello World, Love Space.",
)

FLAVOR_ESCAPE = (
    "You escaped the dome. For now.",
    "Friendship 7 4 fun — and profit.",
    "A pine tree caught electrical fire, but not you.",
    "Secret conference rooms await.",
    "Rendezvous at 44i boo. Mission complete.",
)


def ante_for_round(round_number: int) -> int:
    """The ante due in ``round_number`` (1-based).

    Past the end of the schedule the last value repeats, climbing by
    ``ANTE_ESCALATION_RATE`` for each round beyond it.
    """
    if round_number < 1:
        return ANTE_SCHEDULE[0]
    idx = round_number - 1
    if idx < len(ANTE_SCHEDULE):
        return ANTE_SCHEDULE[idx]
    extra = idx - (len(ANTE_SCHEDULE) - 1)
    return ANTE_SCHEDULE[-1] + extra * ANTE_ESCALATION_RATE


def threshold_for_round(round_number: int) -> int:
    return DOME_BASE_THRESHOLD + (round_number - 1) * DOME_THRESHOLD_SCALE


def depth_label(round_number: int) -> str:
    """The deepest label whose round has been reached."""
    label = DOME_DEPTHS[0][1]
    for at_round, text in DOME_DEPTHS:
        if round_number >= at_round:
            label = text
    return label
