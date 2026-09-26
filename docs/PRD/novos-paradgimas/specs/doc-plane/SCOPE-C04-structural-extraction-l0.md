# SCOPE C04 — Structural Extraction L0 (Prose-as-File)

> **Track:** C — Idealization Plane substrate (`doc-plane/`) · **Domain:** deterministic indexing of documentary files
> **Status:** REVIEW · **Mark:** [B/E] (`horizon_spec_parser` and `spec_fts` exist [B]; section-node emission into the doc plane is [E]) · **Deps:** C02
> **Provenance:** ADR_V1 §3.7 (Emendation II, extraction ladder L0)

## Problem

Markdown documents (specs, ADRs, PRDs) must become addressable graph content without any LLM in the loop. The parser exists [B]; the emission of L0 section nodes with claim-plane anchors does not. L0 is syntax, not semantics — it admits directly, because a heading cannot be an interpretation.

## Scope

**In:**
- Markdown structure parse (extend `horizon_spec_parser`): headings → section nodes with `cbm://docs/<path>#<heading-slug>` URIs, FILE_BYTES anchors per section, frontmatter fields as node properties, code-block detection, link extraction.
- Direct admission of L0 nodes into the documentary plane (no horizon pipeline for pure syntax — the temenos governs *claims*, and a heading is not a claim).
- Link extraction: file paths and URIs found in document bodies are recorded as unresolved reference candidates (resolution itself is C05).
- FTS indexing of section content into the existing `spec_fts` machinery [B].

**Out (named exclusions):**
- Semantic claim extraction (L2 — agent-proposed, through C03's pipeline). Referential resolution against the code graph (C05 — L1). The drift ladder reactions (C06 — this scope emits the events, that scope classifies them).

## Acceptance criteria

1. **Given** a markdown file with N headings, **When** parsed, **Then** N section nodes exist with slug URIs, each with a FILE_BYTES anchor verified against the file, and FTS rows exist for the content.
2. **Given** a heading renamed, **When** re-parsed, **Then** the C06 drift event fires (lexical classification) — L0 emits, the ladder decides.
3. **Given** a section deleted, **When** re-parsed, **Then** the node's anchor fails verification and the `gone` path of the ladder applies — never silent removal.
4. **Given** a document with internal links, **When** parsed, **Then** each link is a recorded unresolved-reference candidate — resolution is C05's; recording is mandatory here.
5. **Given** a file-born section node, **When** queried, **Then** its `status` derives from its anchors (L0 nodes carry no independent epistemic status — they are structure, and structure is verified or gone).
6. **Given** a parse failure (malformed markdown), **When** parsed, **Then** refusal with reason, logged — never a partial parse presented as complete.

## Open questions

- Heading-slug stability: are slugs stable across renumbering (e.g., "§3.6" → "§3.7" shifts)? Proposal: slugs derive from text, not numbers — numeric prefixes are cosmetic, URIs anchor to text slugs; renames are lexical drift, not identity death.
- Should frontmatter (e.g., YAML status fields) act as *status assertions* subject to validation, or as raw properties? Proposal: raw properties — status is the jurisdiction's, never the file's self-report (Axiom of Testimony: a document cannot attest its own epistemic state).
