/**
 * Evaluates hands with the real AdCards.HandEvaluator, for differential testing.
 *
 * The Python port in lavadome/handeval.py has to agree with the JavaScript
 * exactly or the same game scores differently in two places — and a poker
 * evaluator that is subtly wrong plays almost right, which is the hardest kind
 * of bug to notice. Rather than hand-copying a few assertions, this exposes the
 * actual evaluator so both can be run over thousands of random hands.
 *
 * That is the same method web/js/config.js records using when AdCards replaced
 * this game's original evaluator: "Verified behaviourally identical over 20,000
 * random 2-7 card hands."
 *
 * Two sources, tried in order:
 *
 *   1. The published npm package, @magmacrunch/adenosine-cards. This is what CI
 *      uses — there is only one checkout there, so the sibling website repo is
 *      not on disk. Testing against the published artifact is also the stronger
 *      check: it is what the arcade actually ships.
 *   2. The vendored IIFE bundle in the website repo next door, for a dev box
 *      with no node_modules installed.
 *
 * Neither present is a hard error rather than a silent pass. `--check` exits 0
 * when a source is loadable and 1 when none is, which is what lets the Python
 * side skip honestly instead of reporting green having tested nothing.
 *
 * Reads one JSON array of hands from stdin, writes one JSON array of results to
 * stdout. Each hand is an array of {suit, rank, value}; value is supplied by the
 * caller and never rewritten by the evaluator, which is how one evaluator serves
 * both ace-low and ace-high games.
 */

import { existsSync, readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

const here = dirname(fileURLToPath(import.meta.url));

const NPM_PACKAGE = '@magmacrunch/adenosine-cards';
const VENDORED = resolve(here, '../../../website/arcade/shared/adenosine-cards.js');

/** The published package. Present in CI, and on a dev box that ran npm install. */
async function fromNpm() {
  try {
    const mod = await import(NPM_PACKAGE);
    if (!mod?.HandEvaluator) return null;
    return { HandEvaluator: mod.HandEvaluator, source: `npm:${NPM_PACKAGE}` };
  } catch {
    return null;
  }
}

/** The IIFE bundle the arcade serves, from the website repo next door. */
function fromVendoredBundle() {
  if (!existsSync(VENDORED)) return null;
  const source = readFileSync(VENDORED, 'utf8');
  // A bare sandbox with `window` and `globalThis` aliased to it catches either
  // style of global assignment the bundle might use.
  const sandbox = { console, module: {}, exports: {} };
  sandbox.window = sandbox;
  sandbox.globalThis = sandbox;
  vm.createContext(sandbox);
  new vm.Script(source, { filename: VENDORED }).runInContext(sandbox);

  const AdCards = sandbox.AdCards ?? sandbox.window?.AdCards;
  if (!AdCards?.HandEvaluator) return null;
  return { HandEvaluator: AdCards.HandEvaluator, source: `bundle:${VENDORED}` };
}

async function loadEvaluator() {
  return (await fromNpm()) ?? fromVendoredBundle();
}

async function main() {
  const checkOnly = process.argv.includes('--check');

  const loaded = await loadEvaluator();
  if (!loaded) {
    process.stderr.write(
      `no HandEvaluator available.\n` +
      `  tried npm package ${NPM_PACKAGE} (run: npm install --prefix ${here})\n` +
      `  tried bundle ${VENDORED}\n`,
    );
    process.exit(1);
  }

  if (checkOnly) {
    process.stdout.write(loaded.source);
    return;
  }

  const evaluator = new loaded.HandEvaluator();
  const hands = JSON.parse(readFileSync(0, 'utf8'));

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

await main();
