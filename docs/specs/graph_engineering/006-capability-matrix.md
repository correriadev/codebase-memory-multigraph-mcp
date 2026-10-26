# Matriz nomes × capacidades

Data: 2026-10-23. Inspeção estática limitada; não foi executado spike, teste de runtime ou benchmark. Fonte de localização/cobertura: [ORIENTATION.md](ORIENTATION.md). `Existente` significa implementação localizada, não validação deste produto. `Parcial` exige composição/prova adicional. `Nova` significa responsabilidade proposta TS. `Não comprovada` exige investigação ou experimento, não prova universal de ausência.

| Necessidade / nome do briefing | Superfície observada | Estado | Gap e consequência |
|---|---|---|---|
| Consultar topologia, impacto e cobertura | `get_architecture`, `search_graph`, `trace_path`, `check_index_coverage` | Existente, best effort | DAG e independência são decisões novas; gaps/staleness exigem fonte e serialização conservadora |
| `get_ast_anchors` | Fonte/AST internos e anchors aceitos por `promote_horizon`; não há esse nome no registro inspecionado | Parcial | Spike deve provar extração e validação das âncoras via superfície acessível sem escrever store C |
| `transitive_recall_closure` | `src/admission/recall_engine.c`; docs de recall registram limite de integração | Parcial | Função C interna não é endpoint MCP; não tratar recall como gatilho automático nem dependência disponível |
| `evaluate_seam_map`, `check_effect_fingerprint`, `collect_harness_evidence` | `extract_harness_bundle` e `evaluate_fast_path`; `src/harness/harness_v2.c` | Parcial | Bundle contém seams/efeitos/revisões; não executa testes nem garante equivalência semântica absoluta |
| `admit_fast_path` | `evaluate_fast_path`, `record_admission_manifest` | Parcial | Elegibilidade/manifest não concedem promoção; evidência independente precisa ser produzida e vinculada à versão |
| `apply_mutation_gate` / `promote_horizon_to_base` | Tool `promote_horizon`, handler + `cbm_promote_horizon`; hooks opcionais separados | Parcial | Gate C consolida grafo; não aplica código nem implementa toda política de papéis/revisão do PBB |
| Sessão por agente | `union_session_open/get/close`; host enum codex/antigravity, intent scope 1–4 pares | Existente com limites | Host adapter e reconstrução de sessões precisam prova; identidade declarada não autentica perfil |
| RBAC por builder/adversarial/gatekeeper | `restricted` por contrato desconhecido; gateway por classe de efeito | Nova fronteira TS + prova pendente | Proxy/host confiável precisa impedir chamada direta e caminho alternativo; sem isso bloquear PBI-16/slice |
| Claim exclusivo de edição | `union_claim_capture/resolve` são proposições documentais | Nova TS | Criar WorkClaim, aquisição atômica, lease/fencing e mediação da escrita; não usar ledger documental como lock |
| Veredicto independente | `contest_verify` registra contestação; manifest aceita dados declarados | Nova execução/validação TS | Rodar verificador separado e provar origem/versão/resultado; indisponível/inconclusivo não promove |
| Outer Loop e orçamento | Nenhum nome no registro equivale ao ciclo PBI-01–05 | Nova TS | Estados, reservas/custo em voo, retries e breaker; contratos C02/C03 |
| Checkpoint de execução | Sessão/horizonte e mutation journal têm funções delimitadas | Nova TS | Journal próprio; checkpoints F05 e reconciliação entre stores; não restaurar conversa por suposição |
| Descarte `purge_speculative_territory` | `cbm_discard_horizon` interno; `union_session_close` fecha sessão e destrói conteúdo | Parcial | Validar descarte do tipo exato de território via API suportada; sem endpoint geral demonstrado, F05 tem bloqueio de capacidade |
| Event Dispatcher e fontes | Watcher (`watcher.h`) entrega callback de reindexação; `ingest_traces` é ingestão | Nova TS / fontes a validar | Definir detecção de seam/anomalia, inbox/outbox e dedupe; não presumir push confiável de evento de negócio |
| Deep link de nó/aresta | `graph-ui/src/App.tsx` lê workspace/project; há seleção por código na UI | Não comprovada | Validar URL/seleção estável na UI existente; pacote textual não substitui o link requerido PBI-25 |
| Evolução de temas | Tools normativas de Union e catálogo versionado; regras locais harness são store distinto | Parcial | Operador autenticado, alvo e versão corretos; validar contrato real anunciado/handler antes de ativação |
| Admissão de skills | `skills/README.md`: constellation/level deferred, análise apenas | Não comprovada / habilitação futura | PBI-30 precisa capacidade de ativação/reversão real; não declarar atendido com relatório ou attestation |
| Telemetria e medição comparativa | Logs/ledgers existentes não constituem telemetria do novo orquestrador | Nova TS | PBI-31 mínimo acompanha primeiro slice; PBI-32 depende de baseline/limiares humanos Q08 |

## Evidências estáticas principais

- `src/mcp/mcp.c:880–959`, `1064–1095`: registro/schema de tools de horizonte, sessão e harness; comparação literal de nomes citados no briefing com esse registro não encontrou os aliases acima. Isso não equivale a busca exaustiva de capacidades internas.
- `src/mcp/promote_handler.c:46–115`: lookup de sessão, contestações, recusas e override legado; nenhuma dessas verificações é, por si, prova de papel adversarial ou evidência verde da versão candidata.
- `src/admission/admission_gate.c:1538–1552`: helper verifica presença do ID e override; handler possui verificações adicionais. Não atribuir ao helper a política inteira de admissão.
- `src/union/union_session.c:129–169`, `src/mcp/union_handler.c:875–881`: semântica observada de restricted. `src/union/union_claim.h:1–8,81–129`: proposição e store de claims, não exclusão mútua de escrita.
- `src/mcp/harness_handlers.c:397–466`: elegibilidade server-side e campos de manifest; `tdd_oracles_passed` default true é declaração. `docs/feature/harness_v2.md` descreve diferenças schema/handler.
- `src/watcher/watcher.h:1–7,24–40`: callback orientado à indexação. Range parcial `watcher.c:1041` lido diretamente; não fundamenta capacidade de dispatcher.
- `graph-ui/src/App.tsx:767,1508–1520,1561`: parâmetros observados. Busca textual limitada em `graph-ui/src` não confirmou contrato URL de nó/aresta; manter pendência.
- `skills/README.md`: procedimentos não compõem pipeline automático, skills deferred não admitem alterações reais, sessões Union e overlays fractais têm lifecycle diferente.

## Spike de viabilidade: definição, não execução

Preservar os cinco critérios do ADR-005: ciclo ponta a ponta com decisão registrada; kill/reinício consistente; cancelamento efetivo e registrado; custo persistido; recusa observada fora do intent scope. Acrescentar aos cenários do spike a tentativa direta de promoção pelo Builder, a janela de mudança de versão e a indisponibilidade do verificador, como prova das invariantes do slice — sem afirmar que já passaram.

Saídas exigidas do experimento futuro: build/commit, host e versão, configuração efetiva, comandos, dados de entrada, logs de processos e chamadas, journal/estado CBM antes/depois, resultados de cada critério e limitações. Apenas evidência positiva de todos os critérios permite propor ACCEPTED; esta etapa mantém o ADR PROPOSED. Gap que exija mudança C retorna ao operador para decisão explícita, sem ampliar o slice silenciosamente.

Fechar o relatório SP-05 não comprova viabilidade: a entrada em produção exige S0_APPROVED conforme 003. Após o spike, ER-17 deve entregar o adapter concreto do primeiro host, aproveitando a integração experimental quando adequada. Para evolução, cada destino precisa provar CAS da versão ou serialização por alvo que cubra todos os escritores; consulta seguida de escrita não impede rollback obsoleto. Essa capacidade permanece a validar e não é atribuída automaticamente às tools existentes.
