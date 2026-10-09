---
doc_type: adr
domain: architecture
stack: [C11, Tree-sitter, SQLite, WAL, yyjson]
node_id: "adr:harness-perfeito-v2"
tags: [architecture, harness-v2, tradition-rules, admission-manifests, fast-path, sqlite-wal, seam-map, bifocal-bundle]
edges:
  - relation: references
    target: "adr:two-tier-anchors"
  - relation: references
    target: "adr:architecture"
updated: 2026-10-09
---
# ADR-004 — Arquitetura do Harness Perfeito v2 no CBM-MGH: Governança Normativa e Contexto Bifocal

## STATUS
**ACCEPTED** (Consolidado após consultoria e validação arquitetural independente)

---

## 1. CONTEXTO & PROBLEMA

O `codebase-memory-multigraph-mcp` (CBM-MGH) atua historicamente como indexador sintático e semântico de grafos de código em C11 com persistência em SQLite WAL. Na evolução para a **Tríade Fractal** e a arquitetura do **Harness Perfeito v2**, o CBM deixa de ser mero repositório de consultas e assume o papel de **substrato topológico e oráculo normativo de admissão de mudanças**.

### Desafios Identificados
1. **Sobrecarga de Contexto em LLMs**: O envio de arquivos de dependência completos esgota a janela de contexto, aumenta custos e dilui a atenção do modelo em detalhes irrelevantes de implementação.
2. **Ausência de Rastreamento Normativo**: Diretrizes arquiteturais (ex: *Transactional Outbox*, isolamento de domínios) residem em documentos estáticos desacoplados da validação de código, permitindo violações silenciosas.
3. **Falta de Ledger de Admissão Pré-Execução**: Decisões de conformidade do JEV (*Joint Epistemic Verifier*) não são registradas com garantias de atomicidade e rastreabilidade temporal.
4. **Fragilidade de Concorrência e Swap de Gerações**: No CBM, novos índices são preparados e publicados via substituição de arquivo de banco (`pipeline.c`). Tabelas de governança e histórico de auditoria não podem ser destruídas durante rebuilds de geração de código.
5. **Risco de Falsos Negativos em Análise Sintática**: Tree-sitter puro extrai árvores de parsing, mas não resolve fluxos dinâmicos complexos. Tratar ausência de informação como "efeito inexistente" (`false`) geraria falsas liberações de *Fast-Path*.

---

## 2. DECISÃO ARQUITETURAL

Adota-se a arquitetura **Harness Perfeito v2** estruturada em quatro pilares fundamentais:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                       HARNESS EVIDENCE BUNDLE V2                            │
├──────────────────────────────────────┬──────────────────────────────────────┤
│    Target Mutation Nodes (Tier 1)    │     Dependency Capsules (Tier 2)     │
│  - Código-fonte integral exato       │  - Assinatura AST Tree-sitter        │
│  - Sem compressão para mutação/testes│  - Docstring declarada canônica      │
│                                      │  - Effect Fingerprint (Tri-state)    │
├──────────────────────────────────────┴──────────────────────────────────────┤
│  Seam Map: Hipóteses tipadas de integração trans-fronteira (Kafka, Celery)  │
│  Coverage Digest: Status estrito de cobertura ("clean" | "partial" | "missed")│
│  Tradition Rules: Normas canônicas ativas e aplicáveis ao escopo            │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                 GOVERNANÇA & ADMISSÃO (SQLite WAL Durável)                  │
│  - tradition_rules: Oráculo normativo com promoção exclusivamente humana     │
│  - harness_admission_manifests: Ledger de auditoria com BEGIN IMMEDIATE     │
│  - snapshot_id: Hash de integridade de versão (Fail-Closed TOCTOU fence)   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.1. Modelo de Persistência e Durabilidade no SQLite WAL

As tabelas de governança são desacopladas da volatilidade de reconstrução do grafo topológico:

#### Tabela `tradition_rules` (Grafo de Tradição Normativo)
```sql
CREATE TABLE IF NOT EXISTS tradition_rules (
    rule_id TEXT PRIMARY KEY,
    domain TEXT NOT NULL,
    norm_text TEXT NOT NULL,
    enforcement_level TEXT NOT NULL CHECK(enforcement_level IN ('NORMATIVE', 'CONSULTED')),
    status TEXT NOT NULL CHECK(status IN ('APPROVED', 'REVIEW_PENDING', 'UNVERIFIED_CANDIDATE')),
    snapshot_hash TEXT NOT NULL,
    review_interval_seconds INTEGER NOT NULL DEFAULT 2592000, -- 30 dias
    review_due_at INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    reviewed_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_tradition_domain ON tradition_rules(domain);
CREATE INDEX IF NOT EXISTS idx_tradition_status ON tradition_rules(status);
```

* **Governança de Transição:**
  - Propostas de advisors externos ou agentes entram estritamente como `UNVERIFIED_CANDIDATE`.
  - Apenas o operador humano tem autoridade para promover regras para `APPROVED`.
  - Avaliação de Cadência: Se $\text{now} \ge \text{review\_due\_at}$, a regra é avaliada como `REVIEW_PENDING`. Não sofre expurgo silencioso e mantém sua força normativa até revisão explícita.

#### Tabela `harness_admission_manifests` (Ledger de Compliance do JEV)
```sql
CREATE TABLE IF NOT EXISTS harness_admission_manifests (
    manifest_id TEXT PRIMARY KEY,
    bundle_hash TEXT NOT NULL,
    change_hash TEXT,
    snapshot_id TEXT NOT NULL,
    jev_overall_score REAL NOT NULL,
    blocking_violations TEXT NOT NULL, -- JSON array
    warnings TEXT NOT NULL,            -- JSON array
    fast_path_granted INTEGER NOT NULL CHECK(fast_path_granted IN (0, 1)),
    admitted_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_manifest_snapshot ON harness_admission_manifests(snapshot_id);
```

#### Protocolo de Transação e Durabilidade
1. Conexão dedicada para governança configurada com `PRAGMA synchronous = FULL;` e `PRAGMA journal_mode = WAL;`.
2. As tabelas de governança residem no `governance.db` durável (ou são preservadas via migração transacional durante publicação de gerações preparadas em `pipeline.c`).
3. Travessia de grafo e montagem de bundle ocorrem em transações de leitura concorrente fora de locks de escrita.
4. A gravação do manifesto de admissão executa transação curta de escrita com `BEGIN IMMEDIATE`, revalidando obrigatoriamente se o `snapshot_id` da base não avançou (TOCTOU guard).

### 2.2. Contrato do `HarnessEvidenceBundle v2`

1. **Representação Bifocal Assimétrica**:
   - `target_mutation_nodes`: Código-fonte integral exato para os alvos de mutação e criação de testes.
   - `dependency_capsules`: Nós dependentes na fronteira $k$-hop compactados via Tree-sitter em assinatura, documentação e `effect_fingerprint`.
2. **`effect_fingerprint` Tri-State Estruturado**:
   - Para erradicar falsos negativos, efeitos colaterais utilizam a semântica `present`, `absent`, ou `unknown`:
   ```json
   "effect_fingerprint": {
     "writes_db": { "state": "present", "reason": "sql_exec_call", "evidence_ref": "tx.go:42" },
     "acquires_lock": { "state": "absent", "reason": "no_sync_primitives_in_ast", "evidence_ref": null },
     "transaction_scope": { "state": "present", "details": "required" },
     "triggers_events": { "state": "unknown", "reason": "dynamic_delegate_dispatch", "evidence_ref": "event.go:15" }
   }
   ```
3. **`seam_map` Tipado**:
   - Projeta hipóteses semânticas determinísticas de integrações transversais (ex: Kafka $\to$ Celery, webhooks assíncronos) rotuladas com `validation_status: "unverified_hypothesis"`.
4. **`snapshot_id` Canônico**:
   - Composto criptograficamente:
     $$\text{snapshot\_id} = \text{SHA256}(\text{canonical}(\text{project\_id}, \text{graph\_generation}, \text{coverage\_revision}, \text{rules\_revision}, \text{extractor\_version}))$$

### 2.3. Ferramentas MCP do CBM

1. `mcp_cbm_extract_harness_bundle`:
   - Monta o bundle bifocal a partir de `mutation_targets`, profundidade `k_hop_depth` e domínios normativos.
   - Utiliza `yyjson_mut_doc` para serialização de alta performance sem fragmentação de memória em C11.
2. `mcp_cbm_evaluate_fast_path`:
   - Avaliação *Fail-Closed*. Retorna `eligible: true` se e somente se:
     1. `coverage_digest.status == 'clean'`.
     2. Zero `unknown` nos efeitos dos nós do escopo.
     3. Nenhuma dependência não resolvida.
     4. Zero alterações em áreas críticas (macros, autenticação, transações, schemas).
     5. `seam_map.seams` vazio (zero costuras transversais).
   - Retorna estrutura rica: `{"eligible": bool, "reason_codes": string[], "blocking_details": []}`.
3. `mcp_cbm_record_admission_manifest`:
   - Persiste auditoria com `BEGIN IMMEDIATE`, valida snapshot atual e bloqueia se houver itens em `blocking_violations`.
4. `mcp_cbm_tradition_propose`:
   - Registra nova regra exclusivamente com status `UNVERIFIED_CANDIDATE`.

---

## 3. CONSEQUÊNCIAS & IMPACTOS

### Positivos
- **Redução de Payload ($\ge 80\%$ em benchmarks favoráveis)**: Reduz drasticamente o consumo de tokens de LLMs mantendo fidelidade de contratos.
- **Fail-Closed Rigoroso**: Impede que código ambíguo passe desapercebido pelo Fast-Path.
- **Rastreabilidade e Não-Repúdio**: O ledger de admissões vincula o estado do grafo ao veredito do JEV antes de qualquer escrita no repositório.
- **Preservação de Tradição**: Regras arquiteturais não sofrem expurgo silencioso e têm ciclo de vida formal.

### Trade-offs e Mitigações
- **Over-Blocking no Fast-Path**: O rigor de zero `unknown` pode encaminhar muitos bundles para revisão do Advisor/JEV. *Mitigação:* Enriquecer progressivamente as tabelas de símbolos conhecidos e bibliotecas padrão.
- **Isolamento de Memória em C11**: Parsing de grandes arquivos AST e serialização JSON requerem alocação segura. *Mitigação:* Uso estrito de `mimalloc` / `slab_alloc` do CBM e `yyjson_mut_doc_free`.

---

## 4. CRITÉRIOS DE TESTES (TDD)

1. **Teste de Redução Bifocal:** Comprovar redução de $\ge 80\%$ de bytes serializados em fixture com 1 alvo e 10 nós dependentes.
2. **Teste de Integridade do Seam Map:** Gerar nós de hipótese não-sintática com proveniência e assegurar caso negativo (tópicos desconectados não geram seam).
3. **Teste de Bloqueio do Fast-Path:** Matriz exaustiva: 1 caso para cada uma das 5 condições impeditivas e 1 caso legítimo de aprovação.
4. **Teste de Concorrência WAL & Stale Snapshot:** Tentativa de admissão contra snapshot modificado deve abortar imediatamente com `STALE_SNAPSHOT`.
5. **Teste de Imutabilidade da Governança:** Rebuild/reindexação do grafo de código deve manter intactas todas as regras em `tradition_rules` e manifestos em `harness_admission_manifests`.
