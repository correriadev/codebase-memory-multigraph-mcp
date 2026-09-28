# Auditoria do relatório de evolução do CBM e das skills

Data: 2026-09-28. Revisão inspecionada: `ccc5db32`.
Escopo: confrontar o relatório anexado com código, registro MCP, testes, documentação e as skills locais citadas. Não foram realizadas correções no produto.

## Conclusão

**O relatório é parcialmente correto: há implementação substancial e exposição MCP, mas não há evidência de um workflow completo, obrigatório e governado de ponta a ponta.** Ele transforma capacidades isoladas e intenções de arquitetura em garantias operacionais que o código ainda não oferece.

O Track W avançou além do diagnóstico histórico de ferramentas inacessíveis em `AUDIT-WORKFLOW.md`: existem 12 ferramentas Union registradas e despachadas. Entretanto, persistem lacunas na ligação entre proveniência, autorização, promoção, fechamento, persistência e atuação das skills. Portanto, a conclusão “substrato completo” deve ser substituída por “substrato implementado com integração operacional parcial”.

## Método e evidência executada

- Inspeção do histórico recente, das três features abertas no IDE, dos handlers, dos módulos Union, da admissão e das specs Track B.
- Contagem na tabela `TOOLS` de `src/mcp/mcp.c`: **33 ferramentas**; em `src/union`: **22 arquivos C**; na suíte citada: **544 linhas e 7 testes**. A contagem de ferramentas é evidência de registro no código, não de disponibilidade em toda instalação/perfil.
- Execução de `build/c/test-runner.exe union_workflow_e2e`: **7 passaram, saída 0**, com cache temporário isolado. A primeira tentativa retornou `0xC0000135`; adicionar `C:\msys64\clang64\bin` ao `PATH` apenas do processo resolveu a inicialização.
- Foi utilizado o binário já existente, sem recompilação nem comprovação de correspondência exata com HEAD ou sanitizadores. O resultado não certifica build limpo, cobertura ou toda a aplicação.
- Consultadas, como objetos de auditoria, as skills em `C:\Users\corre\.gemini\config\skills`, localização citada pelo relatório. Não se infere o estado de instalações ou repositórios externos não inspecionados.
- A pasta `.codebase-memory` estava inacessível; o Git já apresentava três entradas dessa pasta como removidas antes do trabalho. Elas não foram alteradas. A análise de referências usa os arquivos-fonte acessíveis, não um índice de grafo cuja cobertura não foi verificada.

## Achados principais

### 1. Alta — Fechamento não executa o sweep nem emite o trace Union

`handle_union_session_close` chama `cbm_session_close`. Este emite um evento, descarta o horizonte e remove a sessão. Não chama `cbm_sweep_close_session`, não valida destinos de claims e não emite o trace consolidado. As buscas dos símbolos de sweep e de formatação de trace em `src` encontram suas implementações/declarações, mas não a integração alegada no fechamento.

**Impacto:** fechar com sucesso não prova tratamento de claims órfãs, colheita de invenções ou reconciliação factual do ciclo. A afirmação da feature `union_workflow.md` de que o fechamento dispara sweep não corresponde a esse caminho.

Evidências: [handler](../../src/mcp/union_handler.c), linhas 213–259; [sessão](../../src/union/union_session.c), função `cbm_session_close`; [sweep](../../src/union/union_sweep.c), função `cbm_sweep_close_session`; [trace](../../src/union/union_trace.c).

### 2. Alta — Fundação não preserva proposta nem comprova autoridade humana

`founding_propose` valida campos e devolve `WAITING_HUMAN`, sem armazenar proposta ou usar `srv`. `founding_decide` reconstrói uma proposta a partir do tema, fixa namespace/curador e não recupera justificativa nem sessão de origem. A condição de autonomia vem do próprio JSON, com padrão `false`; o handler não comprova a identidade humana do decisor.

Há ainda divergência objetiva: com `operator_accepted=false`, o core retorna sucesso sem registrar tema, mas o handler responde `decided=true`, `status=DRAFT`. Quando aceita, o core registra estado `ABSENT`, enquanto a resposta também diz `DRAFT`.

**Impacto:** a API não sustenta a garantia de fundação exclusivamente humana e rastreável. A recusa explícita de `is_agent_autonomous=true` é uma checagem declarativa, insuficiente como fronteira de autoridade.

Evidências: [handlers](../../src/mcp/union_handler.c), funções `handle_founding_propose` e `handle_founding_decide`; [core](../../src/union/union_founding.c), `cbm_founding_adjudicate`.

### 3. Alta — Bloqueio por contestação é condicional e limitado ao alvo exato

Em `handle_promote_horizon`, a ausência de sessão Union gera `union.promotion_bypass_session`; a execução continua para a admissão. A consulta de contestações fica no ramo que encontrou sessão. Além disso, `cbm_contest_is_blocked` compara `target_ref` exatamente com o ID recebido e considera apenas `BLOCKING` não resolvida.

**Impacto:** uma contestação sobre claim/nó não bloqueia automaticamente o horizonte que o contém. Horizontes sem sessão escapam desse bloqueio Union, embora continuem sujeitos às verificações da admissão. Isso contradiz a garantia ampla de que contestações ativas sempre impedem promoção.

Evidências: [promoção MCP](../../src/mcp/promote_handler.c), ramo `if (!sh)`; [contestações](../../src/union/union_contest.c), `cbm_contest_is_blocked`.

### 4. Alta — Âncora chamada AST é uma comparação textual

`cbm_ast_signature_match` lê o arquivo, procura `expected_text` ou `symbol_name` com `strstr` e calcula FNV-1a sobre uma fatia textual, com alternativa de normalização de whitespace. Essa rotina não analisa uma árvore Tree-sitter.

**Impacto:** existe verificação em dois níveis, mas não a garantia estrutural descrita no relatório. Ocorrências textuais e fatias não equivalem à identidade sintática de um símbolo. Tampouco se exige imutabilidade de todo o arquivo: uma âncora pode continuar válida após alterações fora do trecho verificado.

Evidência: [anchor_checker.c](../../src/admission/anchor_checker.c), `cbm_fast_offset_match` e `cbm_ast_signature_match`.

### 5. Alta — Proveniência e gateway são chamadas cooperativas

O classificador usa palavras-chave e padrão `CONSULTATIVE`. A validação de proveniência é um endpoint separado; não estabelece um requisito de execução para todas as mutações. Na citação canônica, o validador consulta o tema, mas não verifica a existência de `node_uri` nem a versão fixada. O gateway autoriza/registra uma declaração de ação; não intercepta escritas externas no filesystem.

Também há comportamento permissivo no handler: `effect_class` ausente ou desconhecida permanece `IDEMPOTENT`. A documentação deve distinguir disciplina esperada do cliente de imposição pelo servidor.

Evidências: [roteamento](../../src/union/union_routing.c), `cbm_classify_activity` e `cbm_validate_specialty_provenance`; [handler](../../src/mcp/union_handler.c), `handle_union_record_action`; [dispatcher](../../src/mcp/mcp.c), linhas 17519–17553.

### 6. Alta — Memória institucional continua volátil

O servidor inicializa registros de contratos, temas e bindings em memória; as inicializações zeram as estruturas. No caminho inspecionado, não há carga persistente desses registros no boot. O SQLite dos horizontes não demonstra persistência desses outros objetos.

**Impacto:** temas e vínculos registrados durante a vida do servidor não constituem, por si, tradição institucional durável entre reinícios.

Evidências: [inicialização MCP](../../src/mcp/mcp.c), linhas 1892–1895; [registro de temas](../../src/union/union_theme_registry.c); [bindings](../../src/union/union_binding.c).

### 7. Média — Testes não provam a saga completa nem revisão independente

A suíte de 544 linhas cria servidores em memória e chama `handle_*` diretamente. Verifica respostas e contadores em sete cenários. Não executa uma saga única com código de pagamento, promoção bem-sucedida, sweep, reinício e conferência de logs capturados. Emissão de logs durante o teste não equivale a assertar seu conteúdo.

`contest_verify` atribui `blind_reviser` ao submitter e aceita referências de evidência como strings; não executa um revisor independente nem verifica os artefatos referidos. A ausência de identidade do autor no julgamento é diferente de isolamento comprovado de subagentes.

Evidências: [suíte](../../tests/test_union_workflow_e2e.c); [handler](../../src/mcp/union_handler.c), `handle_contest_verify`; [core](../../src/union/union_contest.c), `cbm_contest_submit`.

### 8. Média — Integração das skills foi superestimada

Nas cópias Gemini consultadas, `adversarial-qa` e `the-grumpy-tech-lead` não instruem chamadas a `contest_verify`. `harness-tracer` descreve registros em `docs/harness-history`, não integração com `union_trace`. A busca literal também não encontrou esses símbolos nas outras skills de meta-harness citadas.

As specs B01–B05 declaram `Status: REVIEW` e implementação no **harness-kit, fora deste ciclo**. Isso confirma intenção, não instalação ou operação das novas skills. Já o limite de 8.000 caracteres e o gerador documental constam na skill `project-memory`; a existência dessas regras não prova fidelidade semântica da documentação.

Evidência versionada: [spec B01](../PRD/novos-paradgimas/specs/harnesskit-skills/SCOPE-B01-graph-grounding.md), linhas 3–4, e demais scopes B02–B05. As observações das skills locais não demonstram quando ou por que foram criadas.

### 9. Média — Reaper existe, mas agendamento automático não foi encontrado

O reaper implementa liveness por PID e TTL de 3.600 segundos. A remoção exige **PID morto e TTL excedido**, não apenas idade. A busca por `cbm_reap_orphan_horizons` em `src` encontrou declaração e implementação, sem chamador de produção.

**Impacto:** há capacidade de limpeza, mas a caracterização como daemon automático não foi comprovada.

Evidências: [reaper](../../src/daemon/horizon_reaper.c), `reap_single_horizon`; [TTL](../../src/daemon/horizon_reaper.h).

## O que o relatório descreve corretamente

Há catálogo de temas com `ABSENT`, bindings, claims, ECG, contratos, gateway, contestações, sessões e testes correspondentes no código. O Makefile inclui os 22 módulos Union. Os handlers de federação usam o merge com min-heap. A notação do relatório merece precisão: o heap mantém O(K) entradas; operações de inserção/remoção custam O(log K), não O(K) como complexidade total do merge.

As 12 ferramentas Union estão tanto na tabela de registro quanto no dispatcher. Isso corrige parte importante da auditoria histórica; não corrige automaticamente os demais gaps de integração. As três features abertas no IDE devem ser lidas como descrição parcialmente normativa, sobretudo nas garantias de sweep, autenticação e âncoras AST.

## Prioridades recomendadas e critérios de conclusão

1. **Fechar as fronteiras de autoridade:** vincular decisões humanas a evidência confiável do host; persistir propostas; diferenciar aceitação/recusa; testar chamadas sem proposta e campos autodeclarados.
2. **Integrar o ciclo:** definir política explícita para horizontes legados, mapear contestações de claims ao horizonte e executar sweep/trace antes de descartar estado.
3. **Preservar memória e evidência:** persistir temas/bindings/contratos, validar nós e versões citados e comprovar restauração após reinício.
4. **Alinhar garantias e implementação:** implementar análise AST real ou documentar a ancoragem como textual; conectar o reaper; corrigir exemplos e alegações documentais.
5. **Provar operação:** adicionar teste por transporte MCP que atravesse o ciclo completo, confira logs e persistência e cubra tentativas de contornar os gates; verificar separadamente a integração do harness-kit.

O critério para declarar o workflow concluído deve ser essa evidência operacional reproduzível. Contagem de módulos, ferramentas e testes aprovados demonstra progresso, mas não substitui a ligação entre as estações.
