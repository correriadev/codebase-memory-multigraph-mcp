# Problem Space — Harness Perfeito v2

**Domínio:** `harness_perfect_v2`  
**Projeto:** `codebase-memory-multigraph-mcp` (CBM-MGH)  
**Data:** 2026-10-09  
**Estado:** especificação de evolução; não constitui implementação nem aprovação de novas regras normativas.

## 1. Objetivo e limites do domínio

Transformar o CBM-MGH no substrato topológico e oráculo normativo da Tríade Fractal: produzir evidência contextual compacta e verificável, identificar quando uma mudança exige análise aprofundada e registrar a admissão do JEV antes da execução. O resultado esperado é reduzir o contexto de dependências sem reduzir a fidelidade dos alvos de mutação, conservar a autoridade humana sobre a Tradição e preservar a governança entre gerações do grafo.

O [ADR-004](../../adr/ADR-004-HARNESS-PERFEITO-V2.md) é a fonte normativa desta evolução; [ADR.md](../../../ADR.md) é seu registro. A [arquitetura](../../adr/ARCHITECTURE.md), o [ADR-002](../../adr/ADR-002-THREE-TERRITORIES.md) e o [ADR-003](../../adr/ADR-003-TWO-TIER-ANCHORS.md) dão os limites de integração. A Tríade Fractal distingue realização observada, intenção de mudança e tradição normativa. O CBM oferece evidência e consulta normativa; o Advisor formula hipóteses; o JEV julga a conformidade; o operador humano aprova normas. Esses papéis não são intercambiáveis.

Estão no escopo: `HarnessEvidenceBundle v2`, representação bifocal, impressão de efeitos tri-state, `seam_map`, avaliação do Fast-Path, ciclo de regras, ledger de manifestos, coordenação de snapshot e quatro ferramentas MCP. Execução de alterações no repositório, algoritmo de pontuação do JEV, execução de Kafka/Celery e publicação de cânones externos ficam fora desta entrega. A admissão registrada não substitui os gates de mutação, contestação e âncoras existentes.

### 1.1 Invariantes de negócio

| ID | Invariante |
|---|---|
| INV-01 | Alvos de mutação contêm seus bytes de fonte completos e exatos; a compactação aplica-se às dependências. |
| INV-02 | Falta de evidência não equivale à ausência de efeito: despacho dinâmico ou cobertura insuficiente produz `unknown`. |
| INV-03 | Toda costura é uma hipótese tipada, com proveniência, nunca uma relação sintática comprovada. |
| INV-04 | Fast-Path exige cobertura `clean`, zero efeitos `unknown`, zero dependências não resolvidas, zero alterações críticas e `seam_map.seams` vazio. |
| INV-05 | Propostas entram em `UNVERIFIED_CANDIDATE`; somente um operador humano autenticado promove para `APPROVED`. |
| INV-06 | Ao vencer a cadência, uma regra é avaliada como `REVIEW_PENDING`, permanece disponível e conserva sua força normativa até revisão humana. |
| INV-07 | Um manifesto com violações bloqueantes não é admitido nem inserido no ledger. |
| INV-08 | A admissão compara o `snapshot_id` atual sob exclusão coordenada com publicadores; qualquer divergência causa rollback integral. |
| INV-09 | Rebuild, reindexação, cancelamento e swap de geração não removem regras nem manifestos comprometidos. |
| INV-10 | Elegibilidade ao Fast-Path não significa conformidade normativa, autoridade humana ou autorização de escrita no repositório. |

## 2. Event Storming

O fluxo abaixo é temporal por caminho de negócio. Ramificações de recusa e o ciclo independente de regras são explícitos. Eventos de cálculo são transitórios; eventos de persistência só são observáveis como sucesso após commit. A especificação não exige um broker de eventos.

| # | Evento de domínio, no passado | Comando disparador | Agregado responsável | Sistemas/atores externos ao domínio | Modelo de leitura e consumidor |
|---|---|---|---|---|---|
| 01 | Geração Topológica Publicada | Publicar geração preparada | Snapshot de Evidência | Pipeline de indexação | Snapshot atual — extrator e JEV |
| 02 | Escopo de Mutação Resolvido | Extrair bundle | Bundle de Evidência | Cliente MCP/orquestrador | Alvos e fronteira k-hop — Advisor |
| 03 | Cobertura do Escopo Apurada | Consultar cobertura do snapshot | Bundle de Evidência | Indexador | `coverage_digest` — JEV e avaliador |
| 04 | Alvos Integrais Capturados | Capturar fonte dos alvos | Bundle de Evidência | Repositório de código | `target_mutation_nodes` — agente de mutação/testes |
| 05 | Cápsulas de Dependência Derivadas | Extrair contratos e efeitos | Bundle de Evidência | Parser Tree-sitter | `dependency_capsules` — Advisor e JEV |
| 06 | Hipóteses de Costura Projetadas | Correlacionar integrações tipadas | Bundle de Evidência | Evidências de configuração Kafka/Celery | `seam_map` — Advisor e JEV |
| 07 | Regras Aplicáveis Vinculadas | Consultar Tradição por domínio | Bundle de Evidência | Catálogo temático externo, somente leitura | Normas e versões — JEV |
| 08 | Bundle de Evidência Selado | Canonicalizar e calcular hashes | Bundle de Evidência | Cliente MCP | Bundle, `bundle_hash` e `snapshot_id` — JEV |
| 09 | Fast-Path Considerado Elegível | Avaliar Fast-Path | Avaliação de Fast-Path | Orquestrador | `eligible=true` — orquestrador |
| 10 | Fast-Path Bloqueado | Avaliar Fast-Path | Avaliação de Fast-Path | Orquestrador | `reason_codes` e detalhes — Advisor/JEV |
| 11 | Conformidade Julgada | Julgar evidência e mudança | Veredito JEV, externo ao CBM | JEV | Pontuação, bloqueios e avisos — ledger |
| 12 | Admissão Recusada por Violação | Registrar manifesto | Manifesto de Admissão | JEV | Recusa estruturada — JEV/orquestrador; nenhuma linha admitida |
| 13 | Admissão Recusada por Snapshot Obsoleto | Revalidar snapshot e registrar manifesto | Manifesto de Admissão | Publicador concorrente | `STALE_SNAPSHOT` — JEV/orquestrador; requer nova evidência |
| 14 | Manifesto de Admissão Registrado | Registrar manifesto sem bloqueios | Manifesto de Admissão | JEV | Ledger durável — auditor e gate de execução |
| 15 | Regra de Tradição Proposta | Propor regra | Regra de Tradição | Advisor/agente | Candidatos não verificados — operador humano |
| 16 | Regra de Tradição Aprovada | Aprovar candidata via canal humano | Regra de Tradição | Operador humano autenticado | Regras aplicáveis e revisão normativa — extrator/JEV |
| 17 | Revisão de Regra Tornada Pendente | Avaliar cadência em consulta | Regra de Tradição | Relógio confiável do servidor | Pendências, com norma preservada — operador/JEV |
| 18 | Regra de Tradição Revisada | Revisar e aprovar via canal humano | Regra de Tradição | Operador humano autenticado | Nova revisão e prazo — extrator/JEV |

### 2.1 Políticas e caminhos alternativos

Após Escopo de Mutação Resolvido, a extração usa uma geração fixada durante toda a leitura. Alvo inexistente ou fonte divergente gera recusa de extração; uma dependência incompleta pode ser representada com lacuna explícita e efeitos `unknown`, sem alegar cobertura limpa. Uma travessia truncada não resulta em um bundle aparentemente completo.

Fast-Path Bloqueado encaminha a evidência ao fluxo completo Advisor/JEV, sem executar mudanças. Um seam Kafka → Celery não aciona mensagens, workers ou tarefas: registra apenas uma hipótese para verificação. Manifesto de Admissão Registrado habilita a consulta ao ledger; a execução ainda deve cumprir os gates existentes e revalidar a evidência no momento da mutação.

Regra de Tradição Proposta não altera o catálogo temático externo. O ledger local mantém propostas e vínculos normativos aprovados no projeto. A promoção local não confere autoridade para publicar um cânone coletivo. `CONSULTED` gera orientação/avisos; `NORMATIVE` pode gerar bloqueios no julgamento do JEV, inclusive quando `REVIEW_PENDING`.

## 3. Classificação de subdomínios

| Subdomínio | Tipo | Justificativa |
|---|---|---|
| Evidência Bifocal | Core | Preserva contexto de mutação e reduz dependências com limites epistemológicos explícitos; diferencia o harness de um indexador comum. |
| Admissão Normativa | Core | Combina evidência, política fail-closed e integridade temporal para impedir admissões baseadas em conhecimento obsoleto. |
| Tradição Normativa | Core | Preserva normas, cadência e autoridade humana como parte verificável da engenharia. |
| Topologia e Cobertura | Supporting | Fornece símbolos, relações e lacunas necessários ao Core; reutiliza o pipeline existente. |
| Projeção de Costuras | Supporting | Expõe hipóteses entre fronteiras para análise do Core, sem assumir verificação semântica completa. |
| Transporte MCP | Generic | Publica contratos JSON-RPC com infraestrutura já disponível; não redefine a semântica do domínio. |
| Persistência e Criptografia | Generic | SQLite WAL, SHA-256 e bibliotecas JSON fornecem mecanismos; as invariantes de admissão pertencem ao Core. |

## 4. Glossário da Linguagem Ubíqua

| Termo canônico | Definição estrita | Anti-patterns e usos a evitar |
|---|---|---|
| Tríade Fractal | Separação entre realização observada, intenção de mudança e tradição normativa, aplicada em cada escopo. | Confundir opinião do agente com norma aprovada. |
| Substrato Topológico | Representação de símbolos, relações, gerações e lacunas usada como evidência do código. | Chamar grafo parcialmente indexado de verdade completa. |
| Bundle de Evidência (`HarnessEvidenceBundle v2`) | Conjunto selado de alvos, cápsulas, costuras, cobertura e regras vinculado a um snapshot. | Usar bundle como autorização automática de mutação. |
| Representação Bifocal Assimétrica | Preservação integral dos alvos e apresentação compacta dos contratos de dependência. | Comprimir alvos ou confundir os tiers com os tiers de âncoras do ADR-003. |
| Alvo de Mutação (`target_mutation_node`) | Símbolo ou arquivo explicitamente selecionado para alteração/teste, com fonte exata de sua extensão declarada. | Retornar somente assinatura, corpo truncado ou código reconstruído. |
| Cápsula de Dependência (`dependency_capsule`) | Contrato de dependência com assinatura AST, documentação declarada e impressão de efeitos. | Sintetizar documentação como se fosse declarada ou deduzir pureza pela ausência de chamadas conhecidas. |
| Impressão de Efeitos (`effect_fingerprint`) | Estado `present`, `absent` ou `unknown`, por categoria de efeito, com motivo e proveniência. | Booleanos sem justificativa; `absent` para despacho dinâmico ou parsing incompleto. |
| Mapa de Costuras (`seam_map`) | Coleção de hipóteses tipadas de integração entre fronteiras, identificadas como `unverified_hypothesis`. | Converter hipótese Kafka → Celery em aresta sintática comprovada. |
| Digest de Cobertura (`coverage_digest`) | Resumo `clean`, `partial` ou `missed` do escopo e lacunas da leitura fixada. | Interpretar `clean` como prova de semântica completa. |
| Snapshot de Evidência (`snapshot_id`) | Identidade criptográfica do estado do projeto, geração, cobertura, regras e versão do extrator. | Usar apenas commit Git, timestamp ou handle de banco antigo. |
| Tradição Normativa (`tradition_rules`) | Regras locais e vínculos ao cânone com domínio, força, revisão e ciclo de autoridade humana. | Publicar cânone externo ou permitir autopromoção por agente. |
| Cadência de Revisão | Intervalo positivo e prazo absoluto que tornam uma regra aprovada pendente de revisão quando `now >= review_due_at`. | Expirar, apagar ou enfraquecer a norma silenciosamente. |
| Fast-Path | Elegibilidade à rota simplificada somente quando todas as condições estruturais do ADR-004 são satisfeitas. | Equiparar elegibilidade a pontuação JEV ou permissão de escrita. |
| JEV (Joint Epistemic Verifier) | Autor externo do julgamento de conformidade da mudança contra evidência e normas aplicáveis. | Inventar limiar de pontuação não definido pelo ADR. |
| Manifesto de Admissão (`harness_admission_manifest`) | Registro durável e imutável de julgamento sem bloqueios, vinculado ao bundle, mudança e snapshot revalidado. | Inserir admissões recusadas como se fossem admitidas ou sobrescrever histórico. |

## 5. Refinamento e critérios mensuráveis

Os dados do pedido resolvem escopo, projeto e domínio. Não há pergunta de negócio impeditiva. As escolhas abaixo refinam opções técnicas abertas do ADR e são propostas de design do modelo, não respostas atribuídas ao usuário:

| ID | Categoria | Decisão proposta | Evidência e motivo |
|---|---|---|---|
| Q01 | Dados | Usar `governance.db` durável separado, com conexão dedicada gerenciada e `synchronous=FULL`. | Opção explícita do ADR-004; a publicação substitui arquivos do grafo. |
| Q02 | Concorrência | Coordenar publicação e admissão por fence por projeto entre processos, além de `BEGIN IMMEDIATE`. | A transação em um banco separado não bloqueia o swap do outro banco. |
| Q03 | Integridade | Tratar transição de cadência como mudança da revisão normativa efetiva no snapshot, sem escrita obrigatória durante consultas. | Evita snapshot inalterado quando uma regra se torna pendente; preserva leituras passivas. |
| Q04 | Autoridade | Encaminhar promoção a canal humano confiável externo às quatro novas ferramentas; não aceitar identidade humana declarada no payload. | Promoção exclusivamente humana no ADR-004 e limites do ADR-002. |
| Q05 | Aceite | Medir redução de bytes JSON contra bundle de fonte integral, com mesmos metadados, em fixture fixa de 1 alvo e 10 dependências; publicar também a redução da projeção de código. | Evita mascarar metadados e atende ao benchmark favorável do ADR-004. |

Aceite: redução bifocal de pelo menos 80% na fixture definida; alvos byte a byte preservados; seams com proveniência e caso negativo desconectado; matriz das cinco condições do Fast-Path; concorrência WAL sem perda; rollback por stale snapshot; governança preservada após rebuild. Os métodos e cenários estão em [003](003-codebase-memory-multigraph-mcp-tactical-design.md) e [004](004-codebase-memory-multigraph-mcp-test-scenarios.md).
