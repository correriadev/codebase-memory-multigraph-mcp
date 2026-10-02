# Prova de memória fractal no grafo real

Em 2026-10-01, a prova passou usando o binário instalado do CBM e o grafo existente do HarnessKit: `C-Users-corre-Documents-harness-kit`, com 4457 nós e 14274 relações. Não foi criado um grafo base artificial, nem solicitada indexação ou promoção ao cânone.

Horizonte aprovado: `h_fractal_real_20261001_194147`.

## O que foi observado

A primeira revisão compilou seis nós semânticos e oito relações; a segunda, sete e onze. Ambas passaram na validação estrita de conectividade. Foram preservados contexto, decisão, pergunta aberta, consequência hipotética e evidência do registro real `t-20261001-1754`. Aceitação humana continua DECLARADO; implementação continua NAO_VERIFICADA. A revisão seguinte é explicitamente uma reexpressão do agente para teste, não nova decisão humana.

Um cliente B recuperou payloads completos e relações pelo MCP enquanto A permanecia conectado. B viu a revisão seguinte sem reiniciar, incluindo a relação SUPERSEDES e o conteúdo anterior. Após a saída de A e B, um cliente C novo recuperou todos os payloads. B e C consultaram somente projeto e horizonte; não leram o documento original ou as revisões Markdown. O controlador conhecia os valores esperados para comparar os retornos.

[Relatório completo](real-20261001-194147/report.json), [chamadas e respostas](real-20261001-194147/mcp-transcript.json), [revisão 1](real-20261001-194147/r001.md), [revisão 2](real-20261001-194147/r002.md) e [schemas do servidor real](real-20261001-194147/tool-schemas.json).

## Repetir em outra conversa

Abra uma sessão nova de Codex ou Antigravity com as skills atualizadas e MCP disponível. Forneça:

> Use integrar-temenos. Retome pelo MCP o projeto C-Users-corre-Documents-harness-kit, horizonte h_fractal_real_20261001_194147. Recupere contexto, decisão, pergunta aberta, consequência, evidência, estados e a relação entre as revisões, sem consultar arquivos físicos. Informe qualquer lacuna.

Após descobrir os schemas reais, a consulta de conteúdo utilizada na prova foi:

```json
{"project":"C-Users-corre-Documents-harness-kit","active_horizons":["h_fractal_real_20261001_194147"],"limit":100,"format":"json"}
```

Essa chamada é para `search_graph`; os payloads estão em `active_horizon_overlays`. Para `query_graph`, acrescente `query: "MATCH (f:File) RETURN f.path LIMIT 1"`. Confira `horizon_nodes` e `horizon_edges` separadamente do resultado base. A consulta base não filtra integralmente o overlay.

## Falhas encontradas e corrigidas

O binário anterior retornou sucesso de sincronização com zero nós e relações para linhas JSON longas. O protocolo agora exige JSON indentado, linhas de até 480 bytes UTF-8 e conferência de contagens e conteúdo. Não houve correção do parser neste trabalho.

Com conteúdo compilado, `query_graph` retornava `result: null`. As strings das relações referenciavam buffers SQLite já liberados antes da serialização. Em `src/mcp/handlers.c`, quatro campos agora são copiados para a memória do documento JSON. O CBM foi recompilado, instalado e seu daemon reiniciado; a mesma prova então recuperou as relações. SHA256 instalado: `698d4b008882b1f2af18344e5bbc4eb52005495ea540776eb2947a76e560f0a3`. A configuração de hooks do Codex foi preservada.

As três skills foram atualizadas nas instalações Codex, Antigravity IDE e Antigravity CLI, com hashes conferidos e backup anterior em `C:/Users/corre/.skill-backups/fractal-update-20261001-194018`. O isolamento das demais skills permanece configurado; sessões devem ser reiniciadas para atualizar o catálogo.

## Limites e ambiente

Descoberta automática inicialmente não passou. A lacuna foi corrigida com `list_horizons`, e uma nova conexão descobriu e recuperou um horizonte recebendo somente o projeto. [Correção e evidências](discovery-corrected.md). A falha original foi preservada, e retenção durável continua não comprovada.

Isso valida transporte real e recuperação imediata entre clientes MCP. Ainda não valida comportamento de outra conversa de LLM, descoberta autônoma de temenos, retenção após morte do proprietário/TTL, recuperação após reinício do daemon, filtragem Cypher completa do horizonte ou promoção institucional. O proprietário retornado é o daemon real, PID 16884; sair do cliente A não mata esse proprietário.

O suporte atual pode remover horizontes de proprietário morto com idade superior a 3600 segundos. Portanto, esta prova não declara memória organizacional durável. Falhas futuras de retenção não podem ser compensadas por leitura do Markdown e tratadas como recuperação CBM.

Para permitir acesso seguro ao runtime real, foram ajustadas duas permissões explícitas do grupo sandbox em `.cache`, `AppData` e `AppData/Local` para leitura/execução, preservando usuário, SYSTEM e administradores. ACLs anteriores foram guardadas nos diretórios `cache-acl-20261001-184715`, `runtime-acl-20261001-190013` e `runtime-acl-20261001-190356`, nesta pasta de testes. Os scripts de preparação documentam mudanças específicas deste computador e não são etapas automáticas das skills. Alterações de ACL e instalação ocorreram com aprovação do host.
