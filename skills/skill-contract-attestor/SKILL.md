---
name: skill-contract-attestor
description: When creating or reviewing a CBM skill, check that its contract defines a universal procedure without embedding craft norms that belong in external themes.
---

# Attest a skill contract

Use when creating or reviewing a skill. It may guide grounding, deliberation, contestation, or tracing in any domain. Backend, frontend, QA, style, and architecture norms belong to Tradition, outside the skill.

## Contract boundary

Check identity, territory and overlap with other procedures, effect class, acknowledged refusals, trace and exclusion obligations when applicable, the skill's provenance, and its separation of project evidence, operator-validated intent, and external thematic norms.

The current host validator rejects an invalid identity, empty or excessive territory, `UNCLASSIFIED` effect class, and a zero acknowledged-refusals bitmask. Missing applicable obligations are findings from this review, not necessarily host refusals. There is no public API to list or admit contracts, so any overlap analysis is local and incomplete.

## Separate procedure from canon

If a skill prescribes how React, DDD, testing, or another craft must be practiced, identify the rule and propose its externalization into a versioned theme. A skill may instruct agents to consult and cite that theme; it cannot claim normative authority without provenance. Also check whether the contract grants the agent power to create a binding, resolve a project-versus-theme conflict, or write to a theme.

## Output

Return `VALID`, `INVALID`, or `DEFERRED` with field findings, overlap, norms to externalize, and required authority. Local validation does not admit a contract: `union_session_open` may still return `CONTRACT_UNKNOWN` and operate in restricted mode.
