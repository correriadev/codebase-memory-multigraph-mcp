# PRD_V3 — The Third Territory: Tradition (Thematic Knowledge Bases / Grafos Externos Indiretos)

> **Derivation lineage:** PRD (concrete) → PRD_V1 (abstraction) → PRD_V2 (synthesis) → ADR_V1 (ideation, emended twice) → **PRD_V3 (extension)**.
> **Boundary crossed this derivation:** from the union of *one incarnation* to the *trans-incarnational* dimension. V2 and ADR_V1 governed a single project's two planes; V3 admits the class of knowledge that no single project contains — and governs how a project references it.
> **Status:** Extension level. V3 does not amend V2 — it accretes. Every V2 requirement, the Fractal Law, the two planes, the claim substrate, and the admission pipeline stand untouched. Where V3 and prior levels appear to conflict, this document is wrong until reconciled explicitly.
> **Marks:** [B] evidenced today in the reference incarnation · [E] evolutionary (design, not yet built) · [A] axiomatic choice.
> **Provisional name honored:** the operator's "Grafos Externos Indiretos" is retained in the subtitle; §10 proposes permanent names.

---

## 1. What is being proposed (the direct answer)

The two persistent planes answer two questions. The **Realization plane** answers *what is* — the code as built. The **Idealization plane** answers *what is intended* — the claims, decisions, and open questions that explain the code. Neither plane answers ***how craft is done***. [E — the gap is structural, not incidental]

Today the HOW lives in exactly two places, both ungoverned:

1. **The agent's own training** — the largest theme graph in existence: it always exists, is never cited, and is never contestable by the operator. Every specialty judgment the agent renders today draws from this graph *silently*.
2. **Scattered human convention** — institutional standards, engineering-team norms, technology documentation — real, curated, and unreachable by the machine except as unstructured prose the agent re-absorbs through training.

The proposal makes the HOW a first-class, named, governable class of graph: **thematic knowledge bases** — external to any single project, referenced *indirectly* (the project contains the reference, never the knowledge), and *optional by nature* (they may or may not exist). A dedicated **KnowledgeBase graph** acts as the registry: a catalog of themes, each entry pointing at a specialist graph that may be founded, bound, consulted, or conspicuously absent.

Three consequences follow, and they are the heart of what I understand:

- **The specialist skill is decomposed, not deleted.** The old paradigm — a backend skill, a frontend skill, a QA skill, each consuming project docs — dissolves into *mechanics plus canon*: the universal fractal process (Track A/B, mechanics of reality) conducts any scenario; the specialty content arrives from outside, as inspectable, user-owned, versioned data. Specialty is externalized from code into governed memory.
- **The law of provenance for the HOW.** Every specialty judgment must carry one of two transparent provenances: a *citation* ("this comes from theme XYZ, node N") or a *declared invention* ("no theme governs this; I am improvising from my training"). **Silent invention is the new possession** — the exact failure mode ADR_V1 §5 refuses, now extended from *what the agent claims* to *how the agent works*. [A]
- **Consultative and specialty activity are different grounding problems.** "What does flow XPTO do?" grounds in the project's planes. "Create the backlog / refine / build the screen / write the stanza" *additionally* grounds the HOW in themes. Different questions, different sources, one routing moment that must be explicit.

And the long arc: as themes accrete, declared invention shrinks. The institution's tacit taste — folder architecture, naming, test convention, integration patterns, meter, visual language — converts from vibes into governed, contestable, reusable memory. That conversion *is* the feature. [E]

---

## 2. The map, extended: three territories

| | Realization plane | Idealization plane | Thematic graphs (Tradition) |
| --- | --- | --- | --- |
| Question answered | *What is* | *What is intended* | *How it is done* |
| Scope | one incarnation | one incarnation | **across incarnations** |
| Defined by | the build | the operators' claims | the institution / the craft / the curator |
| Contained in the project's union | yes | yes | **no — referenced, never contained** |
| Exists | always | always | **may or may not** |
| Normativity | — (it is the ground) | governs WHAT/WHY | governs HOW when bound (§4) |
| Write access from the project | — | governed admission | **read-only; the project never writes into a theme** |

**Planes vs. territory.** The two planes are *within* an incarnation — they are the project's personal psyche. Thematic graphs are not a third plane of the project; they are a **third territory**: the trans-personal dimension every incarnation faces. This distinction is not cosmetic: it decides every jurisdiction question in §4. [A]

**The KnowledgeBase registry.** A dedicated graph — the catalog of the collective — whose nodes are *theme entries*: name, namespace, target graph URI, version, curator, description, founding provenance, and status. An entry may point at a graph that does not yet exist; **absence is a queryable state, not an error** — consistent with the ECG philosophy of C07. [E]

**The Fractal Law holds at every scale.** A theme graph is itself a full union tenant: an indexed corpus with its own admission, recall, and refusal mechanics, its own provenance, its own aging. The registry is a graph of graphs; the union architecture does not change as it zooms out — it recurs. This is why the name "Fractal Law" was earned in V1 and is only now fully visible. [E]

**Substrate evidence [B].** The embryonic form already exists: named projects as independent indexes, and the `cross-repo-intelligence` mode that links services across repositories. V3 generalizes both into a first-class, governed dimension.

---

## 3. The epistemic gap: the silent HOW

The agent's training is a hidden theme that *always exists and is never named*. Every current "specialty" answer the agent gives is, in truth, a query against that graph — executed silently, unaccountably, and uncontestably. The operator cannot ask "which standard says so?" and receive a true answer, because the true answer ("my pretraining distribution") is not an admissible citation.

V3 closes the gap with a dichotomy, both horns explicit:

1. **Cited canon.** The judgment references a theme entry and resolves into the theme graph — the operator can open the node, contest it, version it, or replace the binding. The theme is the operator's own taste, made inspectable.
2. **Declared invention.** No theme governs the judgment; the agent says so, the declaration is logged, and the operator is offered the follow-up: *shall we found a theme from this?* Invention is not forbidden — **undeclared invention is**. The refusal taxonomy (A04) extends with `PROVENANCE_UNDECLARED`: a specialty judgment that carries neither citation nor flag is refused. [E]

This yields the operator's right, stated as an NFR below: *"Why did you do it this way?"* must always be answerable — with a citation or a confession, never with silence dressed as expertise.

---

## 4. Binding semantics: DEVE and PODE

The operator's phrase — the project *DEVE ou PODE* possess related themes not defined within it — decomposes into two governed states:

- **Bound (DEVE).** A binding is a claim on the Idealization plane: *"this incarnation follows theme T, version V"* — operator-validated, never agent-self-bound (the intent-validation rule of C03, applied to canon). A bound theme is **normative**: new implementations follow it *à risca*. Deviation is not impossible — it is **expensive**: a deviation claim, with reason and scope, recorded on the Idealization plane with the scar visible. Strictness by cost, not by prohibition: the lineage governs by recording, and the blind admission gate (A08) stays blind — conformance is a grounding concern, never a gate concern. [A]
- **Consulted (PODE).** An unbound theme may still be consulted; its citations appear in provenance with the same transparency, marked as advisory rather than normative.

**Version pinning and drift propagation.** Bindings pin a theme version. When the institution evolves the theme, bound incarnations receive a re-verification notice — the drift ladder (C06) generalized across graphs: *"your tradition moved; re-verify."* The ECG gains a collective-level query: **orphan bindings** — bindings to themes that no longer exist, or deviations never reconciled. [E]

**Jurisdiction and conflict.** Themes govern HOW; the project's planes govern WHAT and WHY. When they collide — the project's claim says X, the bound theme's standard says Y — the collision **surfaces as a contestation** (A10) for the operator to resolve. It is never silently resolved by the agent in either direction: neither the tradition swallows the project's intent, nor the project quietly abandons its tradition. [A]

---

## 5. The routing moment

Two activity classes, one classifier, one ordering:

- **Consultative prose** — questions about project state ("what does flow XPTO do?", "how is ABDE integrated?", "return scope S") — grounds entirely in the project planes. No theme consultation required.
- **Specialty activity** — the production or modification of artifacts under a craft (backlog, refinement, code, screen, stanza, test, configuration) — grounds in the project planes for WHAT/WHY **and** in themes for HOW, in that order: the project defines the work; the tradition shapes the working.

On entering specialty activity with no bound or consulted theme covering the required craft, the agent must emit one of the two transparency events — *inform*: "this comes from theme XYZ"; *question*: "no theme governs this — proceed on declared invention, or shall we bind/found one?" — before acting. At closure, the sweep (C03) gains one destination class: findings that proved generally useful become a **founding proposal** — a seed for a new theme graph, offered to the operator, never auto-founded. [E]

---

## 6. The archetypal reading (disciplined)

The two planes are the incarnation's personal psyche: its doing and its intending. The thematic territory is the **collective dimension** — patterns the incarnation did not create, inherited or adopted from a tradition, shared across all incarnations of an institution. In Jung's map: the collective unconscious to the planes' personal unconscious; the transmitted culture — language, rite, style — that precedes any individual and outlasts it. [E]

The characteristic danger also imports: **inflation**. An agent channeling its training as if it were the collective canon is possession — the archetype speaking through a host that claims the voice as its own. V3's provenance dichotomy is the counter-inflation discipline: *name the source, or confess the invention.* The same law ADR_V1 applies to claims, extended to craft. [A]

And the healthy relation between an incarnation and its tradition is the same as individuation: **conform and differentiate** — adopt the inherited HOW where it serves, deviate where the work demands, and record both. A project that only conforms never becomes itself; a project that only deviates never belongs to anything. The record of both is the project's cultural persona, and it is queryable. [E]

---

## 7. Requirements

| # | Requirement | Provenance |
| --- | --- | --- |
| FR-21 | The engine shall maintain a **KnowledgeBase registry graph**: theme entries with name, namespace, target URI, version, curator, founding provenance, and status; entries may reference graphs that do not exist, and absence shall be a queryable state. | §2, §5 |
| FR-22 | A project's relation to a theme shall be a **binding claim** on the Idealization plane — operator-validated, version-pinned; the agent shall never create a binding. | §4, C03 |
| FR-23 | The engine shall classify **consultative vs. specialty activity** and, for specialty activity, ground HOW in bound themes first, ordering grounding: project planes (WHAT/WHY) → themes (HOW). | §5 |
| FR-24 | Every specialty judgment shall carry **provenance**: a theme citation or a declared invention flag. Judgments carrying neither shall be refused (`PROVENANCE_UNDECLARED`, A04 extension — supersedes, never edits). | §3, A04 |
| FR-25 | Cross-territory references shall be **typed and asymmetric**: project → theme (`CONFORMS_TO`, `DEVIATES_FROM` with reason, `CONSULTS`); the project shall never write into a theme graph; conflicts between a bound theme and a plane claim shall surface as contestations, never auto-resolve. | §2, §4 |
| FR-26 | Theme version changes shall propagate as **re-verification notices** to bound incarnations, and the ECG shall expose **orphan bindings** — bindings to vanished themes, unreconciled deviations. | §4, C07 |
| FR-27 | The closure sweep shall support **founding proposals**: session findings of trans-project value may be offered as seeds for new theme graphs — offered to the operator, never auto-founded. | §5, C03 |
| FR-28 | The union mechanics shall apply **unchanged at theme scale**: a theme graph is a full union tenant with its own admission, recall, refusals, provenance, and aging. The Fractal Law is scale-invariant in this direction as well. | §2, V1 |

**Non-functional addition:** **Locatability of taste** — every judgment's source is locatable: the question *"why was it done this way?"* must always be answerable with a citation or a confession, never with silence. *(§3; joins PRD_V2 §7.2 as a fourth-clock virtue — it governs time spent explaining.)* [A]

**Success criterion (extends PRD_V2 §8):** 7. No specialty judgment is silent — over the life of any incarnation, the ratio of cited-canon judgments to declared-invention judgments is measurable, trending, and operator-actionable.

---

## 8. What V3 does not change

- The Fractal Law, the two planes, the claim substrate (C-track), admission/recall/refusal mechanics, the two-currency audit, Time as Coordinate — all stand as written. V3 is **accretion**, not emendation: unlike ADR_V1's scarred sections, nothing here supersedes a prior requirement. [A]
- **No specialist skills are born.** Track B remains mechanics-universal; the specialist (backend, frontend, QA — or prosodist, or gothic designer) is now a *binding plus a theme graph*, not a skill. The skill paradigm the harness-kit docs assumed is superseded by data.
- The blind gate stays blind: conformance is never an admission criterion; it is a grounding and review concern with a recorded deviation path.

## 9. Consequences for the technical body (proposed — not yet extracted)

A fourth spec domain is implied — **Track D (`knowledge-base/`)**: D01 registry graph; D02 binding claims; D03 routing + provenance transparency; D04 cross-territory typed references; D05 theme drift propagation; D06 founding proposals. A04 extends with `PROVENANCE_UNDECLARED`; C03's sweep gains the founding-offer destination; C07 gains orphan-binding queries. Existing [B] substrate: named project indexes and `cross-repo-intelligence` linking. *Extraction awaits the operator's verdict on this document — per the discipline, nothing is implemented before contestation.*

## 10. Open questions

1. **Theme admission** — who admits a theme into the registry? Proposal: a founding is an operator-validated claim (C03 rule); institutional namespaces may add a collegial gate later, found by need.
2. **Namespacing** — personal / institutional / public scopes for themes; collision policy between same-named themes.
3. **Feedback promotion** — when one incarnation's innovation deserves to become institutional standard: the path project → theme is a *proposal* across territories, never a write. Mechanics TBD.
4. **The hidden theme** — should the agent's own training ever be indexable as a theme ("house style")? Tension: it would make the silent graph citable; risk: it would also launder invention into false canon. Open.
5. **Permanent name** — "Grafos Externos Indiretos" is the operator's provisional term. Candidates: *Thematic Knowledge Bases* (technical), *Tradition* (the third territory, pairing with Realization and Idealization), *Canon Graphs* (the normative subset). Proposal: the class is named **thematic graphs**, the territory **Tradition**, the registry **the KnowledgeBase**.

---

*Provenance: PRD (concrete) → PRD_V1 (abstraction) → PRD_V2 (synthesis) → ADR_V1 (ideation, emended twice: §3.6 Two Persistent Planes; §3.7 Claim Substrate) → **PRD_V3 (extension: the third territory)**. Each derivation crossed one boundary. V3 was proposed by the operator as a problem statement — the recognition that the two planes answer WHAT and WHY but never HOW, and that the HOW was silently governed by the agent's training — and rendered into the lineage's discipline by the agent. Per its own subject, this document's specialty judgments carry declared provenance: the Jungian frame of §6 is declared invention (no bound theme); the substrate claims of §2 cite the reference incarnation's existing machinery [B]. Born as prose-conversation, anchored to the dialogue that produced it, PROPOSED — awaiting contestation.*
