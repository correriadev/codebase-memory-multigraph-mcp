---
doc_type: feature
domain: ast_anchor_isolation
stack: [C, Tree-sitter, Python]
node_id: "feature:ast-anchor-isolation"
tags: [ast-anchor-isolation, tree-sitter, two-tier-anchors, grammar-dispatch, relocation]
edges:
  - relation: implements
    target: "adr:architecture"
    read: must
  - relation: tested_by
    target: "adr:tests"
    read: must
  - relation: references
    target: "adr:two-tier-anchors"
    read: must
  - relation: child_of
    target: "feature:cross-horizon-admission"
    read: optional
    when: "Required when evaluating admission gate integration or concurrent anchor relocation"
updated: 2026-10-09
---
```graph
{"node_id":"feature:ast-anchor-isolation","domain":"ast_anchor_isolation","implements":["adr:architecture"],"tested_by":["adr:tests"],"entrypoints":["src/admission/anchor_checker.h"],"registration_files":[],"reference_files":["src/admission/anchor_checker.c"],"code_files":[],"test_files":["tests/test_ast_anchor_isolation.py","tests/test_anchor_checker.c"],"knowledge":{"schema_version":1,"entities":[{"id":"capability:ast-anchor-verification","type":"capability","label":"AST Anchor Verification","definition":"Two-tier anchor verification ensuring syntactic and structural declaration integrity before base graph admission.","aliases":["two-tier-verification"]},{"id":"rule:reject-zero-hash","type":"rule","label":"Reject Zero AST Hash","definition":"Anchors presenting ast_signature_hash equal to zero are invalid and must be rejected immediately.","aliases":["zero-hash-guard"]},{"id":"rule:full-file-ast-confinement","type":"rule","label":"Full File AST Confinement","definition":"Declaration verification must parse the complete file AST to reject symbols defined inside comments or string literals.","aliases":["trivia-rejection"]},{"id":"contract:anchor-checker-api","type":"contract","label":"Anchor Checker API","definition":"cbm_verify_two_tier_anchor verifies byte and AST identity, returning relocation offsets without mutating input anchors.","aliases":["cbm_verify_two_tier_anchor"]}],"claims":[{"id":"claim:fast-path-validation","subject":"capability:ast-anchor-verification","relation":"constrained_by","object":"rule:full-file-ast-confinement","statement":"cbm_verify_two_tier_anchor validates symbol declarations in the full file AST, rejecting literals and comments even if byte offsets match.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/anchor_checker.c","locator":"cbm_verify_two_tier_anchor: full file parse and declaration search","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:zero-hash-rejection","subject":"capability:ast-anchor-verification","relation":"constrained_by","object":"rule:reject-zero-hash","statement":"cbm_verify_two_tier_anchor rejects anchors with ast_signature_hash == 0 as malformed input.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/anchor_checker.c","locator":"cbm_verify_two_tier_anchor: hash == 0 guard","snapshot":null}],"derived_from":[],"gap":null},{"id":"claim:api-contract-boundary","subject":"capability:ast-anchor-verification","relation":"exposes","object":"contract:anchor-checker-api","statement":"cbm_verify_two_tier_anchor populates out_relocation when benign shifts occur, preserving const anchor immutability.","kind":"observation","status":"supported","evidence":[{"kind":"code","source":"src/admission/anchor_checker.h","locator":"cbm_verify_two_tier_anchor signature and TwoTierAnchorRelocation","snapshot":null}],"derived_from":[],"gap":null}]}}
```

# AST Anchor Isolation

Two-tier verification ensuring syntactic byte-identity and structural Tree-sitter AST signature matching for candidate horizon admissions.

## OVERVIEW
The AST Anchor Isolation subsystem (Feature F001) protects the Base Knowledge Graph against corrupt or drifting code modifications. It decouples fast byte-offset comparisons from full Concrete Syntax Tree parsing across C, TypeScript, and Python, automatically relocating benign shifts while rejecting structural drift, path traversal escapes, and comment/string trivia confusions.

## FOLDER STRUCTURE
<folder_structure>
```
[project_root]/
├── src/
│   └── admission/
│       ├── anchor_checker.c       # Two-tier anchor logic, Tree-sitter AST search, FNV-1a structural hash
│       └── anchor_checker.h       # TwoTierAnchor, TwoTierAnchorRelocation, and grammar dispatch API
└── tests/
    ├── test_anchor_checker.c      # Native C unit tests for fast-path, trivia, traversal, and relocations
    └── test_ast_anchor_isolation.py # Python integration scenarios for F001 contracts and language grammars
```
</folder_structure>

## KEY INVARIANTS

1. **Non-Zero AST Hash Enforced**: Anchors with `ast_signature_hash == 0` are malformed. Verification immediately rejects them with failure status.
2. **Full File AST Confinement**: Verification evaluates symbols within the full-file Tree-sitter parse tree. Identical text residing inside comment trivia or string literals is explicitly rejected.
3. **Definition Priority over Prototypes**: When scanning C syntax trees, full function definitions (`function_definition`) take precedence over forward declarations (`declaration` prototypes without bodies).
4. **Const Anchor Immutability**: Input `const TwoTierAnchor *` arrays are strictly read-only. Relocated byte coordinates (`new_byte_start`, `new_byte_len`) are returned via `TwoTierAnchorRelocation` out-parameters without mutating callers.
5. **Path Traversal Confinement**: Relative paths with `..` sequences and absolute paths escaping the canonical repository root boundary are rejected.
6. **Bounded Buffer Consumption**: Source files are restricted to a 10 MB maximum allocation ceiling prior to parsing.

## HOW TO VERIFY TWO-TIER ANCHORS

### Prerequisites
1. Canonical repository root path resolved.
2. Initialized `TwoTierAnchor` descriptors populated with expected symbol names, byte ranges, and structural hashes.

### Steps
1. Call `cbm_resolve_language_from_path` to resolve target Tree-sitter grammar (`CBM_LANG_C`, `CBM_LANG_TYPESCRIPT`, `CBM_LANG_PYTHON`).
2. Pass anchors to `cbm_verify_two_tier_anchor` with an optional `TwoTierAnchorRelocation` buffer.
3. Check returned status code and boolean validity flag.

<code_example>
# CORRECT: Safe two-tier anchor verification with relocation tracking
bool is_valid = false;
TwoTierAnchorRelocation relocation = {0};
int rc = cbm_verify_two_tier_anchor(repo_root, &anchor, &is_valid, &relocation);
if (rc == 0 && is_valid) {
    uint32_t active_start = relocation.was_relocated ? relocation.new_byte_start : anchor.byte_start;
}

# WRONG: Mutating const anchor array or bypassing AST validation
TwoTierAnchor *mutable_anchor = (TwoTierAnchor *)&anchor; // Casts away const
mutable_anchor->byte_start = new_offset;                 // Memory mutation violation
</code_example>

## PARAMETERS / CONFIGURATIONS

| Parameter / Field | Type | Description |
|---|---|---|
| `file_path` | `char[512]` | Relative file path within repository boundary |
| `symbol_name` | `char[256]` | Target function, method, class, or interface name |
| `byte_start` | `uint32_t` | Zero-indexed starting byte offset in source file |
| `byte_len` | `uint32_t` | Length in bytes of expected symbol source text |
| `ast_signature_hash` | `uint64_t` | Normalized FNV-1a structural hash of AST declaration node |
| `expected_text` | `char[1024]` | Fast-path byte verification template |

## REFERENCES

- [**ARCHITECTURE.md**](../adr/ARCHITECTURE.md): Layered architecture and admission gate boundary.
- [**TESTS.md**](../adr/TESTS.md): Testing protocol, test suites, and execution commands.
- [**ADR-003-TWO-TIER-ANCHORS.md**](../adr/ADR-003-TWO-TIER-ANCHORS.md): Two-Tier AST Anchors baseline design.
- [**cross_horizon_admission.md**](./cross_horizon_admission.md): Admission gate integration and concurrency arbitration.
