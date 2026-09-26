# SCOPE A01 — Session Horizon Protocol

> **Track:** A — Memory Engine (codebase-memory-mcp) · **Domain:** horizon lifecycle
> **Status:** REVIEW · **Mark:** [E] · **Deps:** none
> **Provenance:** PRD_V2 §4 (Stations 1, 9); ADR_V1 §3.1 (INITIATE/TRACE); PRD_V2 FR-14

## Problem

Every skill invocation must be a bounded temenos — a session horizon with identity, provenance of the belief it is based on, and a lifecycle the host can audit. Today horizons exist as a speculative overlay mechanism, but there is no protocol binding a *skill session* to a horizon: identity declaration, `based_on_seq` declaration, restricted mode for unregistered skills, and a typed closure event.

## Scope

**In:**
- Horizon opening with: skill identity ref, contract ref (A02), territory declaration, `based_on_seq`, declared scales.
- Restricted mode: horizons opened by identities without a registered contract proceed with **all actions treated as irreversible-class** (A05) — restricted, not refused.
- Typed closure event: identity, duration, ledger ref (A06), exclusion summary (A07), trace ref (A09). Content is destroyed; events survive.
- Queryable horizon state at any point in the lifecycle.

**Out (named exclusions):**
- Budget accounting mechanics (A06).
- Trace file contents (A09).
- Promotion semantics (A07).
- Any judgment of skill merit at open/close — the protocol transports identity, it never judges it.

## Acceptance criteria

1. **Given** a registered skill identity with valid contract, **When** opening a session horizon, **Then** the host log records identity, contract ref, territory, and `based_on_seq`, and a queryable horizon handle is returned.
2. **Given** an identity without a registered contract, **When** opening a session horizon, **Then** the horizon opens in restricted mode, recorded as such, with all subsequent actions gated at the irreversible class (verified by at least one blocked unclassified action).
3. **Given** an open session horizon, **When** the world advances (`seq` moves), **Then** the horizon's `based_on_seq` remains legible and the staleness detection of A04 applies — the horizon is never silently re-based.
4. **Given** horizon closure, **When** content is destroyed, **Then** the closure event (identity, duration, counts by type) is emitted and the content is not retrievable — verified by a post-closure query returning nothing.
5. **Given** any horizon, **When** queried mid-lifecycle, **Then** its state is reconstructible from the host log alone (replay test).

## Open questions

- Should horizons survive the process that opened them (daemon-owned), or die with it? Default proposal: die with it — content destroyed, events survive; a crash is a closure with reason `ABNORMAL`.
- One horizon per skill invocation, or per work order (parent scope)? Proposal: per invocation; nesting is a Track-B concern.
