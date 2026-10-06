# Auditoria ampliada dos objetivos do projeto

Data: 2026-10-03. Código: `973c418d365c916edd417ee9302ad03ec17b51aa`.

## Escopo e conclusão

A rodada amplia a revisão do Plano Fractal para os objetivos documentados: inteligência de código local, indexação, horizontes, admissão, governança de efeitos, memória documental, tradição e conformidade dos clientes.

O conceito central é a repetição das mesmas fronteiras de evidência, autoridade, escopo e promoção em cada nível do trabalho: uma narrativa ou execução bem-sucedida não substitui prova de admissão. Realização, idealização e tradição têm jurisdições distintas; uma referência temática não autoriza escrever na fonte.

**A graduação integral não está demonstrada.** Há falhas concretas em caminhos existentes e aceites documentais ainda sem evidência nesta rodada. O escopo de requisitos foi ampliado; isso não representa inspeção exaustiva de todo o código, de todas as linguagens ou execução de todos os experimentos.

Método: leitura direta de documentação, fontes, testes e Git; nenhuma consulta ao CBM instalado ou aos artefatos do índice compartilhado. Nenhuma correção de produção foi aplicada. A skill `the-grumpy-tech-lead` orientou a revisão por evidência e perguntas sobre impactos.

## Autoridade dos documentos

- `docs/README.md`, ADRs, documentos de features e infraestrutura descrevem contratos da implementação.
- `docs/PRD/PRD.md` distingue baseline [B], código desativado [C], evolução [E], decisão aberta [A] e graduação [G]; declara precedência do paper. Esta auditoria não resolve divergências normativas com o paper sem verificar a passagem correspondente.
- Os escopos atômicos em `docs/PRD/novos-paradgimas/specs/README.md` estão em REVIEW; não são uma declaração de implementação completa.
- PRDs históricos, arquivo e REWORK-LOG contextualizam decisões. Problemas históricos corrigidos não foram reapresentados como atuais.
- `docs/.digest.md` aponta commit e geração de 2026-10-01 anteriores ao HEAD; serve como orientação, não como comprovação atual de implementação.

## Matriz dos objetivos de graduação

| Objetivo | Aceite documental | Evidência desta rodada | Resultado |
|---|---|---|---|
| O1 / G0 | Semântica dos seis estados e separação workflow/lifecycle | ADRs, contratos e testes Union | Parcial: testes existentes não são prova completa das transições do PRD |
| O2 / G1 | Cada tese core [E] vira [B], revogada ou adiada explicitamente | PRD e scopes REVIEW | Pendente: falta registro de graduação item a item comprovado nesta rodada |
| O3 / G2 | Três flavors L0–L1, evidenciados por logs dos hosts | Suites de adapters/configuração e gateway | Pendente: testes locais não substituem sessões reais dos três hosts |
| O4 / G3 | Promoção de primeira classe verificável pelo host | Handler e admission gate; A01–A03 abaixo | Não conforme nos caminhos examinados |
| O5 / G4 | Gate persistente com mérito cego ao chamador | Testes de contestação e promoção Union | Parcial: a API de promoção de horizontes precisa de demonstração integrada das mesmas fronteiras |
| O6 / G5 | Recall igual ao fechamento reverso exato de derivação | Recall engine; A05 | Não conforme na API examinada; integração de produção não encontrada na busca de referências |
| O7 / G6 | CHANGE_READY condicionado a unresolved, seq e assumptions | Contratos/grounding e PRD | Pendente: não foi executado o oráculo independente dos três predicados |

## Cobertura de todo o escopo documental

“Parcial” significa evidência delimitada, sem certificação do domínio. “Pendente” não é afirmação de ausência de implementação.

| Domínio / requisitos | Fontes e testes relacionados | Validação / lacunas |
|---|---|---|
| Descoberta: search, Cypher, trace, snippets, arquitetura | `src/mcp/mcp.c`, store, query; store/Cypher | Base exercitada; federação falha nos filtros (A04); precisão multilíngue não medida |
| Indexação, cobertura, gerações e Git impact | index supervisor, pipeline, MCP; `test_index_supervisor.c` | Supervisor exercitado; campanha de incremental, linguagens e comparação completa não executada |
| Federação local, escopo, isolamento e merge | `src/mcp/handlers.c`, horizon pool, kway merge | Inspeção dos overlays; A04; parser de active_horizons não prova semântica de consulta |
| Âncoras e admissão / ADR-003 | promote handler, admission gate, anchor checker | Não conforme: A01–A03; testes de hash não demonstram equivalência AST |
| FR-A: máquina epistemológica | Union session, claim, contract, refusal | Suítes exercitadas; seis estados, assessment versus decisão e seq exigem matriz de transições própria |
| FR-B: DAG e fronteiras INITIATE/PROMOTE/CONTEST | Union promotion/contest; admission | Testes Union exercitados; promoção MCP de horizonte tem A02; DAG completo e distillation/exclusions não certificados |
| FR-C: recall, coordenadas e reabilitação | recall engine, Union contest/drift | A05; falta demonstração integrada de RecallNotice, cascata, coordenadas e reabilitação célula a célula |
| FR-D: router, escalonamento, CHANGE_READY | Union session/contract/ledger | Parcial; alcançabilidade de timeout, snapshot durante aprovação e predicado triplo pendentes |
| FR-E: operador escopado | autoridade MCP, bindings, scope validator | Testes locais exercitados; A06; TTL, seq e autorizações single-use do PRD não certificados |
| FR-F: capability gateway e seam | `src/union/mutation_gate.c`, gateway, ledger, MCP mutation guard | Escopo, host e journal examinados; testes exercitados; classificação/doctor e efeitos reais em cada host pendentes |
| FR-G: horizontes, engine, orçamento | horizon pool/reaper; Union ledger/ECG | Parcial; reaper e recall não têm chamada de produção encontrada por busca direta; não implica ausência de invocação externa |
| FR-H: protocolo, conformance, ecossistema | MCP schemas, client adapters/profiles | Testes de adapters exercitados; A10; interoperabilidade EAP e logs por flavor pendentes |
| FR-I: bootstrap e código desativado | main, CLI, build, adapters | Não foi realizada auditoria exaustiva de código morto nem inventário de ativação [C] |
| FR-J: observabilidade e experimentação | Union trace/ECG; planos de avaliação | Testes locais exercitados; métricas H1–H12 e catálogo adversarial T1–T14 não foram integralmente executados |
| Plano Documental / scopes C01–C07 | claim, anchor, sweep, doc_l0, doc_edges, drift | Suítes exercitadas; roundtrip L0 por host, orfandade/aging e persistência sob falha ainda exigem campanha integrada |
| Tradição / scopes D01–D06 | theme registry, binding, routing, founding, cross-territory | Suítes exercitadas; A06–A10 permanecem; referências não autorizam promover alterações na fonte |
| Skills / scopes B01–B08 | contratos e grounding; artefatos de skills | Contratos internos exercitados; comportamento das skills externas e graduação por nível não certificados |
| Infraestrutura Windows, build, instalação e recursos | Makefile.cbm, platform, CLI/activation | Runner previamente compilado reutilizado; não houve novo build/install nem validação de pacote de assets nesta rodada |
| UI, HTTP, desempenho e linguagens | HTTP/UI, pipeline, EVALUATION_PLAN | Pendentes nesta rodada: UI, benchmarks, matriz completa de linguagens, TSan e cobertura percentual |

### Requisitos não funcionais NFR-1–NFR-18

| Grupo | Estado da evidência |
|---|---|
| NFR-1, 2, 3, 4, 12: âncoras, cobertura, drift, atomicidade e autoridade | A01–A03 impedem declarar conformidade global; suítes Union dão evidência local |
| NFR-5, 16: chaves canônicas e contratos estruturados | Testes de contratos exercitados; todas as bordas do DAG não verificadas |
| NFR-6: recusa nunca apresentada como sucesso | A02 e A05 demonstram sucesso sem garantia do efeito completo |
| NFR-7, 15: log durável separado e memória governada | Ledger exercitado; A07 impede garantir conservação sob concorrência |
| NFR-8, 9: polling e gate offline | Fluxo completo sem camada viva/rede não executado |
| NFR-10: não fabricar evidência | Propostas e aceites sem prova permanecem pendentes neste relatório |
| NFR-11: mérito cego ao chamador | Teste Union passa; campanha com N identidades nos endpoints reais pendente |
| NFR-13: registro antes do efeito | Ordem examinada no mutation gate e testes locais; falha entre journal e efeito real pendente |
| NFR-14: exaustão nunca promove | Prova de alcançabilidade e timeout ao vivo não executadas |
| NFR-17: interpretação versionada | A08 mantém janela entre verificação da geração e consulta |
| NFR-18: custo como resultado experimental | Não houve medição de custo, carga ou experimento nesta rodada |

## Achados confirmados por inspeção

As reproduções abaixo são cenários derivados do código, não ensaios adversariais executados nesta rodada.

### A01 — P1: leitura fora da alocação no parser de promoção

Evidência: `src/mcp/promote_handler.c:109–120`. `byte_len` do JSON é usado no hash do buffer `expected_text` antes das verificações do anchor checker. Um anchor com texto `x`, `byte_len=1000000` e sem hash solicita leitura além do buffer e da alocação. O teste de oversized length chama apenas o checker, não esse parser.

Pergunta: como a entrada MCP garante segurança de memória antes de derivar a assinatura?

### A02 — P1: sucesso de promoção sem garantia da consolidação

Evidência: `src/admission/admission_gate.c:49,65,77–148`. Âncoras vazias pulam verificação; o estado muda antes da consolidação; erros de BEGIN, INSERT e COMMIT não governam o resultado; base_db ausente também chega a OK. Uma base read-only ou bloqueada pode deixar o horizonte PROMOTED sem seus dados consolidados.

Pergunta: qual evidência torna o sucesso de promoção inseparável do commit e da verificação exigida pelo ADR?

### A03 — P2: fallback denominado AST usa busca textual

Evidência: `src/admission/anchor_checker.c:124–146`. O fallback busca substrings e compara FNV, incluindo normalização textual; não identifica um nó AST. Manter o texto anterior num comentário enquanto se altera a função pode satisfazer a busca. Testes atuais de whitespace apenas calculam um hash.

Pergunta: como o verificador distingue o símbolo vivo de uma ocorrência textual em comentário ou literal?

### A04 — P1: overlays ignoram a semântica solicitada

Evidência: `src/mcp/handlers.c:198,287,319,398`. As consultas dos horizontes enumeram nós/arestas sem aplicar filtros de search/Cypher ou raiz, direção e profundidade do trace. Um filtro que não encontra nenhum nó na base ainda pode devolver nós não relacionados no overlay; trace pode trazer arestas de outro símbolo.

Pergunta: qual contrato demonstra equivalência entre consultar a base e consultar a composição federada?

### A05 — P1: recall não calcula fechamento exato

Evidência: `src/admission/recall_engine.h:8`, `src/admission/recall_engine.c:39–41,97,115` e `src/core/visited_set.h:10`. A cascata usa profundidade 5 e armazena no máximo 256 afetados; somente estes recebem atualização. Uma cadeia com dependente no nível 6 ou estrela com 257 dependentes deixa claims sem invalidar apesar do retorno de sucesso. Os testes atuais verificam NULL/init e constante de capacidade. A busca em `src` encontrou definição e declaração, sem chamador de produção; a falha é confirmada no contrato da API, não como incidente de um endpoint em uso.

Pergunta: como o resultado distingue fechamento completo de truncamento para cumprir O6/G5?

### Achados temáticos mantidos da rodada anterior

| ID / prioridade | Evidência atual | Consequência e cenário |
|---|---|---|
| A06 / P1 | `src/mcp/union_handler.c:1539`; `src/union/union_binding.c:158–162` | Credencial só é exigida para o tipo novo NORMATIVE; novo CONSULTED substitui binding NORMATIVE do mesmo tema sem autenticar essa remoção de obrigação |
| A07 / P2 | `src/union/union_binding.c:20–63` | Dois ledgers abertos sobre o mesmo estado salvam snapshots completos sem lock/reload; a segunda escrita perde a primeira, mesmo com ambos retornando sucesso |
| A08 / P1 | `src/mcp/union_handler.c:390–402,1408–1441` | A geração é verificada com um store que é fechado; os handlers abrem/resolvem outro store para consultar; reindex nessa janela pode devolver dados de outra geração sob versão antiga |
| A09 / P2 | `src/union/union_theme_registry.c:332–349` | Processo morto após mkdir deixa .lock persistente; próximas publicações falham após restart, sem recuperação de proprietário examinada |
| A10 / P2 | `src/mcp/mcp.c:894–895`; handler de bindings | tools/list requer validated_by e não anuncia operator_id/operator_token usados na validação normativa; cliente guiado pelo schema não recebe o contrato necessário |

## Plano de validação complementar

### Extensão do refinamento de escopo — 2026-10-03

O refinamento está em [Problem Space](../specs/project_objective_conformance/001-problem-space.md), [Context Map](../specs/project_objective_conformance/002-context-map.md), [desenho tático](../specs/project_objective_conformance/003-codebase-memory-multigraph-mcp-tactical-design.md) e [cenários de teste](../specs/project_objective_conformance/004-codebase-memory-multigraph-mcp-test-scenarios.md), com status **REVIEW**. Esses documentos organizam os achados como entregas, invariantes, dependências e critérios de aceite; não declaram correções implementadas ou testes novos executados. O usuário solicitou continuar as fases 3–4 após o registro das respostas socráticas, mantendo decisões abertas explicitadas.

#### A11 — P1 proposto: perda de arestas distintas no trace federado

Leitura adicional: `src/mcp/handlers.c:398` usa `source_uri` como chave do cursor; `src/query/kway_merge.c:119–121` elimina registros de chave igual. Duas arestas A→B e A→C no mesmo horizonte compartilham a chave e uma é descartada. A própria tabela de consolidação usa identidade composta por origem, destino e tipo (`src/admission/admission_gate.c:84`). Este problema é independente dos filtros de A04: mesmo corrigindo o filtro, a multiplicidade ainda precisa ser preservada.

Aceite: o trace conserva todas as arestas distintas; duplicatas reais têm política explícita; permutar horizontes e paginar não perde nem duplica arestas. Cenário derivado do código, ainda não executado.

#### A12 — jurisdição de projetos, bases e evidências

Leitura adicional: `src/mcp/promote_handler.c:164–178` usa o nome recebido em `%cache%/%project%.db` e abre a base em modo escrita. O dispatch em `src/mcp/mcp.c:17679–17683` chama esse handler. Na fronteira examinada, `../outside` pode selecionar uma base existente fora do cache. `src/admission/anchor_checker.c:7–18` aceita caminhos absolutos ou concatena caminhos relativos sem demonstrar confinamento à raiz do projeto. A escolha do projeto também chega à interpolação SQL em `src/admission/admission_gate.c:79–82`.

Escopo adicional: estabelecer uma única identidade e jurisdição para sessão, horizonte, base de destino e evidência; verificar traversal, caminhos absolutos, links e seleção cruzada de projeto. A observação é do handler e do checker, não uma declaração de exploração remota ou de bypass de toda a autorização do transporte. A interpolação SQL precisa de ensaio específico antes de receber classificação de explorabilidade.

Aceite: a promoção só alcança a base autorizada; evidências externas seguem uma política explícita; entradas que escapam da jurisdição são recusadas antes de abrir ou alterar bases; nenhum projeto é alterado por seleção ambígua.

#### Ordem proposta de entrega

| Etapa | Escopo | Dependência | Evidência de encerramento |
|---|---|---|---|
| 1 | Segurança de entrada A01 e jurisdição A12 | Definir política para evidências externas e seleção de projeto | Entradas adversariais recusadas antes de leitura/abertura indevida |
| 2 | Admissão A02 e âncoras A03 | Etapa 1; definir relação entre promoção de horizonte e gate Union | Falhas não produzem promoção aparente; prova identifica símbolo correto |
| 3 | Consultas A04 e identidade de arestas A11 | Definir identidade, precedência e paginação da composição | Igualdade com oráculo do grafo composto |
| 4 | Recall A05 | Definir grafo de derivação admitido e integração operacional | Fechamento reverso exato, persistência e falha explícita |
| 5 | Autoridade/versionamento A06 e A08; persistência A07 e A09; schema A10 | Decidir transições de binding e ciclo de vida das versões | Autorização, conservação de escrita e consistência dados/versão verificáveis |
| 6 | Critérios O1–O7 e aceites transversais | Entregas corretivas anteriores e registro das decisões abertas | Matriz de requisitos com prova por critério; aceites ausentes continuam pendentes |

As etapas são unidades de aceite, não uma exigência de execução totalmente serial; consultas e persistência podem evoluir independentemente quando seus contratos estiverem definidos. O número histórico de testes aprovados não substitui esses aceites.

### Campanha original e complementos

| Prioridade | Ensaio | Oráculo de aceite |
|---|---|---|
| P1 | JSON adversarial no promote_horizon: comprimento excessivo, negativo, ausente e anchors vazias | Recusa tipada; sem acesso inválido sob ASan/UBSan; sem alteração de estado |
| P1 | Base read-only, BEGIN busy, INSERT inválido, falha de COMMIT | Sem falso OK; estado e dados coerentes após reabrir |
| P1 | Search/Cypher/trace com nós e arestas não relacionados em múltiplos horizontes | Resultado igual ao grafo composto de referência, respeitando filtros e direção |
| P1 | Recall com cadeia >5, estrela >256, diamantes e ciclos proibidos | Conjunto afetado exatamente igual ao fechamento esperado ou recusa explícita sem sucesso parcial |
| P1 | Binding normativo seguido de downgrade sem credencial e com outra identidade | Política de autoridade demonstrada por recusa/registro; obrigação não desaparece sem autorização |
| P1 | Reindex entre checagem de geração e leitura temática | Dados e versão pertencem à mesma geração; ausência de mistura comprovada |
| P2 | AST: função alterada, texto antigo em comentário; whitespace dentro de literal | Equivalência semântica corretamente diferenciada da coincidência textual |
| P2 | Escritores de ledger concorrentes e kill após aquisição de lock | Nenhuma atualização reconhecida perdida; recuperação demonstrada após restart |
| P2 | tools/list seguido de cliente que usa somente schema anunciado | Chamada normativa válida e chamadas inválidas recebem resultados coerentes |
| Graduação | O1/O7: tabela completa de transições e três predicados independentes | Guardas comprovadas; timeout/exaustão nunca alcançam promoção |
| Graduação | O3/O4/O5: sessões nos três hosts, mérito sob N identidades e falhas do seam | Logs do host correlacionados ao journal e ao estado persistente |
| Graduação | O2 e FR-J: inventário de teses, T1–T14, H1–H12 e métricas | Cada tese recebe destino explícito; experimentos têm resultados e limitações preservados |
| Plataforma | Clean build Windows, instalação isolada, assets/UI/HTTP, stdio E2E, cobertura e matriz de linguagens | Artefatos identificados; resultados por plataforma; limiares dos ADRs medidos, não presumidos |

## Execução desta rodada

Reutilizado o runner de testes compilado na etapa anterior, cujo código de produção/testes corresponde ao commit auditado; não foi feito rebuild nesta rodada. SHA-256: `01EAAB8F2B9ECCDA6090DF5AB227FD6575558F2297E9BD5637456DFB83294890`.

Cache de ensaio: `%TEMP%/cbm-project-audit-20261003/cache`. Logs: `test-results.log` e `test-platform-results.log` na mesma pasta pai. Não foi usada a base instalada como objeto de consulta.

- 24 suítes de scope, Union, claims, documentação, tradição e workflow: **157 testes passaram**.
- 14 suítes de armazenamento, Cypher, mutation guard, supervisor, segurança, adapters e ativação: **530 testes passaram, 2 foram ignorados**, exit code 0.
- Total destas duas execuções: **687 passaram, 2 ignorados**; nenhum teste falhou. Esse total não é uma métrica de cobertura dos 89 requisitos funcionais ou dos 18 requisitos não funcionais.
- Os arquivos de testes de anchors, recall e parsing de federação foram lidos; isso não equivale a executar cenários de integração. Sua inclusão na lista de fontes do Makefile também não demonstra que cada função SUITE esteja registrada no runner.
- Não executados: campanha adversarial acima, stdio E2E completo, TSan, benchmarks, cobertura percentual, todos os contratos por linguagem e sessões reais dos hosts.

Os resultados positivos indicam regressões cobertas pelos testes existentes. Não encerram os achados que exigem outros oráculos.
