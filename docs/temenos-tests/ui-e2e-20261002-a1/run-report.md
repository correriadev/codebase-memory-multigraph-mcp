# Session A: UI visual ideation E2E

- Project: `C-Users-corre-Documents-harness-kit` (`C:/Users/corre/Documents/harness-kit`, branch `feature/web-sdk`)
- Horizon: `h_ui_visual_ideation_e2e_20261002_a1`
- Artifact: `docs/temenos/ui-e2e-20261002-a1/visual-ideation.md` in HarnessKit
- Fixture status: every value is synthetic and declared for this test only; no user or product approval is represented.
- Scope selection: the complete catalog had 7 existing ACTIVE horizons (`partial=false`). MCP previews/payloads identified the prior card-persistence decision (`docs/backlog/*.md`); none contained conversational UI styling.

## MCP calls and results

1. `list_projects({detail:"identity",format:"json",limit:100,offset:0})` returned the exact project name and root above.
2. `list_horizons({project:"C-Users-corre-Documents-harness-kit",status:"ALL",limit:100,offset:0})` returned 7 of 7, `partial=false`. IDs: `h_fractal_real_20261001_190429`, `h_fractal_real_20261001_190712`, `h_fractal_real_20261001_190755`, `h_fractal_real_20261001_194114`, `h_fractal_real_20261001_194147`, `h_fractal_real_20261001_200159`, `h_fractal_real_20261001_200609`.
3. `create_horizon({project:"C-Users-corre-Documents-harness-kit",horizon_id:"h_ui_visual_ideation_e2e_20261002_a1",nodes:[...7 nodes...],edges:[...11 edges...]})` returned `success=true`, `status=ACTIVE`, `client_pid=4932`, `based_on_seq="0"`, `nodes_count=7`, `edges_count=11`. Node URIs share `cbm://C-Users-corre-Documents-harness-kit/docs/temenos/ui-e2e-20261002-a1/visual-ideation.md`: root `#artifact`, ordered parts `#part-01` through `#part-06`. Edges: root `CONTAINS_PART` each part; each consecutive pair `NEXT_PART`.
4. `sync_horizon_spec({project:"C-Users-corre-Documents-harness-kit",horizon_id:"h_ui_visual_ideation_e2e_20261002_a1",file_path:"docs/temenos/ui-e2e-20261002-a1/visual-ideation.md",content:<ordered part concatenation>})` returned `success=true`, `nodes_compiled=0`, `edges_compiled=0`, `sections_indexed=7`. The same call was repeated after correcting the file’s final newline; it returned the same counts.
5. `search_graph({project:"C-Users-corre-Documents-harness-kit",active_horizons:["h_ui_visual_ideation_e2e_20261002_a1"],query:"FractalArtifactPart UI-E2E-20261002-A1",format:"json",detail:"default",limit:100,max_output_tokens:10000})` returned 14 overlays: root, all 6 full part payloads, and 7 sections.
6. `query_graph({project:"C-Users-corre-Documents-harness-kit",active_horizons:["h_ui_visual_ideation_e2e_20261002_a1"],query:"MATCH (n) RETURN n.qualified_name AS qualified_name, labels(n) AS labels LIMIT 1",format:"json",max_rows:1})` exposed 14 horizon nodes (7 explicit artifact nodes + 7 sections) and all 11 expected edges.
7. `validate_scope_horizon({horizon_id:"h_ui_visual_ideation_e2e_20261002_a1",strict_connectivity:true})` returned `VALID`, `isolated_nodes_count=0`, `unresolved_dependencies_count=0`.

## Byte and hash verification

Python read the new Markdown file as bytes and compared it with the UTF-8 bytes reconstructed from the six MCP part payloads. The first comparison found the file was 4,778 bytes while MCP returned 4,779 bytes; the sole difference was the missing final LF in the file. Appending that LF made the bytes identical. The corrected file was then synced again.

- File bytes: 4,779
- MCP reconstruction bytes: 4,779
- Byte equality: `true`
- File SHA-256: `04e97568007d183ac5d303041408b12cc8b0baefe65f574b54386a34b14ccc9d`
- MCP reconstruction SHA-256: `04e97568007d183ac5d303041408b12cc8b0baefe65f574b54386a34b14ccc9d`

Part SHA-256 values in order:

1. `525d8bafefd0542e63c13874501f9843a44d757ac4e6b141890d4390079ec65c`
2. `9243c9d6b1c771336b89a81e1fbbc50db4d7b1337ef9d10ae2ce8fd022fdecd4`
3. `d04b66f002451fb1de319a1902db716a6f933152b233167e98f1bb914dd4e3aa`
4. `cc24a8ca82ee05791b7f13f6ad75b3c2fd588f6d94df2b5bab3110998c088271`
5. `f1631518868f0aa247c9379e45e63578658c12e784bc71f29bb444b453dc759c`
6. `3a9907bc0e510aed7fe20f1725ac0e793ce28b94040733dea93da42152483e33`

## Session B independente

A leitora recebeu somente o projeto e o assunto `UI conversacional de cards/refinamento do HarnessKit`; não recebeu `horizon_id`, caminho, valores visuais, hash esperado ou notas da sessão A. Usou apenas MCP e não leu arquivos, código ou cache do HarnessKit. `list_horizons(status=ALL)` retornou 8/8 candidatos, `partial=false`, `has_more=false`, `unreadable_entries=0`. Consultou os oito conteúdos; os sete horizontes históricos tratam da persistência de cartões Markdown de backlog. O único conteúdo de ideação visual foi `h_ui_visual_ideation_e2e_20261002_a1`, selecionado pelo conteúdo, não pela data.

A leitora recuperou todos os 14 nós do horizonte (raiz, seis partes, sete seções), todas as 11 arestas e o texto integral das seis partes sem truncamento. As partes tinham 704–923 bytes UTF-8; reconstrução total de 4.779 bytes. Os resultados incluíram o fluxo abrir conversa → ler turnos → compor/enviar → observar streaming → copiar ou tentar novamente; composição visual de rail 256 px, transcrição até 720 px, drawer de 320 px, header 56 px e composer mínimo de 96 px; tokens `#F7F8FA`, `#FFFFFF`, `#17202A`, `#657182`, `#DCE2E8`, `#3157D5`, `#25845A`, `#B76D12`; breakpoints de 1100/760/520 px; e estados do composer, streaming, navegação, drawer e acessibilidade. A leitora também identificou como desconhecidos o produto/fluxos reais, necessidade de rail/drawer, copy, marca/tema escuro, capacidades de backend, retenção, plataformas e responsável por aprovação.

Cada seção identifica os valores como `SYNTHETIC TEST FIXTURE`, declarados somente para o ensaio, com aprovação de produto igual a nenhuma. A hash calculada pela leitora sem arquivo foi igual à calculada pelo escritor e ao SHA-256 dos bytes do arquivo após corrigir o LF final. As sessões A/B foram subagentes isolados usando o MCP instalado; isso não equivale a executar manualmente a descoberta de skills no host Antigravity.

## Limitação

The artifact root still has `digest_status=PENDING_POST_ROUND_TRIP`. The live `create_horizon` schema accepts node payloads, but its documentation does not explicitly guarantee safe same-URI node upsert. No digest metadata mutation was attempted; the verified digest and part hashes are recorded here.
