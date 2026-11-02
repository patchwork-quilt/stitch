#!/usr/bin/env python3
"""
attest_release.py — sign a stitch release with stitch itself.

Given the built artifact files of a release, this script creates a stitch
ledger (default: release-attestation.jsonl) in which a tenant named
"stitch-release" binds one cell of kind "release" whose payload records
the release version and the SHA-256 of every artifact, and then attests
that cell. The script audits the finished ledger with stitch's own audit
path and prints the ledger's head hash — the anchor.

Why: stitch's whole claim is accountable state — a record whose hashes
let a stranger check that what they downloaded is what the releaser
committed to, without trusting the releaser's say-so. A release of stitch
should therefore be attested the stitch way.

How anyone verifies (no trust in this script required):

  1. Re-derive the artifact hashes yourself, e.g.
         sha256sum stitch_substrate-0.4.1.tar.gz
     and compare them against the payload of the BIND entry of kind
     "release" in release-attestation.jsonl — plain JSONL, readable with
     any tool.
  2. Audit the ledger with stitch's own auditor:
         python3 -c "import stitch; v = stitch.audit_ledger('release-attestation.jsonl'); print(v.ok, v.head_hash)"
     or run this script's --verify mode:
         python3 scripts/attest_release.py --verify release-attestation.jsonl ARTIFACT...
     which re-checks every artifact hash against the ledger payload,
     runs the same audit, and prints the anchor. The anchor printed at
     release time is also quoted in the GitHub release notes; if the
     head hash you compute differs, the ledger you hold is not the
     ledger that was released.

Usage:
    python3 scripts/attest_release.py [--version X.Y.Z] [--out LEDGER]
                                      ARTIFACT [ARTIFACT ...]
    python3 scripts/attest_release.py --verify LEDGER ARTIFACT [ARTIFACT ...]

Requires the stitch package (pip install ./stitch-python). From a bare
clone it falls back to adding ./stitch-python to sys.path. Stdlib + stitch
only, like the library itself.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path

try:
    import stitch
except ImportError:  # bare clone, package not installed
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "stitch-python"))
    import stitch

from stitch import Ledger, Tenant, attest, audit_ledger, bind

TENANT = "stitch-release"
RELEASE_KIND = "release"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def artifact_records(paths):
    records = []
    seen = set()
    for p in paths:
        p = Path(p)
        if not p.is_file():
            raise SystemExit(f"attest_release: not a file: {p}")
        if p.name in seen:
            raise SystemExit(f"attest_release: duplicate artifact name: {p.name}")
        seen.add(p.name)
        records.append({"name": p.name, "sha256": sha256_file(p)})
    return records


def create(args) -> int:
    out = Path(args.out)
    if out.exists():
        raise SystemExit(
            f"attest_release: {out} already exists — an attestation ledger "
            "is append-only history; pick a new --out or remove it first."
        )
    records = artifact_records(args.artifacts)
    version = args.version or stitch.__version__
    payload = {"version": version, "artifacts": records}

    tenant = Tenant(identity=TENANT)
    ledger = Ledger(path=str(out))
    try:
        cell = bind(tenant, ledger, kind=RELEASE_KIND, payload=payload)
        attest(
            tenant,
            ledger,
            cell,
            claim=f"stitch {version} release: {len(records)} artifact(s), "
            "SHA-256 hashes as bound in this cell",
        )
    finally:
        ledger.close()

    verdict = audit_ledger(str(out))
    if not verdict.ok:
        print(
            f"attest_release: AUDIT FAILED (chain_ok={verdict.chain_ok}, "
            f"law_ok={verdict.law_ok})",
            file=sys.stderr,
        )
        return 1
    print(f"attestation ledger: {out}")
    print(f"version: {version}")
    for r in records:
        print(f"  {r['sha256']}  {r['name']}")
    print(
        f"audit: OK (entries={verdict.entries}, chain ok, law ok, "
        f"tenant={verdict.tenant})"
    )
    print(f"head hash (anchor): {verdict.head_hash}")
    return 0


def verify(args) -> int:
    ledger_path = Path(args.ledger)
    if not ledger_path.is_file():
        raise SystemExit(f"attest_release: not a file: {ledger_path}")
    bound = None
    for line in ledger_path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        entry = json.loads(line)
        if entry.get("op") == "BIND" and entry.get("kind") == RELEASE_KIND:
            bound = entry.get("payload") or {}
            break
    if bound is None:
        print("attest_release: no release BIND entry in ledger", file=sys.stderr)
        return 1

    expected = {r["name"]: r["sha256"] for r in bound.get("artifacts", [])}
    actual = {r["name"]: r["sha256"] for r in artifact_records(args.artifacts)}
    ok = True
    for name in sorted(set(expected) | set(actual)):
        if expected.get(name) is None:
            print(f"  UNBOUND  {name} (not in the ledger payload)")
            ok = False
        elif actual.get(name) is None:
            print(f"  MISSING  {name} (bound in the ledger, not supplied)")
            ok = False
        elif expected[name] != actual[name]:
            print(f"  MISMATCH {name}: ledger {expected[name]} != file {actual[name]}")
            ok = False
        else:
            print(f"  match    {name}  {actual[name]}")

    verdict = audit_ledger(str(ledger_path))
    print(
        f"audit: {'OK' if verdict.ok else 'FAILED'} (entries={verdict.entries}, "
        f"chain_ok={verdict.chain_ok}, law_ok={verdict.law_ok})"
    )
    print(f"version: {bound.get('version')}")
    print(f"head hash (anchor): {verdict.head_hash}")
    return 0 if (ok and verdict.ok) else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Create (or verify) a stitch release attestation ledger."
    )
    ap.add_argument("--version", help="release version (default: stitch.__version__)")
    ap.add_argument(
        "--out",
        default="release-attestation.jsonl",
        help="ledger to write (create mode; default: release-attestation.jsonl)",
    )
    ap.add_argument(
        "--verify",
        metavar="LEDGER",
        help="verify mode: check ARTIFACTs against an existing attestation ledger",
    )
    ap.add_argument("artifacts", nargs="+", help="artifact files to hash / check")
    args = ap.parse_args(argv)
    if args.verify:
        args.ledger = args.verify
        return verify(args)
    return create(args)


if __name__ == "__main__":
    raise SystemExit(main())
