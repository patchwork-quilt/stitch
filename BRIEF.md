# Build the stitch showcase page (docs/index.html)

You are the showcase builder for **stitch** (github.com/SuperInstance/stitch).
Clone it fresh, branch `showcase`, and create ONE file: `docs/index.html` —
a self-contained showcase page (all CSS/JS/SVG inline, ZERO external
resources, no webfonts, no CDNs — the repo's ethos is dependency-free).
It must be good-looking: dark theme, generous spacing, card motifs.
Write the process into a `SHOWCASE-REPORT.md` at repo root as you go.

## Hard rule: only the facts below. Invent NO numbers, NO quotes, NO results.

### The project in one line
stitch: a tiny substrate primitive for accountable agent state — 11
append-only stitches on hash-chained JSONL ledgers; shared state is
minted only where sovereign ledgers agree; γ+η ≤ log₂3 per tenant per
tick. Current version v0.4.0.

### Centrepiece: STITCH WAR — a real game, really played (2026-10-27)
Three AI agent seats on three physical machines (Thor, Orin, ProArt)
played a two-trick card game over stitch itself. Each play was a
booking in the player's own ledger on its own machine; the game
bundle travelled by plain file copy (scp); each agent, after playing,
relayed the bundle and woke the next machine's agent itself — no
central driver after the deal. A witness later derived the whole game
from the four ledger files alone. A doctored move file in rehearsal
refused as payload_mismatch and scored nothing.

Hands: thor [3, 12], orin [4, 8], proart [2, 14]. Plays, in order:
  turn 0: thor,   trick 1, card 12, xchg t1-thor,   payload hash 55282c950de0…
  turn 1: orin,   trick 1, card 4,  xchg t1-orin,   payload hash 6ecf7078425a…
  turn 2: proart, trick 1, card 14, xchg t1-proart, payload hash 05475e6665a7…
  turn 3: thor,   trick 2, card 3,  xchg t2-thor,   payload hash 921c81dab43c…
  turn 4: orin,   trick 2, card 8,  xchg t2-orin,   payload hash 84cdae0e40d3…
  turn 5: proart, trick 2, card 2,  xchg t2-proart, payload hash f70d9f18a97a…
Trick 1 winner: proart (14). Trick 2 winner: orin (8).
Final score: orin 1, proart 1, thor 0 — a draw between orin and proart.
Relay route: dealer (Muse's VM) → thor → orin → proart → thor → orin
→ proart → back to thor, where the witness derive ran.

Build an interactive REPLAY: a Step button (and Reset) that walks the
six plays in order. Each step: the current box lights up on a route
diagram (three machine nodes + dealer), the played card appears on
the table in its trick row, a caption shows the booking (xchg + hash).
After turn 2 and turn 5, the trick winner banner shows. At the end,
the final score + the line: "Derived from the ledgers alone — the
record is the game." Cards should look like cards (rank big, player
name, machine under it). Card 14 = Ace.

### Section: how a mint works
Inline SVG flow, four beats: (1) two sovereign ledgers, each on its
own machine; (2) both book the same exchange — SEND in one, RECEIVE
in the other, payload hash committed at booking time; (3) whenever
the files meet, agree() compares the bookings; (4a) MINTED — the
shared fact exists, derived, stored nowhere new; (4b) REFUSED — with
the refusal reasons as chips: no_send · no_receive · payload_mismatch
· unsigned_booking · bad_signature. Caption: "Shared state is never
stored. It is derived, on demand, where records agree."

### Section: v0.4 — the R&D release (scoreboard cards)
1. Byte-exactness fuzz — 1,884 payloads probed across Python/Rust:
   floats were the untested half of the contract (Python books 1e-05,
   a native Rust writer books 0.00001; 139/776 float texts drift ±1
   ulp through a default JSON parser). Shipped: a SPEC numeric policy
   + a precision-safe Rust audit. Cargo tests 6 → 10.
2. Fast Ed25519 — sign ~360 ms → ~1.5 ms, verify ~290 ms → ~4 ms,
   signatures byte-identical, stdlib only. The per-attestation audit
   cliff is gone.
3. Signed bookings — an impostor ledger claiming a tenant's identity
   minted in v0.3 when unsigned; under v0.4 key enforcement it refuses
   (unsigned_booking / bad_signature). Keyless behaviour byte-identical
   to v0.3. One honest residual, stated on the card: a verbatim replay
   of a genuine signed booking still mints.
Gates: 174 Python tests · 10 Rust tests · cross-language check PASSED ·
v0.3 had earlier been verified on five surfaces (VM, Oracle, Orin,
Thor, ProArt).

### Section: timeline
- v0.1 — the 11 stitches and the conservation law.
- v0.2 — the agreement layer: bookings, minting, the head-hash anchor.
- v0.3 — budgets become declarations; N-party agreement; Ed25519
  attestations (opt-in). 54 → 150 tests. Adopted with a position-aware
  budget patch after independent assessment caught a draft flaw.
- v0.4 — the R&D release: signed bookings, ~300x faster crypto, the
  numeric policy. 174 tests. Plus: three agents played cards over it.

### Footer
Links (relative or github URLs): SPEC.md, VERIFICATION.md, AGENTS.md,
tutorials/, the repo. Line: "Built in the open by Casey and his fleet —
the process is in VERIFICATION.md; this page shows only what the
record proves." Leave a clear HTML comment `<!-- NEXT SECTION -->`
at the end of main as the growth point.

## Delivery
- Commit on branch `showcase` with the process in the commit body.
- Try `git push origin showcase`; if it fails, `git bundle create ~/stitch-showcase.bundle showcase`.
- Write `~/stitch-showcase.DONE` when finished. Validate the HTML
  opens standalone (no external refs: grep for http:// or https:// in
  src=/href= of assets — link hrefs to github.com in the footer text
  are fine, loaded resources are not).
