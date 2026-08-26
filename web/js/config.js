// config.js - Texas Hold'Em Lava Dome | MagmaCrunch Media © 2024

// ── Score backend is now handled by ScoreClient (MAGMA//OPS dashboard) ──
// No external API keys needed

// ── Cards and hand evaluation ────────────────────────────────
// All of it now comes from AdCards (../shared/adenosine-cards.js):
//   - AdCards.HandEvaluator replaces the old js/hand-eval.js. Verified
//     behaviourally identical over 20,000 random 2-7 card hands.
//   - AdCards.HAND_RANKS and AdCards.HAND_POINTS hold the values that used to be
//     declared here; they match exactly.
//   - Card constants (SUITS, RANKS, SUIT_SYMBOLS, SUIT_COLORS, RANK_VALUES) were
//     declared here but read by nothing; AdCards carries its own for rendering.
// AdCards.RANK_VALUES is ace-LOW (A === 1) while this game is ace-high, and the
// evaluator reads `value` off the card objects it is given without rewriting it.
// Dealer._draw() restamps every dealt card ace-high, which is what makes aces
// score correctly here — see js/dealer.js.

// ── Chip configuration ───────────────────────────────────────
const STARTING_CHIPS     = 500;   // Chip stack at session start
const MIN_BET            = 10;    // Minimum bet per street
const MAX_BET_MULTIPLIER = 10;    // Max bet = ante * this

// ── Dome ante schedule ───────────────────────────────────────
// Ante paid at the start of each round (index = round number, 0-based)
// After the schedule runs out, the last value repeats + ANTE_ESCALATION_RATE
const ANTE_SCHEDULE = [
    10,   // Round 1  — Lithosphere
    10,   // Round 2
    20,   // Round 3  — Contemplate the Plate Tectonic
    20,   // Round 4
    30,   // Round 5  — Figure the Shoreline
    30,   // Round 6
    50,   // Round 7  — Penultimate Drop
    50,   // Round 8
    75,   // Round 9  — Pendant Stop
    75,   // Round 10
    100,  // Round 11 — Hazardous Metals in Ambient Air
    125,  // Round 12
    150,  // Round 13 — I would go up to the hot lava
    200,  // Round 14 — Millstone, 2063
    250   // Round 15+ — All All & All (repeats, +25 each round after)
];
const ANTE_ESCALATION_RATE = 25; // Added per round beyond schedule

// ── Dome depth labels (themed after band songs) ──────────────
// Shown as flavor text for current round depth
const DOME_DEPTHS = [
    { round: 1,  label: 'I keep my cards close to my heart'                       },
    { round: 2,  label: 'Eager for second chances'                       },
    { round: 3,  label: 'Contemplate the Plate Tectonic'    },
    { round: 4,  label: 'Contemplate the Plate Tectonic'    },
    { round: 5,  label: 'Figure the Shoreline'             },
    { round: 6,  label: 'Figure the Shoreline'             },
    { round: 7,  label: 'Penultimate Drop'                  },
    { round: 8,  label: 'Penultimate Drop'                  },
    { round: 9,  label: 'Pendant Stop'                      },
    { round: 10, label: 'Pendant Stop'                       },
    { round: 11, label: 'Hazardous Metals in Ambient Air'   },
    { round: 12, label: 'Hazardous Metals in Ambient Air'   },
    { round: 13, label: 'I Would Go Up to the Hot Lava'    },
    { round: 14, label: 'Millstone, 2063'                   },
    { round: 15, label: 'All All & All'                      }
];

// ── Dome point threshold ─────────────────────────────────────
// Your hand must score >= this to "beat" the dome each round
// Scales with round number
const DOME_BASE_THRESHOLD  = 10;   // Round 1 threshold (One Pair territory)
const DOME_THRESHOLD_SCALE = 5;    // Added per round

// ── Payout multipliers ───────────────────────────────────────
// Win: get back bet * multiplier based on hand strength
const PAYOUT_MULTIPLIERS = {
    'Royal Flush':    10,
    'Straight Flush': 6,
    'Four of a Kind': 4,
    'Full House':     3,
    'Flush':          2.5,
    'Straight':       2,
    'Three of a Kind':1.5,
    'Two Pair':       1.25,
    'One Pair':       1,
    'High Card':      0   // Can't beat the dome with high card
};

// ── Street names ─────────────────────────────────────────────
const STREETS = ['hole', 'flop', 'turn', 'river'];

// ── Flavor text ──────────────────────────────────────────────
// Random quips shown at various moments
const FLAVOR_BUST = [
    'The dome has claimed another soul.',
    'Bus full of time-traveling twenty-somethings — and you.',
    'What happened to you in all the confusion?',
    'The dome is not forgiving.',
    'Millstone, 2063. That\'s you now.'
];

const FLAVOR_WIN_BIG = [
    'I keep my cards close to my heart.',
    'Maybe the instruments failed and maybe they didn\'t.',
    'This has always been true.',
    'Ocean of storms — you surfed it.',
    'Hello World, Love Space.'
];

const FLAVOR_ESCAPE = [
    'You escaped the dome. For now.',
    'Friendship 7 4 fun — and profit.',
    'A pine tree caught electrical fire, but not you.',
    'Secret conference rooms await.',
    'Rendezvous at 44i boo. Mission complete.'
];

// ── Lava theme colors (reference) ───────────────────────────
const LAVA_COLORS = {
    black:       '#0a0000',
    darkRed:     '#1c0000',
    deepRed:     '#3b0000',
    lavaDark:    '#6b0000',
    lavaMid:     '#9b0000',
    lavaBright:  '#cc2200',
    orange:      '#dd4400',
    orangeHot:   '#ff5500',
    orangeGlow:  '#ff7700',
    yellow:      '#ffcc00',
    yellowPale:  '#ffe680',
    white:       '#fff8f0'
};
