# SHOWCASE-REPORT — building docs/index.html

Date: 2026-10-27 · branch: `showcase` · one deliverable file: `docs/index.html`
(52.5 KB, self-contained; this report is the second file added).

## 0. Starting point

The environment provided a fresh checkout of `SuperInstance/stitch` at
`/home/proart/stitch-showcase`, already on branch `showcase` at the v0.4.0
head (`3569b75`), clean tree except the untracked task brief (`BRIEF.md`,
left untracked on purpose — it is the task input, not a deliverable). That
satisfies "clone fresh, branch showcase" for this run; nothing else was
pre-existing (`docs/` did not exist).

Hard rule honoured: **every number, hash, quote, and result on the page comes
from the brief.** No facts were invented, and nothing was borrowed from
repo docs beyond phrasing that the brief itself uses. Presentational
mappings that extend a given fact: card faces show letter ranks for 11+
(the brief gives "Card 14 = Ace"; J/Q/K are the standard companions and the
numeric value is printed small beside the letter), and the player colours
(thor violet, orin cyan, proart amber) are decoration, not data.

## 1. Design decisions

- **Zero external anything**, per the repo's dependency-free ethos: one HTML
  file, all CSS/JS/SVG inline; system font stacks only (no webfonts); the
  favicon is a data-URI SVG (base64, so not even an XML-namespace string
  trips a naive external-reference grep).
- Dark theme (`#0a0d13` ground, `#10151f` card surfaces, dashed-stitch
  section dividers as the recurring motif), generous section spacing
  (92px), card motifs throughout. Mint green = minting/positive; red =
  refusal; per-player colours link route nodes ↔ cards ↔ hands.
- Sections: hero (the one-line facts as chips + a two-ledgers/two-chains
  SVG whose dashed box is the minted fact) → **STITCH WAR** (route diagram
  + interactive replay) → **How a mint works** (four-beat inline SVG flow
  with the MINTED/REFUSED fork and the five refusal chips) → **v0.4
  scoreboard** (three stat cards + gates strip) → **timeline** → footer
  with the required line and `<!-- NEXT SECTION -->` left at the end of
  `<main>`.
- The replay: `Step` walks deal → turn 0…turn 5 → witness derive
  (7 states, 7 relay legs on the route diagram). Each step lights the
  arriving leg (animated dashes) and the acting machine's node, deals the
  card into its trick slot (rank big, player, machine), and shows the
  booking caption (`xchg` id + payload hash, verbatim from the record).
  Winner banners after turns 2 and 5; final score + "Derived from the
  ledgers alone — the record is the game." at the witness step; `Reset`
  restores the deal. Hands strip dims cards as they are played.

## 2. Validation actually run (with the misses)

1. **JS syntax** — script extracted and `node --check`: PASS.
2. **External-reference audit** — regex over all `src=`, `href=`, `url()`:
   the only `http(s)` hits are six github.com **anchor links** (topbar
   "repo ↗" + footer docs/repo links — links, not loaded resources; no
   `src=`, no `<img>`, no external `url()`, no stylesheets/scripts fetched).
3. **Rendered screenshots** — headless Chromium (the Playwright build in
   `~/.cache/ms-playwright`; no system Chrome installed) at 1440×6600 and
   414×7600, including a **`file://` load** to prove the page opens
   standalone. Sections inspected segment-by-segment with a vision-model
   QA pass.
4. **Interaction, real clicks in real Chromium** — temp preview copies with
   an auto-click harness (deleted afterwards), then `--dump-dom` and 24
   parser assertions on the post-JS DOM: all six cards shown, trick-1
   winner = proart's Ace (ring) with losers dimmed, trick-2 winner = orin's
   8, both banners, final panel + witness caption, leg0…leg6 lit with leg6
   current, thor node active with "witness derive · here", all six hand
   cards marked played, Step disabled at the end — and the full Reset path
   back to "the deal". **24/24 PASS.**
5. **Vision QA findings → fixed → re-verified:** (a) the `agree()` box's
   second line overflowed its border — shortened to "whenever the files
   meet"; (b) the fifth refusal chip (`bad_signature`) read as an orphan —
   now spans the grid as a footer chip; (c) fork arrowheads butted against
   the MINTED/REFUSED borders — pulled back ~4px. Polish: `witness derive`
   label nudged onto its curve, empty-slot placeholders brightened one
   step, refusal-chip font 10px → 9.5px.

**Misses, stated plainly:** the browser-use plugin bridge refused to launch
("No installed Chrome or Chromium executable…"); symlinking the Playwright
Chromium into PATH and `/usr/local/bin` did not help, so the plugin path
was abandoned and Chromium was driven directly (screenshots + dump-dom)
instead. The AGENTS.md full test matrix was **not** run — this change
touches no code, only adds `docs/`; the library, tests, and tutorials are
untouched (verified: `git status` shows only the two new files).

## 3. Delivery

- Commit on `showcase` with this process in the commit body
  (files: `docs/index.html`, `SHOWCASE-REPORT.md`).
- `git push origin showcase` attempted; on failure,
  `git bundle create ~/stitch-showcase.bundle showcase`.
- `~/stitch-showcase.DONE` written with the final commit hash and outcome.
