# Descoberta automática de horizonte — não passou

Teste real em 2026-10-01, usando o binário instalado e uma conexão MCP nova. Entrada do leitor: somente `C-Users-corre-Documents-harness-kit`. Nenhum horizon_id foi fornecido; nenhum documento humano ou arquivo do cache foi lido; nenhuma ferramenta de escrita foi chamada.

O catálogo real de ferramentas não expõe listagem/descoberta de horizontes. `list_projects` retorna projetos, não os horizontes associados. `search_graph` com label `FractalTemenos` retornou zero itens. Busca textual e por nome encontrou documentação geral sobre temenos/horizonte, mas nenhum ID dos horizontes reais do teste. `query_graph` para `FractalTemenos`, sem `active_horizons`, também retornou zero itens. As cinco chamadas tiveram respostas MCP válidas, sem `isError`.

Resultado: `NOT_DISCOVERABLE_WITH_CURRENT_MCP`. Isso não demonstra que os horizontes foram apagados; demonstra que essas interfaces não permitem descobri-los recebendo apenas o projeto. A prova anterior de recuperação com ID explícito permanece um teste distinto.

Para alcançar descoberta real, o CBM precisa expor um catálogo consultável por projeto/contexto, associado aos metadados dos horizontes e ao seu ciclo real. O leitor descobriria candidatos, selecionaria o contexto com o humano quando ambíguo e só então consultaria os overlays com `active_horizons`. Listagem de arquivos pelo agente, IDs adivinhados e leitura do registro humano não aprovam esse critério.

[Relatório](report.json), [chamadas e respostas](mcp-transcript.json), [schemas reais](tool-schemas.json). Script reproduzível: `python ./skills-fractal/probe-horizon-discovery.py`.
