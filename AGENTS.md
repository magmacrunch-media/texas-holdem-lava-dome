# Texas Hold'Em Lava Dome — agent brief

One game, two versions, one repo:

- `web/` — browser version (adenosine engine, plain JS). Source of truth for
  rules and tuning (`js/config.js`, `js/dome.js`). Deployed by the website
  repo: run `make sync-texas-holdem-lava-dome` there to copy `web/` into
  `arcade/solitaire_THLD/` (historical folder name, kept for the URL). Never
  edit the website repo's copy directly — it gets overwritten.
- `wii/` — Wii port (magnolia engine, C99). Has its own `AGENTS.md` and a
  README that records the deliberate differences from the web build (ace-high
  restamping, raise-buys-a-card). Expects magnolia checked out beside this
  repo.

A rules change is not done until both versions have it (or the commit says
why one is skipped).
