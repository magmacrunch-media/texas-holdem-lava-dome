// main.js - Texas Hold'Em Lava Dome | MagmaCrunch Media © 2024
const { ChipAnim, DENOMS } = AdCards;

document.addEventListener('DOMContentLoaded', () => {
    console.log("Texas Hold'Em Lava Dome — MagmaCrunch Media © 2024");

    // Initialize chip animation
    ChipAnim.init('chipDisplay', {});

    const state     = new GameState();
    const dealer    = new Dealer(state);
    const evaluator = new AdCards.HandEvaluator();
    const dome      = new Dome(state, evaluator);
    const betting   = new Betting(state);
    const scoring   = new Scoring(state);
    const ui        = new UI(state, dealer, evaluator, dome, betting, scoring);

    // Load scores on startup so the high scores modal is ready
    scoring.loadHighScores().then(() => scoring.displayHighScores());

    ui.init();

    // Initialize chip display with starting chips
    ChipAnim.setChips(500);

    console.log('Game ready.');
});
