# Descoberta de horizontes corrigida

O CBM agora expõe `list_horizons`. Uma sessão recebe apenas o nome exato do projeto, descobre candidatos pelo MCP e utiliza os IDs retornados para recuperar os overlays. O agente não precisa consultar Markdown ou enumerar arquivos do cache.

```json
{"project":"C-Users-corre-Documents-harness-kit","offset":0,"limit":50}
```

O catálogo retorna ID, estado nativo, PID do proprietário e sua presença atual, datas, base, origem da associação e uma prévia contextual quando houver nó FractalTemenos. A prévia tem limite de 1000 caracteres e sinaliza truncamento; recuperação integral continua usando search_graph/query_graph. Ordenação por ID permite paginação estável para o conjunto observado. Alterações concorrentes no catálogo não têm garantia de snapshot entre páginas.

O filtro padrão é ACTIVE; `status=ALL` inclui os demais estados nativos armazenados. A leitura não ativa nem promove horizontes e não atualiza heartbeat. Entradas inacessíveis são sinalizadas por `partial` e `unreadable_entries`; falha de acesso não vira catálogo vazio confirmado.

Novos horizontes persistem o projeto na tabela horizon_context, inclusive antes da compilação de nós. create_horizon e sync_horizon_spec recusam vínculo conflitante. Horizontes antigos permanecem descobertos pela autoridade exata das URIs cbm://<projeto>/..., com associação marcada legacy_node_uri; não há migração de escrita durante listagem. Horizonte antigo vazio e sem vínculo explícito não tem contexto recuperável para associação ao projeto.

## Teste real

Validação final do binário instalado: [descoberta e recuperação](discovery-20261001-200631/report.json), [chamadas reais](discovery-20261001-200631/mcp-transcript.json) e [regressão de escrita/recuperação](real-20261001-200609/report.json). A conexão leitora descobriu sete horizontes sem receber IDs e recuperou `h_fractal_real_20261001_200609`. As dez verificações de descoberta passaram. A regressão também confirmou descoberta do horizonte ainda vazio por vínculo explícito e recusa de create/sync com projeto conflitante, preservando metadados e memória.

Binário recompilado e instalado, daemon reiniciado, configuração de hooks preservada. SHA256 instalado: `63e3c0a6056601aa5b66047083c1c9b4792f562b48f7a3acf0e9a17033e9df55`. Reiniciar as sessões dos agentes para carregar o novo schema MCP e as skills atualizadas.

[Primeira prova após a correção](discovery-20261001-200218/report.json) descobriu seis horizontes no grafo HarnessKit a partir apenas do projeto. Uma nova conexão recuperou conteúdo, contexto, decisão, pergunta aberta, consequência, evidência e relação SUPERSEDES de um candidato descoberto. Paginação em páginas de dois, repetição estável, isolamento por projeto e rejeição de parâmetros inválidos passaram.

O candidato usado pela prova técnica foi escolhido deterministicamente para conferir o transporte. Isso não aprova seleção automática do contexto humano: com vários candidatos pertinentes, o agente deve apresentar opções e esclarecer qual retomar. A skill integrar-temenos e as três cópias do protocolo foram atualizadas em Codex, Antigravity IDE e CLI, com backup em `C:/Users/corre/.skill-backups/fractal-update-20261001-195846`.

Para teste manual, abra uma nova sessão e peça:

> Use integrar-temenos. No projeto C-Users-corre-Documents-harness-kit, descubra pelo MCP os temenos existentes. Apresente os contextos para eu escolher, sem ler arquivos físicos e sem presumir minha intenção.

O teste anterior sem catálogo continua preservado em [falha inicial](discovery-20261001-195239/resultado.md). A recuperação durável após TTL e o comportamento dos hosts continuam critérios separados. Nenhuma indexação do grafo base ou promoção foi solicitada por estas provas.
