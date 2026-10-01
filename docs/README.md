# Project Documentation

Index of project technical documentation for **codebase-memory-mcp**. Use the links below to navigate available documents, architectural records, features, guides, and graph topology.

## Documentation Index

| Document | Description | Reading |
|---|---|---|
| [**.digest.md**](./.digest.md) | Fast-path machine-readable orientation digest (stack, test commands, rules). | **Mandatory** |
| [**.graph.json**](./.graph.json) | Macro relation graph index for agent topology navigation and 1-hop routing. | **Mandatory** |
| [**ARCHITECTURE.md**](./adr/ARCHITECTURE.md) | Architecture, folder organization, layers, and code patterns for the project. | **Mandatory** |
| [**TESTS.md**](./adr/TESTS.md) | Testing strategies, test suites, minimum coverage, and execution commands. | **Mandatory** |
| [**ADR-001-COGNITIVE-RESPIRATION.md**](./adr/ADR-001-COGNITIVE-RESPIRATION.md) | Architectural decision record on cognitive respiration, diástole, and mutation seam. | **Mandatory** |
| [**ADR-002-THREE-TERRITORIES.md**](./adr/ADR-002-THREE-TERRITORIES.md) | Architectural decision record on the three epistemic territories and craft provenance. | **Mandatory** |
| [**ADR-003-TWO-TIER-ANCHORS.md**](./adr/ADR-003-TWO-TIER-ANCHORS.md) | Architectural decision record on two-tier AST anchors and horizon admission gate. | **Mandatory** |
| [**code_discovery.md**](./feature/code_discovery.md) | Realization Plane symbol discovery, Cypher graph querying, and call-chain tracing tools. | Optional |
| [**index_governance.md**](./feature/index_governance.md) | Repository indexing, path coverage verification, project lifecycle, and git diff impact tools. | Optional |
| [**multi_graph_federation.md**](./feature/multi_graph_federation.md) | Ephemeral cognitive horizon overlays, speculative modeling, and AST two-tier anchor admission tools. | Optional |
| [**multi_graph_federation_e2e.md**](./feature/multi_graph_federation_e2e.md) | Black-box MCP stdio JSON-RPC E2E test suite covering isolation, two-tier anchors, and recovery. | Optional |
| [**mutation_gate.md**](./feature/mutation_gate.md) | Host lifecycle hooks, PreToolUse seam interception, intent scoping, and mutation journals. | Optional |
| [**union_workflow.md**](./feature/union_workflow.md) | Cognitive session horizons, grounded repository mutation gates, craft theme bindings, and contestation tools. | Optional |
| [**TOOLS_REFERENCE.md**](./guide/TOOLS_REFERENCE.md) | Exhaustive reference encyclopedia documenting all 35 MCP tools with schemas, examples, and responses. | Optional |
| [**CONSUMPTION.md**](./guide/CONSUMPTION.md) | Complete client onboarding, host configuration (Antigravity, Claude, Cursor, Codex), and agent guidelines. | Optional |
| [**WORKFLOWS.md**](./guide/WORKFLOWS.md) | Detailed walkthrough of four real-world scenarios: exploration, horizons, mutation gate, and tradition. | Optional |
| [**INFRASTRUCTURE.md**](./INFRASTRUCTURE.md) | C11 build toolchains, multi-platform runtime matrix, SQLite WAL storage, and daemon reaper. | Optional |
| [**PRD_V3.md**](./PRD/novos-paradgimas/PRD_V3.md) | Foundational PRD V3 establishing the Third Territory (Tradition / Canon) and Epistemic Provenance. | Optional |
| [**PRD_PTBR.md**](./PRD/novos-paradgimas/PRD_PTBR.md) | Portuguese foundational vision document on deep psychological archetypes (Jung in silicon, Temenos, Nigredo). | Optional |

## Recommended Reading Order

If an exact path is supplied, read it directly. Otherwise use this order:

1. **.digest.md** — fast AI orientation (architecture pattern, stack, test commands).
2. **.graph.json** — macro relation graph index for 1-hop document lookup.
3. **ARCHITECTURE.md** & **TESTS.md** — foundational architecture, runtime layers, and testing protocols.
4. **ADR-001**, **ADR-002**, **ADR-003** — core architectural decisions (Cognitive Respiration, Three Territories, Two-Tier Anchors).
5. **TOOLS_REFERENCE.md** — full technical reference for all 35 MCP tools.
6. Selected feature, guide, PRD, or spec documents when specific domain context is required.
