# Revisão e plano de testes do Plano Fractal

Data: 2026-10-03. Referência: `8fe4517e8231780cb22150c5643cad942f51e93c`.
Estado: R01–R05 corrigidos e regressões focadas aprovadas em 2026-10-03; campanha completa T01–T38 continua **pendente**.

## 1. Conceito central obtido antes da revisão

O Plano Fractal aplica **o mesmo ciclo de criação governada em cada escala**: enunciado, tarefa, feature, projeto e produto. A estrutura permanece; variam duração e orçamento. A V2 une criação e memória: uma execução bem-sucedida precisa ser verificada e depois admitida para adquirir autoridade. Memória é conhecimento com jurisdição, proveniência, contestação, história e recall, não apenas armazenamento.

As dez estações são concepção, interrogação, diferenciação, sacrifício, ressurreição, sublimação, trabalho da sombra, verificação, admissão e reflexão. No runtime, o ADR traduz esse ciclo em `INITIATE → GROUND → DELIBERATE → CONCRETIZE → CONTEST → VERIFY → ADMIT → TRACE`.

Cada invocação delimita um **temenos/horizonte**. Hipóteses permanecem propostas; dúvidas continuam abertas; decisões humanas não fabricam evidência. Autorizações têm escopo, validade temporal e uso controlado. Uma promoção atravessa uma fronteira por vez e declara suas exclusões. A evidência da verificação vem do registro observável do host, não da narrativa do agente.

A V3 distingue três territórios: realização responde **o que existe**; idealização responde **o que se pretende e por quê**; tradição temática responde **como se faz**. O projeto referencia o tema, sem absorver seu conteúdo ou ganhar direito de escrita sobre ele. Descobrir um tema não estabelece binding. Um juízo de especialidade precisa citar um nó de uma versão identificada ou declarar invenção. A tradição também está sujeita às mesmas regras de admissão, contestação e correção.

Consequências para o teste: **persistência não prova aprovação; round trip não prova completude semântica; metadados de versão não provam a identidade do conteúdo; uma identidade declarada não prova autoridade humana**.

Fontes conceituais:

- [PRD V1, §6 e FR-9](../PRD/novos-paradgimas/PRD_V1.md): invariância do ciclo em escala.
- [PRD V2, §§1–4 e §6](../PRD/novos-paradgimas/PRD_V2.md): criação/memória, verificação/admissão, sujeito delimitado e testemunho.
- [ADR V1, §§2–3](../PRD/novos-paradgimas/ADR_V1.md): protocolo de invocação e comunicação por claims.
- [PRD V3, §§2–5 e FR-21–FR-28](../PRD/novos-paradgimas/PRD_V3.md): tradição, binding, proveniência e invariância em temas.

Os documentos distinguem baseline, evolução e questões abertas; suas propostas não constituem prova de implementação concluída.

## 2. Escopo e método

Revisar diretamente fontes e histórico Git, conforme a instrução de não consultar o CBM do ambiente. Nenhuma consulta ao grafo instalado foi realizada nesta revisão.

Escopo principal: commits `8fe4517e` e `9a45db4d`, com dependências atuais dos fluxos Union. Features: catálogo temático, busca/travessia, proveniência, relações Markdown, horizonte Fractal, transporte integral de documentos e recuperação entre processos. Incluir binding, deriva e fundação quando necessários para confrontar essas features com FR-21–FR-28.

Foram lidos handlers MCP, registro temático, binding, routing, deriva, relações de pipeline, catálogo de horizontes, sincronização, construtor de envelope e testes/relatórios correspondentes. Esta é uma revisão delimitada, não auditoria exaustiva do repositório. Após a revisão, foram aplicadas correções TDD para R01–R05 e executadas as quatro suítes focadas; isso não confirma os resultados históricos no binário instalado.

## 3. Achados de código

Prioridades: P1 = quebra relevante de autoridade/proveniência; P2 = perda de estado ou comportamento incorreto em condição específica. Os passos abaixo são reproduções propostas a partir do fluxo de código, não resultados de testes executados nesta revisão.

### R01 — P1: binding pode simular aprovação humana

Evidência: [handler](../../src/mcp/union_handler.c), linhas 1466–1523; [validação](../../src/union/union_binding.c), linhas 35–50.

`handle_binding_claim` copia `validated_by` diretamente do pedido e admite a claim sem autenticar credencial do operador. O núcleo rejeita strings contendo `agent`, mas aceita `operator`, `human_operator` ou `AGENT`. Não verifica o tema/versão no catálogo nesse caminho.

Reprodução: um cliente agente envia um binding `NORMATIVE` com `validated_by="operator"`, tema e versão preenchidos, sem autorização do host. O fluxo retorna `admitted=true`. Isso viola a distinção entre identidade declarada e autoridade intencional em FR-22.

Teste existente: [test_union_binding.c](../../tests/test_union_binding.c), `test_binding_operator_normative_admitted`; [workflow](../../tests/test_union_workflow_e2e.c), `test_w05_territory_workflow`. Os testes positivos aceitam precisamente uma string de operador. Eles não demonstram autenticidade. Priorizar T08–T10.

### R02 — P1: pin de catálogo não fixa conteúdo do grafo

Evidência: [registro](../../src/union/union_theme_registry.c), linhas 267–284; [leitura temática](../../src/mcp/union_handler.c), linhas 1347–1455; resolução de citação, linhas 945–990.

A imutabilidade compara os campos do registro, incluindo `target_uri`. A leitura resolve esse URI para um projeto e consulta seu índice atual. Nenhuma geração/hash/snapshot da versão é encaminhada à busca. É permitido cadastrar versões distintas apontando para o mesmo projeto mutável.

Reprodução: cadastrar V1 apontando para projeto P; alterar o texto de um tópico mantendo seu URI e reindexar P; consultar e validar a citação com V1. A leitura alcança o conteúdo novo e pode retornar `anchor_verified=true` para V1. Uma versão fixada deixa de identificar a regra que fundamentou a decisão original.

Teste existente: [test_theme_versions_are_immutable_and_pinnable](../../tests/test_union_theme_registry.c) compara descrições do catálogo; não modifica o grafo de destino. O E2E temático trabalha com uma versão do corpus. Priorizar T06.

### R03 — P2: bindings desaparecem ao recriar o servidor

Evidência: [inicialização MCP](../../src/mcp/mcp.c), linhas 2029–2038; [ledger](../../src/union/union_binding.c), linhas 53–72.

O catálogo é carregado de disco, mas o ledger de bindings é inicializado vazio. A admissão insere em um array do servidor; esse caminho não grava a claim no plano persistente de idealização.

Reprodução: admitir binding; destruir e recriar o servidor sobre o mesmo cache; consultar o novo ledger. A relação normativa não estará presente. Os testes de reabertura do catálogo e de retenção de horizonte não cobrem esse estado. Priorizar T11.

### R04 — P2: dois escritores do catálogo podem perder registros

Evidência: [theme_registry_save/open/register](../../src/union/union_theme_registry.c), linhas 35–78, 85–140 e 301–314.

Cada instância mantém uma cópia em memória e salva o array inteiro no mesmo arquivo, usando também o mesmo nome `.tmp`. Não há releitura/merge ou coordenação de escritores nesse caminho.

Reprodução determinística: A e B abrem catálogo vazio; A registra X; B, ainda com snapshot vazio, registra Y; reabrir o arquivo resulta somente em Y. Escritas simultâneas também disputam `.tmp`. O problema requer duas instâncias escritoras sobre o mesmo arquivo; um único daemon serializado não basta para reproduzi-lo.

Teste existente: `test_theme_catalog_survives_reopen` cobre escritor seguido de leitor, não dois escritores. Priorizar T05 e registrar se o produto proíbe/coordena esse modo no transporte real.

### R05 — P2: deriva é emitida mesmo se publicar versão falhar

Evidência: [cbm_theme_publish_version](../../src/union/union_theme_drift.c), linha 31 e linhas 33–59.

O retorno de `cbm_theme_registry_register` é ignorado. O código pode marcar bindings como `DRIFT_PENDING`, emitir notices e retornar `OK` após uma recusa de capacidade, imutabilidade ou persistência.

Reprodução: preencher o catálogo até sua capacidade e pedir nova versão de um tema com binding. O registro recusa a versão, mas a função continua anunciando a deriva. [test_union_theme_drift.c](../../tests/test_union_theme_drift.c) cobre publicação válida e diagnósticos, sem falha nessa etapa. Priorizar T14.

## 4. Revisão dos testes existentes

| Área | Evidência no repositório | O que cobre | Limite relevante |
|---|---|---|---|
| Catálogo | `tests/test_union_theme_registry.c`, 8 casos registrados | Schema, ausência, lookup, namespace, busca/página, metadados imutáveis e reabertura | Sem dois escritores, corrupção/falha de escrita ou snapshot do conteúdo |
| Routing | `tests/test_union_routing.c` | Heurística e validação de proveniência no núcleo | Núcleo valida metadados; resolução de nó pertence ao handler |
| Workflow | `tests/test_union_workflow_e2e.c` | Handlers, claims, sessões, autorização em outros fluxos e regressão da string de busca | Grande parte chama handlers em processo; não equivale a daemon IPC/host real |
| Relações Markdown | `tests/test_pipeline.c`, `pipeline_markdown_theme_relations_are_typed_and_incremental` | Relações válidas, tipo desconhecido, alvo inexistente e remoção após edição | Segunda execução não afirma a rota usada; fallback full pode satisfazer as mesmas assertions |
| Temático MCP | `docs/temenos-tests/ui-e2e-20261002-a1/run-theme-graph-e2e.py` | Escritor/leitor, indexação temática, descoberta, travessia, citação e recuperação de fixture | Depende de snapshot/fixture; não cobre pin após mudança do corpus nem aprovação do conteúdo |
| Vida útil JSON | `docs/temenos-tests/ui-e2e-20261002-a1/theme-catalog-lifetime.c` | Regressão de use-after-free e URI/citação com resposta controlada | Harness documental separado; manter como regressão permanente da suíte |
| Retenção | `docs/temenos-tests/retention-restart-20261002/result.md` e runners | Evidência histórica de saída do dono e reinício, com reconstrução de 82 bytes | Não prova TTL, agendamento do reaper, reboot ou update |
| Reaper | `tests/e2e/reaper_harness.py` | Simulação Python de recuperação | Usa heartbeat/TTL configurável; C usa created_at/TTL fixo; não substitui execução C |
| Documento integral | `docs/temenos-tests/full-document-20261002` e `build_horizon_document.py` nas três skills | Envelope com partes, SHA-256 e preservação UTF-8 | Não foi localizada, no escopo de testes revisado, uma suíte dedicada de limites/fuzz para o builder |

O relatório temático histórico registra 8/8, 7/7, 20/20 e 278 testes de pipeline aprovados em seu ambiente. São evidências anteriores, não um PASS do build Windows instalado nesta sessão. As metas percentuais em `docs/adr/TESTS.md` são requisitos; não foi produzida medição atual de cobertura.

## 5. Ambiente e evidência exigidos

Executar contra um binário compilado do commit sob teste. Registrar commit, SHA-256, flags, SO, versões de ferramentas, IDs da execução e origem sintética das fixtures. Usar cache, runtime e workspace privados, com ACL restrita à conta de teste no Windows. Não copiar nem consultar os índices compartilhados para estes cenários; gerar pequenos repositórios sintéticos independentes.

No Windows, usar PowerShell 7, MSYS2 CLANG64/Make, Python e o binário com UI. Para suites C com ASan/UBSan/TSan, usar Linux ou WSL2 com toolchain correspondente; o ambiente Windows preparado não inclui WSL2 e o runner nativo não comprova ausência de falhas de memória.

Conservar transcript JSON-RPC, stderr, snapshots SQL de controle, hashes, diferenças de estado e códigos de recusa. O escritor pode criar a fixture; o leitor recebe somente contexto mínimo. Manter o hash esperado no controlador externo, sem fornecê-lo como conteúdo ao leitor. Uma assertion do leitor deve ser confrontada com a evidência capturada pelo controlador.

## 6. Casos prioritários: catálogo, autoridade e tradição

| ID / prioridade | Dado / quando | Resultado e oráculo esperado |
|---|---|---|
| T01 / P0 | Registrar tema válido; omitir individualmente campos obrigatórios | Registro recuperável; negativas retornam schema invalid sem alteração de arquivo |
| T02 / P0 | Tema ACTIVE, ABSENT, DEPRECATED e ID desconhecido | Ausência registrada distinta de ID desconhecido; leitura de ABSENT não inventa conteúdo |
| T03 / P1 | Buscar termos AND, aliases, filtros, empates; paginar até o fim | União das páginas coincide com conjunto esperado, sem duplicação; total/has_more corretos |
| T04 / P0 | Repetir ID/V1 com campo de conteúdo alterado; publicar V2 | V1 permanece; alteração recusada; V2 resolvível por pin e latest |
| T05 / P0 | A/B abrem mesmo catálogo; registrar X/Y alternada e simultaneamente | Ambos persistem ou há recusa explícita; nunca sucesso com perda silenciosa; cobre R04 |
| T06 / P0 | V1/V2 apontam para P; editar/reindexar tópico P preservando URI | V1 retorna conteúdo original ou recusa drift; não certifica texto novo como V1; cobre R02 |
| T07 / P0 | Corromper JSON, negar escrita/rename, interromper save | Falha tipada; catálogo anterior intacto; nenhuma confirmação falsa; retomada determinística |
| T08 / P0 | Binding normative com validated_by=operator/AGENT/agent e sem credencial | Autoridade não pode ser obtida pela string; todas tentativas sem prova recusadas; cobre R01 |
| T09 / P0 | Credencial válida, expirada, reutilizada e de outro contexto | Só autorização válida no escopo produz binding; testemunho do host identifica aprovação real |
| T10 / P0 | Binding para tema/versão inexistentes, modo inválido ou scope vazio | Falha tipada; nada admitido silenciosamente; registrar decisão de contrato para scope |
| T11 / P0 | Admitir binding/deviation; encerrar e recriar servidor e daemon | Recuperar intenção, versão e autoria sem nova decisão fabricada; cobre R03 |
| T12 / P0 | Especialidade sem fonte; invenção sem/com rationale; citação com nó ausente | Silent invention recusada; invenção explícita identificada; âncora falsa recusada |
| T13 / P1 | Citar nó de outro projeto/arquivo, fragmento modificado, URI longo; backend indisponível | Comparação de URI exata; indisponibilidade não confundida com ausência; sem truncamento aceito |
| T14 / P0 | Publicar V2 com catálogo cheio ou falha de persistência | Recusa propagada; nenhuma deriva para versão não publicada; ledger intacto; cobre R05 |
| T15 / P1 | Publicar V2 válido; bindings em V1; tema removido e desvios pendentes | Notices e órfãos identificáveis; revalidação humana explícita; nenhuma atualização automática do pin |
| T16 / P0 | Projeto tenta escrever tema; curator autorizado escreve | Barreira assimétrica preservada em transporte real, não apenas helper; conflito vira contestação |
| T17 / P0 | Propor fundação; aceitar/recusar sem/com credencial; reiniciar antes da decisão | Proposta durável e pendente; agente não funda tema nem produz aprovação; rejeição registrada |

## 7. Casos de indexação temática e API

| ID / prioridade | Dado / quando | Resultado e oráculo esperado |
|---|---|---|
| T18 / P0 | Indexar Markdown com os oito tipos permitidos | Arestas tipadas com origem/target_ref corretos; contagem conferida em SQL e MCP |
| T19 / P1 | IDs duplicados, alvo próprio/inexistente/outro arquivo, tipo inválido | Sem ligação ambígua ou entre arquivos; invalid/unresolved observáveis |
| T20 / P1 | Marker sem fechamento, duas declarações, espaços/newlines e limite de 500 bytes | Contrato de parsing explícito; nenhum resultado completo para declaração não processada; testar antes/depois do limite |
| T21 / P0 | Editar/remover relação/tópico/arquivo; executar caminho incremental | Relações obsoletas removidas; afirmar rota via seam/log; comparar grafo final com full independente |
| T22 / P1 | Reindexar sem mudanças, repetir declaração e target | Sem novas arestas duplicadas; rota no-op comprovada; estado estável |
| T23 / P0 | Chamar tools/list e tools/call pelo stdio Windows e daemon novo | Schemas/funções presentes e coerentes; erros tipados com tipos de argumento incorretos |
| T24 / P0 | Executar busca/travessia com leitura e tentar CREATE/DELETE/SET na query | Só leitura permitida; hash lógico/SQL do tema intacto após negativas |
| T25 / P1 | Resultados planos BM25 e agrupados; múltiplas páginas e zero resultados | URIs exatas em ambos formatos; distinção entre resultado vazio, erro e backend indisponível |

## 8. Casos de horizonte, documento e retenção

| ID / prioridade | Dado / quando | Resultado e oráculo esperado |
|---|---|---|
| T26 / P0 | Criar/sincronizar horizonte com projeto explícito; listar por outro projeto | Isolamento exato; projetos com prefixos semelhantes não vazam; legacy usa URI exato |
| T27 / P1 | Catálogo vazio, páginas, DB ilegível/corrompido/symlink | Paginação determinística; partial/unreadable refletem perda de leitura; não certificar completude parcial |
| T28 / P0 | Dois leitores/escritor; saída abrupta durante sync e nova tentativa | Estado transacional; sem mistura de versões; retry idempotente ou recusa explícita |
| T29 / P0 | Transportar arquivo vazio, BOM, CRLF/LF, acentos, emoji, aspas, barras e texto grande | Bytes, tamanho, SHA-256 e ordenação iguais; leitor reconstrói somente por MCP |
| T30 / P0 | Omitir/duplicar/reordenar parte; adulterar conteúdo/hash; URI duplicado | Recuperação incompleta/corrompida recusada; nunca substituir parte por resumo |
| T31 / P1 | Limites de linha JSON 479/480/481, payload 2999/3000, IDs/path longos | Aceitação/recusa condiz com parser real; fronteiras testadas nas três cópias do builder |
| T32 / P0 | Encerrar dono antes do TTL; iniciar daemon e leitor novos | Horizonte reencontrado e íntegro; dono ausente não implica aprovação nem descarte imediato |
| T33 / P0 | Produção C: dono morto, idades antes/igual/depois do TTL e WAL/SHM | Expiração conforme contrato real e logs; afirmar execução/agendamento C; simulação Python não vale como prova |
| T34 / P1 | Dono vivo além do TTL, PID reutilizado, acessos negados | Não coletar horizonte válido com evidência insuficiente; documentar limites de identidade do processo |
| T35 / P1 | Reiniciar dispositivo/aplicativo e atualizar binário com horizonte existente | Compatibilidade e recuperação íntegra ou migração/recusa explícitas; teste não altera instalação compartilhada |
| T36 / P0 | Caso Fractal integral: ideia → hipótese → dúvida → decisão → evidência → contestação → sweep | Todo objeto mantém status/autoria/proveniência; nenhum OPEN silenciosamente resolvido; trace factual |
| T37 / P0 | Promoção com contestação blocking, base stale, exclusões e orçamento esgotado | Gate recusa ou admite uma fronteira com exclusões nomeadas; exaustão nunca concede autoridade |
| T38 / P1 | Repetir fluxo em escala de tarefa, feature e tenant temático | Mesmas fronteiras/invariantes; orçamento e duração específicos; sem atalho de autoridade no tema |

P0 significa gate obrigatório desta campanha; P1 significa segunda rodada. Não confundir prioridade de teste com severidade P1/P2 dos achados.

## 9. Sequência e comandos

1. Fixar commit/hash, preparar corpus sintético e isolamento de cache/runtime com ACL privada.
2. Criar regressões R01–R05 que falhem pelo motivo correto; conservar evidência do RED.
3. Executar unidades/handlers focados, depois transporte stdio/daemon Windows.
4. Executar envelope/round trip e retenção com leitor novo; controlador externo verifica bytes e efeitos.
5. Executar suites sanitizadas no Linux/WSL2, depois TSan para concorrência relevante.
6. Executar T33 no reaper C real; concluir gates separados de TTL, restart e atualização.

Comando Windows existente, para guards gerais da plataforma (não substitui a matriz Fractal):

```powershell
pwsh -File scripts/test-windows.ps1 -GuardsOnly -Binary C:\Users\User\AppData\Local\Programs\codebase-memory-mcp\codebase-memory-mcp.exe -Make C:\msys64\usr\bin\make.exe
```

Comandos Linux/WSL2 após disponibilizar toolchain e test-runner sanitizado:

```sh
make -f Makefile.cbm test-focused TEST_SUITES="union_theme_registry union_routing union_binding union_theme_drift union_cross_territory union_founding union_workflow_e2e horizon_spec_parser" TEST_SEAMS=1
make -f Makefile.cbm test-union-workflow TEST_SEAMS=1
make -f Makefile.cbm test-tsan
```

O runner documental `run-theme-graph-e2e.py` deve receber `CBM_BINARY_PATH`, `CBM_CACHE_DIR` e runtime privado. Preparar esse cache com fixtures sintéticas; o helper histórico que copia índices compartilhados não atende ao isolamento desta campanha. O runner atualmente exige projeto-base/fixture conhecidos: adaptar o harness antes de executar, sem reutilizar dados reais do usuário.

## 10. Critérios de saída e registro

Aceitar a campanha quando todos os P0 passarem no commit avaliado, R01–R05 tiverem regressão permanente, as recusas não alterarem estado indevidamente e as evidências externas coincidirem com o retorno MCP. Falha de pré-condição ou falta de Linux/WSL2 é **BLOCKED**, não PASS.

Separar resultados por gate: comportamento Windows, autoridade, identidade de versão, integridade documental, restart, TTL e sanitizadores. Um PASS parcial não aprova os demais. Não transformar metadados sintéticos em decisões reais de produto.

Registrar por caso: ID, commit/hash, plataforma/flags, fixture, estado anterior/posterior, ações observadas, retorno esperado/obtido, transcript, stderr, duração, resultado `PASS/FAIL/BLOCKED` e limitação. Os cenários abrangentes T01–T38 permanecem **PLANNED**; os testes de regressão para R01–R05 estão registrados na seção 12.

## 11. Referências de execução e contratos

- [Protocolo de testes](../adr/TESTS.md): camadas, sanitizadores e metas de cobertura.
- [Workflow Union](../feature/union_workflow.md): descoberta temática, binding, sessions e gate de mutação.
- [Escopo D01](../PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D01-registry-graph.md): catálogo e versão.
- [Relatório temático anterior](ui-e2e-20261002-a1/theme-graph-report.md): evidências históricas e limitações.
- [Retenção anterior](retention-restart-20261002/result.md): saída do dono/restart; TTL pendente.
- [Reaper aproximado](../../tests/e2e/reaper_harness.py): distinção explícita entre simulação Python e produção C.

## 12. Correções TDD e execução focada

Foram adicionadas regressões para os achados, aplicadas as correções e executadas as suítes focadas abaixo:

```text
Windows / MSYS2 CLANG64, Clang/Clang++, ASan+UBSan, TEST_SEAMS=1
make -f Makefile.cbm BUILD_DIR=<temp>/cbm-tdd-build CC=clang CXX=clang++ \
  test-focused TEST_SUITES="union_theme_registry union_theme_drift union_binding union_workflow_e2e" TEST_SEAMS=1
Resultado: 39 passed
```

- **R01:** o handler recusa binding `NORMATIVE` sem credencial verificada pelo host; a autoria gravada vem da identidade autenticada. A regressão cobre identidade declarada, token incorreto e credencial válida. `CONSULTED` mantém validação opcional conforme D02.
- **R02:** cada versão ativa com grafo registra a geração opaca do store; busca e travessia recusam quando a geração atual diverge. A suíte cobre a imutabilidade do token no catálogo; a mutação do grafo entre publicação e leitura ainda requer o cenário de integração T06.
- **R03:** bindings e desvios são salvos em `theme_bindings.json`, lidos na abertura e revertidos em memória quando a gravação falha. A regressão abre um segundo ledger e confere recuperação; o restart completo de daemon permanece T11.
- **R04:** alterações do catálogo adquirem trava exclusiva de diretório, recarregam o snapshot atual sob a trava e só então salvam; contenção explícita retorna `THEME_PERSISTENCE_FAILED`. A regressão verifica contenção e dois snapshots obsoletos em sequência; simultaneidade multiprocesso continua incluída em T05.
- **R05:** a recusa de publicação é propagada antes de alterar bindings ou produzir notices; catálogo cheio deixa a versão e o ledger intactos.

A campanha T01–T38, os testes de transporte real stdio/daemon e TSan não foram executados nesta rodada. O binário instalado não foi consultado nem atualizado.
