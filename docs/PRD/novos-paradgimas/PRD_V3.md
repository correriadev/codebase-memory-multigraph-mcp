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

## 9. PBB Breakdown: Product Backlog Building

Applying the PBB (Product Backlog Building) framework to PRD_V3: decomposing the third territory (Tradition) into Personas, Features, and Product Backlog Items (PBIs) with BDD acceptance criteria verifiable exclusively via host logs (Axiom of Testimony).

### 9.1 Personas

1. **Persona 1: The Operator / Tech Lead (The Sovereign Subject)**
   - *Nature:* Holds the intentional and normative mandate; sovereign over purpose, standards adoption, and acceptable risk; holds zero epistemic authority to fabricate evidence.
   - *Needs:* Answer to *"Why was it done this way?"* (citable canon or explicit confession); the ability to bind normative (`DEVE`) or advisory (`PODE`) traditions to a project; explicit recording of costly deviations (`DEVIATES_FROM`); prompt notification of tradition drift; sole authority to accept/decline theme founding proposals.
2. **Persona 2: The Autonomous Craft Agent (The Craft Worker)**
   - *Nature:* Operates across generation, refactoring, backlog formulation, and testing. Universal mechanics (Track B) driven by externalized canon (Track D).
   - *Needs:* Strict activity classification (`CONSULTATIVE` vs `SPECIALTY`); clear two-fork provenance mandate (cite canon or confess invention); structural barrier preventing accidental mutation of shared tradition graphs; automated refusal if attempting silent invention.
3. **Persona 3: The Institutional Curator (The Guardian of Tradition)**
   - *Nature:* Manages the collective corpus; maintains the catalog of themes, versions, and namespaces; monitors institutional taste across incarnations.
   - *Needs:* A dedicated KnowledgeBase registry where theme absence is a queryable state (`ABSENT`), not an error; cross-graph drift detection triggering re-verification notices when canon evolves; queryable visibility into orphan bindings and unreconciled project deviations; receipt of harvestable founding proposals from completed project sessions.

### 9.2 Features (PBB Canvas)

- **Feature F1: KnowledgeBase Registry & Absence Semantics (`FR-21`, `FR-28`)**
  - Manages the catalog of thematic graphs. Exposes absence as a queryable first-class epistemic state. Maintains fractal scale-invariance where each material theme graph is an independent union tenant.
- **Feature F2: Binding Claims & Deviation Ledger (`FR-22`, `FR-25`)**
  - Enables version-pinned binding claims on the Idealization plane (`DEVE` / `PODE`). Prevents agent self-validation. Governs strictness through cost by recording explicit, scarred deviation claims (`DEVIATES_FROM`).
- **Feature F3: Epistemic Activity Routing & Provenance Enforcement (`FR-23`, `FR-24`)**
  - Segregates consultative queries from specialty craft production. Enforces the provenance dichotomy: every craft judgment must carry a canon citation or a declared invention flag, rejecting undeclared invention via `PROVENANCE_UNDECLARED`.
- **Feature F4: Cross-Territory Typed Edges & Non-Write Barrier (`FR-25`)**
  - Implements unidirectional, typed cross-territory edges (`CONFORMS_TO`, `DEVIATES_FROM`, `CONSULTS`). Enforces a strict non-write barrier protecting theme graphs from project mutations. Elevates collisions to open contestações (`CONTESTATION_OPEN`).
- **Feature F5: Theme Drift Propagation & Orphan Diagnostics (`FR-26`)**
  - Emits re-verification notices to bound projects when a theme version advances. Equips the ECG with collective queries for orphan bindings and unreviewed deviations.
- **Feature F6: Session Closure Sweep & Founding Proposals (`FR-27`)**
  - Extends session closure sweep (C03) to harvest reusable craft patterns developed under declared invention, formulating founding proposals offered strictly to the operator.

### 9.3 Product Backlog Items (PBIs) & BDD Acceptance Criteria

#### PBI-01: Theme Catalog Registration and Absence Querying (Feature F1 → Spec D01)
- **User Story:** *As an Institutional Curator, I want to register and query theme entries in a dedicated KnowledgeBase registry graph, so that projects can discover institutional craft standards and query missing themes without runtime failures.*
- **Scenario 1 (Valid Registration):**
  - **Given** a valid theme registration payload containing `theme_id`, `namespace`, `curator`, `target_uri`, and `version`,
  - **When** submitted to the KnowledgeBase registry,
  - **Then** a theme node is admitted with `status=ACTIVE` and creation timestamp.
- **Scenario 2 (Querying Unmaterialized Theme):**
  - **Given** a registered theme entry whose physical graph has not yet been instantiated,
  - **When** queried by an agent during grounding,
  - **Then** the registry returns the entry with `status=ABSENT` and zero host exceptions are thrown.
- **Scenario 3 (Malformed Theme Schema):**
  - **Given** a theme submission missing required catalog metadata,
  - **When** evaluated,
  - **Then** refusal `THEME_SCHEMA_INVALID` is emitted naming the missing attributes.

#### PBI-02: Operator-Validated Theme Binding & Deviation Scars (Feature F2 → Spec D02)
- **User Story:** *As an Operator, I want to bind my project to an exact version of a craft theme and record explicit deviations with reasons, so that compliance is strictly governed by cost rather than silent divergence.*
- **Scenario 1 (Operator Normative Binding):**
  - **Given** an operator command binding `@inst/clean-arch` at version `2.1.0` as `NORMATIVE` (`DEVE`),
  - **When** processed by the admission pipeline,
  - **Then** a `type=BINDING` claim is recorded on the Idealization plane with `validated_by=operator` and pinned semver.
- **Scenario 2 (Agent Self-Binding Prohibited):**
  - **Given** an agent attempting to create a binding claim with `validated_by=agent`,
  - **When** validated,
  - **Then** the admission gate emits refusal `BINDING_SELF_VALIDATED`, blocking the claim.
- **Scenario 3 (Recording Costly Deviation):**
  - **Given** an intentional architectural departure from a bound theme rule,
  - **When** authorized by the operator,
  - **Then** a `type=DEVIATION` claim is admitted with `theme_rule_node_ref`, `reason`, and `affected_scope`, preserving the scar visibly.

#### PBI-03: Epistemic Activity Routing & Anti-Silent Invention (Feature F3 → Spec D03)
- **User Story:** *As an Operator, I want specialty judgments to be structurally checked for provenance, so that the agent can never present pretraining statistical bias as unvetted authority.*
- **Scenario 1 (Consultative Routing):**
  - **Given** a user query asking "What does service OrderProcessor do?",
  - **When** evaluated by the classifier,
  - **Then** it is classified as `CONSULTATIVE`, grounding is restricted to the project planes, and no theme citations are demanded.
- **Scenario 2 (Specialty Activity with Canon Citation):**
  - **Given** a task generating a new domain module under a bound theme,
  - **When** the agent emits code citing `{theme_id: "@inst/clean-arch", node_uri: "rules/aggregate-root"}`,
  - **Then** the judgment is admitted with epistemic status `CITED_CANON`.
- **Scenario 3 (Specialty Activity with Declared Invention):**
  - **Given** a task in an area without governing themes,
  - **When** the agent provides `declared_invention: true` with rationale,
  - **Then** the judgment is admitted with epistemic status `DECLARED_INVENTION`.
- **Scenario 4 (Undeclared Invention Refused):**
  - **Given** a specialty judgment produced without canon citation and without declared invention,
  - **When** evaluated,
  - **Then** refusal `PROVENANCE_UNDECLARED` is emitted and logged.

#### PBI-04: Cross-Territory Edge Typing & Non-Write Barrier (Feature F4 → Spec D04)
- **User Story:** *As an Institutional Curator, I want theme graphs to be protected by an immutable non-write barrier, so that project incarnations never mutate shared institutional memory.*
- **Scenario 1 (Strict Edge Typing):**
  - **Given** an edge created from a project entity to a theme node,
  - **When** validated,
  - **Then** the edge type must strictly belong to `{CONFORMS_TO, DEVIATES_FROM, CONSULTS}`.
- **Scenario 2 (Write Barrier Enforcement):**
  - **Given** any mutation command targeting a theme graph originating from a project session context,
  - **When** intercepted by the engine gateway,
  - **Then** mutation is blocked and refusal `TERRITORY_WRITE_FORBIDDEN` is logged.
- **Scenario 3 (Conflict Contestation):**
  - **Given** a direct contradiction between a project claim and a bound theme standard,
  - **When** identified,
  - **Then** the agent is prohibited from auto-resolving; event `CONTESTATION_OPEN` (A10) is emitted to the operator.

#### PBI-05: Theme Drift Propagation & Orphan Diagnostics (Feature F5 → Spec D05)
- **User Story:** *As an Operator, I want to be alerted when a bound theme advances and query orphan bindings, so that architectural drift and deprecated standards are immediately visible.*
- **Scenario 1 (Drift Notice on Theme Bump):**
  - **Given** a theme advancing from version `1.0.0` to `2.0.0` in the registry,
  - **When** published,
  - **Then** all incarnations carrying a normative binding receive a `THEME_DRIFT_NOTICE`, marking the binding `DRIFT_PENDING`.
- **Scenario 2 (Orphan Binding ECG Query):**
  - **Given** a project bound to a theme subsequently marked `ABSENT` or `DEPRECATED`,
  - **When** ECG query `query_orphan_bindings` is run,
  - **Then** the binding is returned as an orphan binding with its risk status and target provenance.

#### PBI-06: Closure Sweep Founding Proposals (Feature F6 → Spec D06)
- **User Story:** *As an Operator, I want high-value patterns crafted during sessions to be offered as theme founding proposals at closure, so that team craft can crystallize into institutional memory without uncontrolled auto-founding.*
- **Scenario 1 (Harvesting Founding Seed):**
  - **Given** a session containing validated `declared_invention` patterns,
  - **When** the closure sweep runs,
  - **Then** a `FOUNDING_PROPOSAL` is presented to the operator containing suggested namespace, rationale, and seed rules.
- **Scenario 2 (Operator Accepts Proposal):**
  - **Given** a `FOUNDING_PROPOSAL`,
  - **When** accepted by the operator,
  - **Then** a new theme entry is admitted to the KnowledgeBase registry with `status=DRAFT` and origin session provenance.
- **Scenario 3 (Operator Declines Proposal):**
  - **Given** a `FOUNDING_PROPOSAL`,
  - **When** declined by the operator,
  - **Then** the seed is discarded with a typed exclusion counter (`FOUNDING_PROPOSAL_DECLINED`) in the closure record.
- **Scenario 4 (Autonomous Founding Prohibited):**
  - **Given** an agent attempting to create a theme graph without operator validation,
  - **When** intercepted,
  - **Then** refusal `AUTO_FOUNDING_FORBIDDEN` is emitted.

---

## 10. Extracted Technical Specifications: Track D (`knowledge-base/`)

Extracted and formalized into atomic specification files under `specs/knowledge-base/`:

| Spec ID | Name | Domain | Deps | Key Artifacts & Refusals |
| --- | --- | --- | --- | --- |
| [D01](specs/knowledge-base/SCOPE-D01-registry-graph.md) | KnowledgeBase Registry Graph & Absence Querying | Catalog tenancy | A01, A04 | Node model, `status=ABSENT`, `THEME_SCHEMA_INVALID`, `THEME_UNKNOWN` |
| [D02](specs/knowledge-base/SCOPE-D02-binding-claims.md) | Binding Claims (DEVE/PODE) & Deviation Ledger | Intent governance | C01, C03, D01 | `type=BINDING`, `type=DEVIATION`, `BINDING_SELF_VALIDATED`, scar queries |
| [D03](specs/knowledge-base/SCOPE-D03-routing-provenance-transparency.md) | Epistemic Routing & Provenance Transparency | Provenance enforcement | A03, A04, D02 | `ActivityClass`, two-fork provenance, `PROVENANCE_UNDECLARED` refusal |
| [D04](specs/knowledge-base/SCOPE-D04-cross-territory-typed-references.md) | Cross-Territory Typed Edges & Non-Write Barrier | Cross-graph boundary | C05, D01, D02 | `CONFORMS_TO`, `DEVIATES_FROM`, `CONSULTS`, `TERRITORY_WRITE_FORBIDDEN` |
| [D05](specs/knowledge-base/SCOPE-D05-theme-drift-propagation.md) | Theme Drift Propagation & Orphan Binding ECG Queries | Drift & ECG health | C06, C07, D01, D02 | `THEME_DRIFT_NOTICE`, `query_orphan_bindings`, `query_unreconciled_deviations` |
| [D06](specs/knowledge-base/SCOPE-D06-founding-proposals.md) | Closure Sweep Founding Proposals | Harvest & founding | C03, D01, D03 | `FOUNDING_PROPOSAL` destination, `AUTO_FOUNDING_FORBIDDEN` refusal |

### 10.1 Refusal Taxonomy Extensions (Accretes A04)

Track D introduces six typed refusals to the closed taxonomy:
1. `PROVENANCE_UNDECLARED`: Specialty judgment carries neither canon citation nor declared invention flag.
2. `BINDING_SELF_VALIDATED`: Agent attempted to self-validate a normative or consulted theme binding.
3. `TERRITORY_WRITE_FORBIDDEN`: Project session attempted graph mutation inside an external theme graph.
4. `AUTO_FOUNDING_FORBIDDEN`: Agent attempted to instantiate a new theme graph without operator validation.
5. `THEME_SCHEMA_INVALID`: Theme catalog submission missing mandatory fields (`theme_id`, `namespace`, `curator`, `version`).
6. `THEME_UNKNOWN`: Queried namespace or theme identifier does not exist in registry.

---

## 11. Open questions

1. **Theme admission** — who admits a theme into the registry? Proposal: a founding is an operator-validated claim (C03 rule); institutional namespaces may add a collegial gate later, found by need.
2. **Namespacing** — personal / institutional / public scopes for themes; collision policy between same-named themes.
3. **Feedback promotion** — when one incarnation's innovation deserves to become institutional standard: the path project → theme is a *proposal* across territories, never a write. Mechanics TBD.
4. **The hidden theme** — should the agent's own training ever be indexable as a theme ("house style")? Tension: it would make the silent graph citable; risk: it would also launder invention into false canon. Open.
5. **Permanent name** — "Grafos Externos Indiretos" is the operator's provisional term. Candidates: *Thematic Knowledge Bases* (technical), *Tradition* (the third territory, pairing with Realization and Idealization), *Canon Graphs* (the normative subset). Proposal: the class is named **thematic graphs**, the territory **Tradition**, the registry **the KnowledgeBase**.

---

*Provenance: PRD (concrete) → PRD_V1 (abstraction) → PRD_V2 (synthesis) → ADR_V1 (ideation, emended twice: §3.6 Two Persistent Planes; §3.7 Claim Substrate) → **PRD_V3 (extension: the third territory)** → **PBB Extraction (Track D Scopes D01–D06)**. Each derivation crossed one boundary. V3 was proposed by the operator as a problem statement — the recognition that the two planes answer WHAT and WHY but never HOW, and that the HOW was silently governed by the agent's training — and rendered into the lineage's discipline by the agent. Per its own subject, this document's specialty judgments carry declared provenance: the Jungian frame of §6 is declared invention (no bound theme); the substrate claims of §2 cite the reference incarnation's existing machinery [B]. Formalized via PBB analysis into engineering specifications D01–D06, awaiting final operator review.*
