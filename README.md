# Texas Hold'Em Lava Dome

Solo Texas Hold'Em against an escalating dome: beat the threshold or forfeit
the bet, bank your chips or leave them in play. One repo, every version of
the game.

Named after the band Texas Hold'Em Lava Dome, on the magmacrunch media record
label, whose song titles supply the flavour text on the depth screens.

| Version | Folder | Engine | Where it runs |
|---------|--------|--------|---------------|
| Browser | [`web/`](web/) | [adenosine](https://github.com/magmacrunch-media/adenosine) | [magmacrunch.com/arcade/solitaire_THLD](https://magmacrunch.com/arcade/solitaire_THLD/) |
| Wii | [`wii/`](wii/) | [magnolia](https://github.com/magmacrunch-media/magnolia) | Homebrew Channel |
| Terminal | [`tui/`](tui/) | [magmacrunch.engine](https://pypi.org/project/magmacrunch/) | any terminal |

## Layout

- `web/` — the browser version. **This folder is the source of truth**; the
  copy served from the website repo at `arcade/solitaire_THLD/` (that name is
  historical — the URL keeps it) is generated from it via
  `make sync-texas-holdem-lava-dome` in that repo. Edit here, sync there.
- `wii/` — the Wii port. Builds with devkitPPC and expects the magnolia engine
  checked out beside this repo (`../../magnolia` from inside `wii/`). See
  [`wii/README.md`](wii/README.md).
- `tui/` — the terminal version. Runs on the
  [magmacrunch.engine](https://pypi.org/project/magmacrunch/) engine's terminal backend.
  Install with `pipx install magmacrunch-thld` and launch with `lava-dome`.
  Also available as part of the
  [magmacrunch](https://pypi.org/project/magmacrunch/) arcade.
  See [`tui/README.md`](tui/README.md).

## Working on the game

A rules or tuning change usually lands in all three versions:
`web/js/config.js` holds the tuned numbers and `web/js/dome.js` the round
flow, and both the Wii port's C modules and the TUI's Python scenes cite
those sources. Change `web/` first, then carry the change into `wii/source/`
and `tui/`.

This repo was formed from `texas-holdem-lava-dome-wii` (whose history it
keeps) plus the browser version imported from the website repo.

## License

[PolyForm Noncommercial License 1.0.0](LICENSE) — read it, learn from it, build
on it, play with it. Any noncommercial purpose is permitted; commercial use is
reserved. The game's name, art, audio and visual design are reserved outright
and are not covered by that licence: see [NOTICE](NOTICE) for the exact
boundary and the third-party components.
