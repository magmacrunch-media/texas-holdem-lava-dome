/**
 * Evaluates hands with the real AdCards.HandEvaluator, for differential testing.
 *
 * The Python port in lavadome/handeval.py has to agree with the JavaScript
 * exactly or the same game scores differently in two places — and a poker
 * evaluator that is subtly wrong plays almost right, which is the hardest kind
 * of bug to notice. Rather than hand-copying a few assertions, this exposes the
 * actual browser evaluator so both can be run over thousands of random hands.
 *
 * That is the same method web/js/config.js records using when AdCards replaced
 * this game's original evaluator: "Verified behaviourally identical over 20,000
 * random 2-7 card hands."
 *
 * Reads one JSON array of hands from stdin, writes one JSON array of results to
 * stdout. Each hand is an array of {suit, rank, value}; value is supplied by the
 * caller and never rewritten by the evaluator, which is how one evaluator serves
 * both ace-low and ace-high games.
 *
 *   echo '[[{"suit":"spades","rank":"A","value":14}, ...]]' | node js_oracle.mjs
 *
 * Usage is via tests/test_handeval.py, which skips when node is absent.
 */

import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

const here = dirname(fileURLToPath(import.meta.url));

// The vendored IIFE bundle the arcade actually serves, not the TypeScript
// source — so this checks against the build that ships, and needs no compiler.
const BUNDLE = resolve(
  here, '../../../website/arcade/shared/adenosine-cards.js',
);

function loadAdCards() {
  const source = readFileSync(BUNDLE, 'utf8');
  // The bundle assigns a browser global. A bare sandbox with `window` and
  // `globalThis` aliased to it catches either style of assignment.
  const sandbox = { console, module: {}, exports: {} };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  vm.createContext(sandbox);
  new vm.Script(source, { filename: BUNDLE }).runInContext(sandbox);

  const AdCards = sandbox.AdCards ?? sandbox.window?.AdCards;
  if (!AdCards?.HandEvaluator) {
    throw new Error(`AdCards.HandEvaluator not found in ${BUNDLE}`);
  }
  return AdCards;
}

function main() {
  const AdCards = loadAdCards();
  const evaluator = new AdCards.HandEvaluator();

  const input = readFileSync(0, 'utf8');
  const hands = JSON.parse(input);

  const results = hands.map((hand) => {
    const r = evaluator.evaluate(hand);
    return {
      name: r.name,
      rank: r.rank,
      points: r.points,
      tiebreakers: r.tiebreakers ?? [],
      description: r.description,
      partial: r.partial ?? false,
    };
  });

  process.stdout.write(JSON.stringify(results));
}

main();
