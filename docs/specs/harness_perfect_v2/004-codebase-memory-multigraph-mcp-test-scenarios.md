# Test Scenarios — codebase-memory-multigraph-mcp

**Domain:** `harness_perfect_v2`  
**Project:** `codebase-memory-multigraph-mcp`  
**Framework:** harness C11 existente, ASan/UBSan, TSan e E2E Python/unittest por MCP stdio  
**Date:** 2026-10-09

## 1. Base dos cenários e fixtures

Esta especificação deriva dos agregados, VOs, serviços e repositórios de [003 — Tactical Design](003-codebase-memory-multigraph-mcp-tactical-design.md), usando os termos de [001](001-problem-space.md) e os relacionamentos de [002](002-context-map.md). Os cenários ainda não são testes implementados nem resultados de execução. Testes Unitários isolam políticas sem I/O; Integração usa SQLite/Tree-sitter/fence reais; Funcionais usam as quatro ferramentas MCP.

| Fixture | Conteúdo e finalidade |
|---|---|
| BIFOCAL-80 | Um alvo com whitespace/CRLF preserváveis e dez dependências de corpos extensos/contratos pequenos; baseline integral com mesmos metadados; byte sizes calculados, não hardcoded. |
| EFFECTS-CLOSED | Funções puras fechadas, escrita SQL, lock, transação e emissão de evento reconhecidas. |
| EFFECTS-UNKNOWN | Despacho dinâmico, chamada externa desconhecida, macro não expandida, parsing parcial e ciclo com saída desconhecida. |
| SEAM-BRIDGE | Produtor Kafka e bridge explícito até tarefa Celery, com cluster/namespace/tópico, endpoints e proveniência. |
| SEAM-DISCONNECTED | Nomes semelhantes, tópicos distintos ou mesmo tópico em clusters distintos, sem bridge compatível. |
| RULES-CADENCE | Candidata, aprovada NORMATIVE e aprovada CONSULTED, com relógio injetável antes/no/depois do prazo. |
| WAL-PROJECT | Banco de grafo substituível, `governance.db` em disco temporário separado, conexões/processos independentes, fence e barreiras controladas. |

Oráculos usam bytes decodificados, JSON canônico, hashes, estados de domínio, contagem/conteúdo de linhas e eventos/recibos observáveis. Concorrência usa barreiras/failpoints em posições definidas; não depender de sleeps para criar a corrida. Cada teste recebe diretório/banco isolado e destrói somente seus próprios recursos.

## 2. Testes Unitários

### U01 — Should manter identidade quando a ordem de inserção JSON variar

**Given:** dois Bundles de Evidência equivalentes, com chaves, alvos, domínios e cápsulas inseridos em ordens diferentes.  
**When:** o serializer v2 ordena conjuntos e canonicaliza ambos.  
**Then:** os bytes UTF-8, `bundle_hash` e `snapshot_id` são iguais; conteúdo sequencial declarado preserva sua ordem.  
**Rastreio:** 003 §2.2–2.3; tarefa 03.

### U02 — Should preservar fonte integral quando houver escaping e caracteres especiais

**Given:** Alvo de Mutação com aspas, barras, Unicode, CRLF, comentários e whitespace significativos; variante com bytes não UTF-8.  
**When:** a fonte é serializada e decodificada, usando base64 na variante não UTF-8.  
**Then:** bytes e comprimento originais são idênticos, `source_hash` é preservado e nenhum truncamento/normalização ocorre.  
**Rastreio:** 003 §1.2 e §2.2; tarefas 03/10.

### U03 — Should alterar snapshot quando qualquer componente canônico mudar

**Given:** tupla válida com project_id, graph_generation, coverage_revision, rules_revision e extractor_version.  
**When:** cada componente é alterado isoladamente e o snapshot é recalculado.  
**Then:** cada variante produz outro `snapshot_id`; concatenações ambíguas não colidem por construção do objeto canônico.  
**Rastreio:** 003 §2.3; tarefa 03.

### U04 — Should recusar VO quando identidade ou intervalo for inválido

**Given:** casos parametrizados de hash curto/não hexadecimal, ID vazio, offset negativo, extensão fora dos bytes disponíveis e soma com overflow.  
**When:** os VOs são construídos.  
**Then:** cada entrada é recusada com código de validação e nenhuma instância parcial válida é devolvida.  
**Rastreio:** 003 §1.2 e §2.1; tarefa 01.

### U05 — Should classificar present quando efeito conhecido tiver evidência

**Given:** ASTs de EFFECTS-CLOSED com escrita SQL, lock, transação e emissão de evento.  
**When:** cada categoria da Impressão de Efeitos é calculada.  
**Then:** a categoria correspondente é `present`, com reason e evidence_ref válidos; transaction_scope conserva details quando aplicável.  
**Rastreio:** 003 §2.4; tarefa 04.

### U06 — Should classificar absent quando análise fechada comprovar ausência

**Given:** AST completo de função suportada e cadeia fechada sem efeito da categoria.  
**When:** a Impressão de Efeitos é calculada.  
**Then:** a categoria é `absent` com motivo de análise fechada; o caso não se apoia apenas na falta de primitivas locais.  
**Rastreio:** 003 §2.4; tarefa 04.

### U07 — Should classificar unknown quando resolução for incompleta

**Given:** cada caso de EFFECTS-UNKNOWN e nenhuma evidência positiva da categoria.  
**When:** a Impressão de Efeitos é derivada.  
**Then:** o efeito é `unknown` com motivo específico; parsing incompleto, macros e despacho dinâmico jamais viram `absent`.  
**Rastreio:** 003 §2.4; tarefa 04.

### U08 — Should combinar efeitos conservadoramente quando houver múltiplas contribuições

**Given:** conjuntos `present+unknown`, `absent+unknown`, `absent+absent` e ciclo com saída desconhecida.  
**When:** a agregação atinge seu ponto fixo.  
**Then:** resultados são respectivamente `present`, `unknown`, `absent` apenas com fechamento e `unknown`; a execução termina sem recursão infinita.  
**Rastreio:** 003 §2.4; tarefa 04.

### U09 — Should recusar impressão de efeitos quando contrato estruturado estiver incompleto

**Given:** campo obrigatório ausente, estado desconhecido do protocolo, reason vazio ou `present` sem evidence_ref.  
**When:** a entrada estruturada é validada.  
**Then:** cada caso é inválido; não existe conversão implícita para `absent` nem elegibilidade permissiva.  
**Rastreio:** 003 §2.1; tarefas 01/04.

### U10 — Should interpretar cadência quando intervalo inteiro positivo for fornecido

**Given:** intervalos 1 e 2592000 e relógio fixo com soma representável.  
**When:** o VO de Cadência de Revisão é criado e o prazo é calculado na revisão humana.  
**Then:** intervalos são preservados e review_due_at é reviewed_at mais interval_seconds; omissão usa 2592000.  
**Rastreio:** 003 §2.1 e §3.1; tarefas 01/02.

### U11 — Should recusar cadência quando parsing ou soma for inválido

**Given:** zero, negativo, fração, string `30d`, texto com espaços e número além de int64; adicionalmente reviewed_at cuja soma excede int64.  
**When:** o parser/VO calcula a cadência.  
**Then:** todos são recusados, sem wraparound, coerção ou prazo passado silencioso.  
**Rastreio:** 003 §2.1; tarefa 01.

### U12 — Should iniciar candidata quando proposta válida for criada

**Given:** proposta com ID, domínio, norma, força e snapshot_hash válidos, sem capacidade humana.  
**When:** Propor Regra de Tradição é aplicado.  
**Then:** estado é `UNVERIFIED_CANDIDATE`, reviewed_at é 0 e a proposta não integra as normas aplicáveis.  
**Rastreio:** 003 §3.1; tarefa 02.

### U13 — Should promover regra quando operador humano confiável revisar candidata

**Given:** candidata válida e capacidade humana autenticada pelo adaptador de operador.  
**When:** Aprovar Regra de Tradição é aplicado com evidência de revisão.  
**Then:** estado é `APPROVED`, reviewed_at/prazo são definidos e a revisão inclui identidade do operador.  
**Rastreio:** 003 §3.1; tarefas 02/08.

### U14 — Should bloquear promoção quando agente alegar autoridade humana

**Given:** candidata e entrada de agente com alegação textual ou booleano `human=true`, sem capacidade confiável.  
**When:** promoção é tentada.  
**Then:** `HUMAN_APPROVAL_REQUIRED` impede transição; a candidata e sua revisão permanecem inalteradas.  
**Rastreio:** 003 §3.1; tarefa 02.

### U15 — Should manter força normativa quando cadência vencer

**Given:** regras APPROVED NORMATIVE e CONSULTED com review_due_at=T.  
**When:** consulta é feita em T−1, T e T+1.  
**Then:** estado efetivo é APPROVED antes e REVIEW_PENDING no/depois do prazo, sem apagar regras; NORMATIVE permanece normativa e CONSULTED permanece consultada.  
**Rastreio:** 003 §2.3 e §3.1; tarefas 02/03.

### U16 — Should mudar revisão efetiva quando prazo cruzar sem escrita

**Given:** catálogo persistido inalterado e snapshot calculado antes do prazo.  
**When:** regras são consultadas no prazo com relógio injetado.  
**Then:** rules_revision efetiva e snapshot_id mudam, sem exigir UPDATE durante consulta; recuo detectado do relógio produz CLOCK_UNCERTAIN.  
**Rastreio:** 003 §2.3; tarefa 03.

### U17 — Should recusar manifesto quando veredito tiver bloqueios ou score não finito

**Given:** um veredito com blocking_violations não vazio e variantes com NaN/Infinity no score.  
**When:** Manifesto de Admissão é construído.  
**Then:** construção é recusada; score alto não neutraliza bloqueios e nenhum limiar arbitrário é aplicado a um score finito sem bloqueios.  
**Rastreio:** 003 §3.3; tarefas 02/11.

### U18 — Should retornar decisão fail-closed quando condição estrutural falhar

**Given:** bundle e mudança válidos do caso F0 abaixo.  
**When:** cada variante F1–F5 e a variante combinada FC são avaliadas.  
**Then:** os resultados seguem exatamente a matriz; motivos são únicos, ordenados e seus detalhes apontam a evidência.  
**Rastreio:** 003 §3.2; tarefa 06.

| Caso | Única alteração sobre F0 | eligible | reason_codes esperados |
|---|---|---|---|
| F0 | Coverage clean/completa, todos os efeitos conhecidos, dependências resolvidas, mudança não crítica, seams vazio | true | `[]` |
| F1 | Coverage partial; repetir com missed e truncamento | false | `COVERAGE_NOT_CLEAN` |
| F2 | Um efeito unknown; repetir em alvo e em cápsula | false | `UNKNOWN_EFFECT` |
| F3 | Uma dependência não resolvida | false | `UNRESOLVED_DEPENDENCY` |
| F4 | Mudança crítica; repetir macros, autenticação, transações e schemas | false | `CRITICAL_CHANGE` |
| F5 | Um seam tipado | false | `SEAM_PRESENT` |
| FC | Todas as cinco condições impeditivas | false | Os cinco códigos, sem duplicatas |

Nas variantes isoladas, fixtures de DTOs separam condições para testar a política; na integração real, lacunas podem produzir simultaneamente unknown e dependências não resolvidas, e todos os códigos devem aparecer.

### U19 — Should recusar elegibilidade quando descritor de mudança faltar ou divergir

**Given:** bundle válido com change ausente, patch não interpretável ou target adicional fora do scope.  
**When:** Avaliar Fast-Path valida o pedido.  
**Then:** CHANGE_EVIDENCE_REQUIRED ou CHANGE_SCOPE_MISMATCH é retornado, sem `eligible=true`.  
**Rastreio:** 003 §2.1 e §3.2–3.3; tarefa 06.

## 3. Testes de Integração

### I01 — Should reduzir payload em pelo menos 80% quando fixture favorável for extraída

**Given:** BIFOCAL-80 indexada e baseline integral canônica, com mesmo alvo e metadados.  
**When:** Extrair Bundle de Evidência constrói o JSON bifocal com Tree-sitter real.  
**Then:** `1 - bifocal_bytes/full_source_bytes >= 0.80` para bytes JSON UTF-8 completos; registrar ambos os tamanhos e projeção de código; alvo permanece byte a byte idêntico; dez dependências viram cápsulas.  
**Rastreio:** 003 §5.4; INV-01; tarefa 10.

### I02 — Should refletir lacuna quando parsing ou paginação da fronteira for incompleta

**Given:** dependência com parsing parcial, fronteira com múltiplas páginas e variante que atinge limite de recurso.  
**When:** a extração percorre a fronteira.  
**Then:** páginas completas são consumidas no caso sem limite; casos incompletos declaram ranges/reasons, scope_complete=false quando truncados e efeitos unknown, impedindo Fast-Path.  
**Rastreio:** 003 §2.2–2.4; tarefas 04/10.

### I03 — Should recusar captura quando fonte do alvo divergir durante leitura

**Given:** geração pinada e failpoint entre leitura inicial e verificação final de hash do alvo.  
**When:** outro participante altera os bytes do alvo nesse intervalo.  
**Then:** SOURCE_DRIFT recusa o bundle como evidência válida; nenhum código parcial é publicado nem manifesto gravado.  
**Rastreio:** 003 §1.2, §2.2 e §5.3; tarefa 10.

### I04 — Should produzir hipótese tipada quando bridge Kafka para Celery tiver proveniência

**Given:** SEAM-BRIDGE na geração fixada.  
**When:** Projeção de Costuras correlaciona os sinais reconhecidos.  
**Then:** seam async_message_bridge contém source/destination refs, chave qualificada, versão e referências verificáveis, com validation_status=unverified_hypothesis; Fast-Path é bloqueado por SEAM_PRESENT.  
**Rastreio:** 003 §2.4 e §3.2; tarefa 05.

### I05 — Should manter seams vazio quando tópicos estiverem desconectados

**Given:** SEAM-DISCONNECTED, inclusive nomes iguais em clusters distintos sem bridge.  
**When:** a mesma projeção é executada.  
**Then:** nenhuma costura é criada por coincidência textual; não ocorre conexão runtime com broker/worker.  
**Rastreio:** 003 §2.4; tarefa 05.

### I06 — Should recusar Seam Map quando referência ou proveniência for adulterada

**Given:** seam com endpoint ausente, hash de proveniência divergente, ID duplicado conflitante ou status de confirmação não previsto em v2.  
**When:** o mapa é validado contra o snapshot.  
**Then:** cada variante retorna SEAM_INTEGRITY_ERROR; duplicatas idênticas legítimas são deduplicadas deterministicamente sem aceitar conflitos.  
**Rastreio:** 003 §2.4; tarefa 05.

### I07 — Should verificar FULL e WAL quando conexão durável for aberta ou reaberta

**Given:** WAL-PROJECT e pool com perfil de governança.  
**When:** conexão é adquirida, devolvida e reaberta.  
**Then:** PRAGMA journal_mode retorna wal e synchronous corresponde a FULL; falha de aplicação/verificação do perfil causa GOVERNANCE_UNAVAILABLE, sem fallback NORMAL.  
**Rastreio:** 003 §5.1; tarefa 07.

### I08 — Should permitir leitura concorrente quando writer de governança estiver ativo

**Given:** conexão leitora com snapshot SQLite anterior e segunda conexão com BEGIN IMMEDIATE.  
**When:** writer insere e commita um manifesto enquanto leitor continua sua leitura.  
**Then:** leitor observa snapshot coerente anterior, writer conclui e nova transação leitora observa a linha completa; não há linha parcial nem lock de escrita mantido pela travessia.  
**Rastreio:** 003 §5.1–5.3; tarefa 07.

### I09 — Should serializar writers quando dois processos registrarem admissões

**Given:** dois processos independentes, conexões próprias, mesma geração e dois manifest_id distintos, coordenados por barreira.  
**When:** ambos tentam Registrar Manifesto de Admissão.  
**Then:** commits são serializados pela fence/SQLite; com retry limitado ambos persistem uma vez, ou um recebe GOVERNANCE_BUSY sem linha; nunca há perda, duplicação ou corrupção.  
**Rastreio:** 003 §5.2–5.3; tarefas 09/11/14.

### I10 — Should fazer rollback quando snapshot avançar antes de admissão

**Given:** bundle selado em S0 e pausa antes de adquirir fence de registro.  
**When:** publicador estabiliza S1 e registro de S0 prossegue com BEGIN IMMEDIATE.  
**Then:** STALE_SNAPSHOT é retornado; zero linhas para o novo manifesto, nenhuma revisão colateral e conexão pode iniciar nova transação. Repetir com mudança só de cobertura, regras e versão do extrator.  
**Rastreio:** 003 §2.3 e §5.3; INV-08; tarefa 11.

### I11 — Should impedir swap quando admissão estiver entre comparação e commit

**Given:** processo de registro segura fence exclusiva e passou pela comparação de snapshot; publicador concorrente está na barreira anterior ao swap.  
**When:** registrar commita e libera a fence.  
**Then:** publicador só substitui o grafo depois do commit; manifesto corresponde ao snapshot da linearização, sem swap oculto entre validação e inserção.  
**Rastreio:** 003 §5.3; tarefa 09.

### I12 — Should abortar admissão quando revisão normativa vencer durante transação

**Given:** bundle antes do prazo e relógio injetável que cruza review_due_at após a primeira revalidação, antes do commit.  
**When:** o registrador verifica novamente a revisão efetiva.  
**Then:** snapshot divergente causa STALE_SNAPSHOT e rollback; regra vencida continua normativa, não desaparece para permitir admissão.  
**Rastreio:** 003 §2.3 e §5.3; tarefas 03/11.

### I13 — Should recuperar publicação quando processo cair entre swap e metadados

**Given:** publication_status=PUBLISHING comprometido e geração preparada identificável; variantes de crash antes e depois da substituição do arquivo.  
**When:** outro processo tenta extrair/admitir e o recuperador é iniciado sob fence.  
**Then:** pedidos são recusados até recuperação; marcador do arquivo define a geração verificável estabilizada; não se admite usando metadados antigos com arquivo novo.  
**Rastreio:** 003 §5.3; tarefa 09.

### I14 — Should rejeitar violações bloqueantes quando registro for solicitado

**Given:** snapshot atual, score finito alto e blocking_violations não vazio.  
**When:** Registrar Manifesto de Admissão é chamado.  
**Then:** BLOCKING_VIOLATIONS e zero linhas novas; tentativa direta de inserir array não vazio ou JSON inválido também falha pelas constraints.  
**Rastreio:** 003 §3.3 e §5.1; tarefa 11.

### I15 — Should preservar idempotência quando resposta se perder após commit

**Given:** manifesto comprometido e falha de transporte antes de entregar o recibo.  
**When:** mesmo manifest_id e conteúdo são reenviados.  
**Then:** recibo original, admitted_at original e idempotent=true são retornados; uma única linha existe; conteúdo divergente retorna MANIFEST_CONFLICT sem overwrite.  
**Rastreio:** 003 §5.3; tarefa 11.

### I16 — Should desfazer efeitos parciais quando inserção ou commit falhar

**Given:** failpoints em INSERT e COMMIT de novo manifesto, e na gravação de regra/revisão/auditoria.  
**When:** persistência encontra erro SQLite ou falha injetada.  
**Then:** não retorna sucesso, executa rollback, não deixa manifesto/regra/audit trail parcial e libera lease/fence; commit já concluído é distinguido por consulta idempotente na recuperação.  
**Rastreio:** 003 §5.2–5.3; tarefas 08/11.

### I17 — Should persistir revisão humana atomicamente quando regra for aprovada

**Given:** candidata persistida e porta de operador autenticada.  
**When:** revisão aprova a regra.  
**Then:** estado, prazo, snapshot_hash, identidade no audit trail e incremento de rules_revision são comprometidos juntos; bundle posterior inclui a regra aplicável.  
**Rastreio:** 003 §3.1 e §5.1–5.3; tarefa 08.

### I18 — Should conservar regra revisada quando proposta antiga for repetida

**Given:** proposta candidata já aprovada por humano e retry idêntico da proposta original.  
**When:** Propor Regra de Tradição recebe o mesmo rule_id.  
**Then:** não rebaixa estado, prazo ou revisão; ID com conteúdo diferente retorna RULE_CONFLICT; nunca sobrescreve norma aprovada.  
**Rastreio:** 003 §3.3; tarefa 08.

### I19 — Should preservar governança quando grafo for reconstruído ou reindexado

**Given:** regras candidatas/aprovadas/pendentes, reviews e manifestos comprometidos; digests de linhas ordenadas registrados.  
**When:** rebuild completo, reindexação incremental e swap de geração ocorrem; repetir com cancelamento e falha de rename.  
**Then:** IDs, textos, estados persistidos, timestamps, hashes e audit trail permanecem idênticos; só metadados de publicação mudam quando apropriado; governance.db e seus sidecars não são alvo do cleanup do grafo.  
**Rastreio:** 003 §5.1 e §5.3; INV-09; tarefa 14.

### I20 — Should sobreviver reinicialização quando commit durável tiver sido confirmado

**Given:** processo gravou regras e manifesto com commit FULL confirmado.  
**When:** processo é encerrado abruptamente e banco é reaberto por novo processo.  
**Then:** registros confirmados existem com conteúdo íntegro, registros não comprometidos não aparecem e verificação SQLite não acusa corrupção; caso não equivale a prova de tolerância a hardware defeituoso.  
**Rastreio:** 003 §5.1; tarefa 14.

### I21 — Should impedir alteração do ledger quando cliente tentar update ou delete

**Given:** manifesto comprometido.  
**When:** UPDATE, DELETE ou INSERT OR REPLACE tenta mudar sua identidade/conteúdo.  
**Then:** triggers/constraints recusam a mutação e linha original permanece íntegra; aplicação nunca usa replace para retry.  
**Rastreio:** 003 §5.1–5.3; tarefas 07/11.

### I22 — Should manter conteúdo literal quando parâmetros contiverem texto SQL

**Given:** proposta cujo norm_text contém aspas e trecho semelhante a SQL.  
**When:** repository usa prepared statement e bindings para gravá-la.  
**Then:** texto é preservado literalmente e nenhuma instrução extra executa; IDs/caminhos inválidos continuam sujeitos à validação de domínio.  
**Rastreio:** 003 §5.2; tarefas 07/08.

## 4. Testes Funcionais MCP

### F01 — Should anunciar quatro contratos quando cliente listar ferramentas

**Given:** servidor com adapters Harness v2 registrados.  
**When:** cliente solicita tools/list.  
**Then:** as quatro ferramentas exatas do ADR-004 aparecem, com inputSchema estrito e limites; nenhuma delas oferece promoção humana.  
**Rastreio:** 003 §3.3; tarefas 12/13.

### F02 — Should registrar admissão quando fluxo válido concluir sem bloqueios

**Given:** projeto estável, regra aprovada aplicável e mudança não crítica F0; integrador JEV confiável.  
**When:** cliente extrai bundle, avalia Fast-Path e registra veredito sem bloqueios com hashes correspondentes.  
**Then:** eligible=true tem motivos vazios, manifesto é persistido e recibo é retornado após commit; nenhuma mutação de repositório é executada pelo endpoint.  
**Rastreio:** 003 §3.3 e §5.3; tarefas 12/13.

### F03 — Should impedir falsa concessão quando cliente adulterar bundle ou mudança

**Given:** bundle original com seam/unknown ou mudança crítica.  
**When:** cliente remove seam, troca unknown por absent, declara coverage clean ou troca patch/change_hash e solicita avaliação/registro com fast_path_granted=true.  
**Then:** recomposição/hash detecta adulteração ou política local recusa FAST_PATH_NOT_ELIGIBLE; nenhum manifesto com concessão falsa é inserido.  
**Rastreio:** 003 §3.2–3.3; tarefas 12/13.

### F04 — Should encaminhar análise completa quando Fast-Path estiver bloqueado

**Given:** cada variante real das cinco condições impeditivas.  
**When:** mcp_cbm_evaluate_fast_path é chamado.  
**Then:** eligible=false e motivos/detalhes estruturados são retornados; não há manifesto nem execução automática; JEV pode julgar depois e registrar manifesto sem concessão Fast-Path se não houver bloqueios normativos.  
**Rastreio:** 003 §3.2–3.3; tarefa 12.

### F05 — Should persistir apenas candidata quando agente propor regra

**Given:** agente autorizado a propor norma, sem capacidade humana.  
**When:** mcp_cbm_tradition_propose recebe pedido válido; variantes acrescentam status=APPROVED ou human=true.  
**Then:** pedido válido retorna UNVERIFIED_CANDIDATE; variantes com campos proibidos são recusadas sem promoção ou alteração de regra existente.  
**Rastreio:** 003 §3.1–3.3; tarefa 13.

### F06 — Should rejeitar entrada inválida quando schema ou limites forem violados

**Given:** JSON malformado, chaves duplicadas, versão desconhecida, path fora do root, target vazio, k<0/k>8, arrays ou textos além dos limites.  
**When:** cada endpoint recebe a entrada correspondente.  
**Then:** isError=true com código tipado é retornado antes de persistência; não há crash, truncamento silencioso ou elegibilidade verdadeira.  
**Rastreio:** 003 §2.1 e §3.3; tarefas 12/13.

### F07 — Should preservar leitura passiva quando bundle e Fast-Path forem consultados

**Given:** servidor com contadores de sessão, ledger, revisões persistidas e recursos conhecidos; relógio válido.  
**When:** extração e avaliação são repetidas, inclusive no vencimento de regra.  
**Then:** não criam sessões nem debitam ledger/escrevem regras; revisão efetiva pode mudar por tempo; AST/JSON/leases/arenas são liberados ao final.  
**Rastreio:** 003 §1.2, §2.3 e §3.3; tarefa 12.

### F08 — Should recusar autoridade inválida quando canal de governança não for confiável

**Given:** chamador sem autorização do host para registrar veredito ou sem capacidade humana para revisão.  
**When:** tenta mutação de governança e alega identidade no JSON.  
**Then:** controles do host recusam a ação, nenhuma linha/revisão é alterada e autodeclaração não autentica o ator.  
**Rastreio:** 003 §3.1–3.3; tarefas 08/13.

### F09 — Should liberar recursos quando cancelamento ou OOM interromper fluxo

**Given:** failpoints de alocação/Tree-sitter/yyjson e cancelamento na extração ou registro.  
**When:** cada falha é acionada.  
**Then:** falha estruturada sem resposta permissiva, sem leak/use-after-free/double-free, sem lock retido; gravação não comprometida faz rollback e requisição posterior funciona.  
**Rastreio:** 003 §1.2 e §5.3; tarefas 03/10/11/14.

## 5. Plano de execução TDD e evidência de aceite

| Etapa | Cenários | Dependência real | Evidência exigida |
|---|---|---|---|
| Red/Green de VOs e políticas | U01–U19 | Nenhuma para políticas; ASTs controladas nos extratores | Falha inicial relevante, aprovação posterior e nomes dos testes. |
| Repositories e extratores | I01–I08, I14–I18, I21–I22 | Tree-sitter e SQLite embarcados | Bytes/hashes, PRAGMAs, constraints e consultas de linhas. |
| Corridas e publicação | I09–I13 | Processos/conexões reais e fence | Ordem das barreiras, snapshot antes/depois e rollback integral. |
| Durabilidade | I19–I20 | Pipeline real, disco e reinicialização | Digests de conteúdo antes/depois, recibos de commit e integridade SQLite. |
| Contrato funcional | F01–F09 | MCP stdio real | Pedidos/respostas, códigos e efeitos persistidos observáveis. |

Integrar suites propostas ao runner existente conforme tarefa 14. Comandos herdados de [TESTS.md](../../adr/TESTS.md): `make -f Makefile.cbm test-foundation`, `make -f Makefile.cbm test`, `make -f Makefile.cbm test-tsan`, `make -f Makefile.cbm test-union-workflow` e `python3 tests/e2e/run_e2e.py`. Suites novas só podem ser invocadas por nome depois de registradas; não presumir comando novo já existente.

Executar ASan/UBSan para a mudança e TSan para coordenação em ambiente suportado; o processo de testes deve usar a plataforma de build disponível no projeto. Cobertura de referência: Core e Admission 90%, Query e Gateway 85%, global 80%, conforme protocolo do projeto; estes percentuais são cobertura de testes, distintos dos 80% de redução de payload. Falha nova requer correção e repetição dos cenários afetados.

Não se especificam testes de exclusão CRUD para manifesto/regra porque não há deleção de negócio nesta evolução. Não se fazem chamadas runtime Kafka/Celery, pois esses sistemas participam como evidência de hipóteses. Escala/limiar do score JEV, algoritmo de julgamento e UI de operador não são inventados aqui; validam-se apenas contratos e autoridade definidos em 003.

Aceite final exige evidência observável dos seis critérios do pedido: redução bifocal, integridade de seams, matriz fail-closed, concorrência WAL, stale snapshot com rollback e preservação de governança após rebuild. Aprovar estes testes não concede promoção humana a regras nem dispensa os gates de execução do ADR-003.
