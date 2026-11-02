#!/bin/sh
# check_axioms.sh — the axiom gate for the Lean formalisation.
#
# Prints the axioms the three headline theorems depend on and fails
# if sorryAx appears among them. The theorems must rest on Lean's
# standard axioms alone (propext, Classical.choice, Quot.sound):
# a sorryAx in the list would mean a `sorry` is hiding somewhere in
# the dependency chain and the "proof" is a claim, not a proof.
#
# Usage: sh check_axioms.sh   (from anywhere; needs the library built
# — run `lake build` first — and elan/lake on PATH)
set -eu
LEAN_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CHECK="$LEAN_DIR/AxiomCheck.lean"
trap 'rm -f "$CHECK"' EXIT
cat > "$CHECK" <<'EOF'
import StitchLean

#print axioms Stitch.minted_sound
#print axioms Stitch.conservation_of_written
#print axioms Stitch.lawful_of_written
EOF
cd "$LEAN_DIR"
out=$(lake env lean AxiomCheck.lean)
printf '%s\n' "$out"
if printf '%s' "$out" | grep -q 'sorryAx'; then
    echo "AXIOM GATE FAILED: sorryAx among the dependencies" >&2
    exit 1
fi
echo "axiom gate: no sorryAx"
