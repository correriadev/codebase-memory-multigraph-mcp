# Skills fractais — experimento 0.2

Fonte das três skills independentes para teste manual em Codex e Antigravity. O horizonte CBM é obrigatório; o documento humano é a representação legível correspondente. Hooks preservados. As provas reais exigiram uma correção no transporte das relações e a adição do catálogo MCP `list_horizons`, com vínculo persistido ao projeto.

- `temenos`: entrada explícita, contexto e primeira compressão.
- `dialogo-sombra`: pressupostos e consequências dentro do contexto delimitado.
- `integrar-temenos`: memória, expansão, pausa, encerramento e retomada.

O [contrato](temenos/references/contrato-fractal.md) e o [protocolo de memória](temenos/references/memoria-cbm.md) são distribuídos em cada pacote para instalação autossuficiente. As três cópias de cada referência devem permanecer idênticas. Estados conceituais são do experimento, não novos estados nativos do MCP. A aceitação de propósito desconhecido é uma hipótese de teste explícita.

## Instalação e invocação

Execute `./skills-fractal/install.ps1` a partir do repositório. O script copia somente estas três pastas para:

- Codex: `~/.codex/skills`.
- Antigravity IDE: `~/.gemini/config/skills`.
- Antigravity CLI: `~/.gemini/antigravity-cli/skills`.

Por padrão, não sobrescreve destinos diferentes. Para atualizar as três skills, use `./skills-fractal/install.ps1 -Update`: preserva e verifica um backup das versões anteriores antes de copiar. Escrita fora do workspace depende da permissão do host. Os diretórios Antigravity seguem a [documentação oficial](https://www.antigravity.google/docs/skills?tab=ide); a pasta antiga `~/.gemini/antigravity/skills` é legada e não recebe outra cópia para evitar duplicação.

Abra uma nova sessão para conferir descoberta. Codex: `$temenos`; Antigravity: `/temenos` ou pedido textual explícito para usar a skill. Se o host não descobrir a skill, indique o caminho absoluto do SKILL.md instalado; isso testa execução, não comprova descoberta automática. Dentro do temenos, não é necessário invocar as duas capacidades seguintes a cada turno. A política `allow_implicit_invocation: false` é metadado do Codex; no Antigravity a entrada explícita também consta nas instruções.

Exemplo inicial: `Use temenos. No projeto <nome inequívoco>, quero entender uma parte do sistema; ainda não sei para quê.`

Registros do teste ficam em `docs/temenos/<id>/registro.md` no workspace usado. Confira o arquivo e não apenas a resposta do agente.

## Teste manual

Execute os mesmos casos nos dois hosts, em sessões separadas. Use um projeto real com grafo e documentação contextual para os casos positivos. Anote host, prompt, caminho do registro, chamadas reais, resultado e impedimento. PASSOU requer comportamento observado; NAO_EXECUTADO e BLOQUEADO não são sucesso.

| Caso | Prompt/condição | Critério observável |
| --- | --- | --- |
| Entrada | Pergunta comum sem invocar skill; depois invocar temenos | Nenhum fluxo fractal automático; invocação inicia delimitação |
| Projeto ambíguo | `Use temenos. Qual o fluxo de uma operação?` num repo existente | Pergunta qual contexto; cwd não vira seleção humana |
| Projeto conhecido | Projeto explícito com grafo e núcleo contextual | Síntese não técnica com fontes antes da investigação específica |
| Curiosidade | `Não sei meu propósito; só quero entender.` | Propósito DESCONHECIDO não bloqueia próximo passo já delimitado |
| Sem projeto | `Use temenos. Quero explorar um sonho, sem projeto.` | SEM_PROJETO; exige substrato CBM real compatível ou registra bloqueio; não inventa grafo/projeto/diagnóstico |
| Greenfield | `Use temenos. Este é um projeto novo chamado Aurora.` | Declarações são origem da memória; não exige índice de código, mas exige horizonte em substrato compatível |
| Brownfield incompleto | Projeto existente sem grafo ou núcleo não técnico | Impedimento legível; não inventa contexto nem reclassifica como novo |
| Falha de MCP | Grafo inacessível ou ferramenta não exposta | Distingue falha de acesso de inexistência; persiste limitação |
| Sombra menor | `Estou considerando X; quais consequências?` | Examina tensão pertinente, evidencia condições; não força objeções |
| Tradição divergente | Norma fixada e evidência que a contesta | Registra vínculo e divergência; não apaga evidência |
| Mudança de contexto | Humano introduz outro projeto durante diálogo | Desambigua vínculo; cria filho apenas quando necessário |
| Pausa com questões | `Pare por aqui, ainda não decidi.` | PAUSADO com destinos abertos; não fabrica consenso/promoção |
| Encerramento | `Encerre esta conversa, mantenha a pergunta aberta.` | ENCERRADO local com MANTIDO_ABERTO; lifecycle MCP honesto |
| Retomada | Nova sessão, fornecer apenas projeto e horizon_id | Recupera contexto e dúvidas pelo MCP sem ler Markdown; verifica mudanças sem rebase silencioso |
| Persistência recusada | Ambiente bloqueia escrita ou encerramento MCP | Declara PENDENTE/RECUSADO e estado real, sem afirmar sucesso |

Avalie separadamente: descoberta da skill, execução local da metodologia, persistência humana, recuperação contextual do CBM e persistência organizacional no CBM. Um sucesso local não comprova integração MCP.

## Prova no grafo real

Descoberta automática corrigida com `list_horizons` e aprovada no MCP instalado. `python ./skills-fractal/probe-horizon-discovery.py` usa uma conexão nova, recebendo só projeto, sem ler documentos/cache ou fornecer IDs. [Correção e teste](../docs/temenos-tests/discovery-corrected.md). A [falha inicial](../docs/temenos-tests/discovery-20261001-195239/resultado.md) permanece preservada como evidência histórica.

Prova aprovada em 2026-10-01: projeto `C-Users-corre-Documents-harness-kit`, horizonte `h_fractal_real_20261001_194147`. [Resultado legível](../docs/temenos-tests/resultado-real.md) e [relatório das verificações](../docs/temenos-tests/real-20261001-194147/report.json). As nove instalações foram atualizadas e verificadas; versões anteriores preservadas em `C:/Users/corre/.skill-backups/fractal-update-20261001-194018`.

`python ./skills-fractal/prove-real-horizon.py --probe` consulta o catálogo e schemas do binário instalado. Sem `--probe`, usa o grafo real existente do HarnessKit e decisões do temenos `t-20261001-1754`, cria um horizonte novo e testa sincronização, recuperação por outro cliente MCP, revisão e recuperação imediata por cliente novo após a saída do escritor. Nunca indexa, promove ou altera o grafo base. O script reutiliza o cliente stdio existente em `tests/windows/mcp_stdio.py`.

O cliente B recebe apenas projeto e horizonte e consulta pelo MCP; o programa controlador conhece os dados esperados para validar o retorno. Isso comprova transporte e conteúdo, não substitui o teste comportamental manual em outra conversa de LLM. Evidências, schemas reais, revisões e falhas ficam em `docs/temenos-tests/real-<data-hora>/`.

Se o ambiente exigir runtime privado, passar `--runtime <diretório real autorizado>`; isso não muda o cache/grafo. Falhas de segurança do daemon são impedimentos de ambiente, não resultados aprovados. O script `prepare-real-cache.ps1` documenta a correção específica autorizada neste computador (backup de ACL da pasta `.cache` e runtime privado); não é um instalador genérico nem deve ser executado automaticamente pelas skills.

Recuperação após TTL/reinício do daemon e descoberta autônoma de horizontes permanecem gates separados. Não alegar memória durável a partir de leitura imediata bem-sucedida.

## Isolamento das demais skills

`./skills-fractal/isolate-skills.ps1 -PlanOnly` inventaria as pastas que serão retiradas. Sem esse parâmetro, move as demais skills dos diretórios locais inventariados para `~/.skill-backups/fractal-<data-hora>`, fora da descoberta padrão. Inclui a pasta `.system` do Codex e skills de plugins locais, preservando conexões, binários e configurações dos plugins.

Cada backup contém `manifest.json`, caminhos originais e hashes SHA256 de todos os arquivos. O script verifica o conteúdo após cada movimento e preserva as três skills fractais. Não altera as fontes antigas em `skills/` do repositório nem impede leitura arbitrária do backup: este é isolamento de descoberta, não uma barreira de segurança de filesystem.

Para restaurar, use `./skills-fractal/isolate-skills.ps1 -Restore -BackupPath '<caminho absoluto do backup>' -PlanOnly` para conferir e depois repita sem `-PlanOnly`. A restauração recusa sobrescrever uma pasta recriada pelo host. Em caso de interrupção parcial, confira o estado e os caminhos no manifesto antes de restaurar; a restauração automática pressupõe que todas as pastas foram movidas.

Abra novas sessões nos dois hosts e confira o catálogo. Instruções já carregadas na conversa atual permanecem. Atualizações do aplicativo, reinstalação de plugins ou outros workspaces podem disponibilizar novas skills: o isolamento verificado é dos diretórios locais inventariados neste perfil e workspace.

Backup realizado em 2026-10-01: `C:/Users/corre/.skill-backups/fractal-20261001-174539` (81 pastas, conteúdo verificado por hash).

Uma segunda conferência encontrou `.system` recriado automaticamente pelo Codex e quatro skills do plugin Sites. Foram arquivadas em `C:/Users/corre/.skill-backups/fractal-20261001-175027` (5 pastas adicionais, incluindo outra cópia de `.system`).

Para evitar que skills conhecidas reapareçam no catálogo do Codex, foram acrescentadas 68 entradas `[[skills.config]]` com `enabled = false` em `~/.codex/config.toml`, conforme a [documentação oficial](https://learn.chatgpt.com/docs/build-skills). O arquivo anterior está em `fractal-20261001-174539/codex-config-before-isolation.toml`; `codex-disable-report.json` registra caminhos e hashes. As três fractais não foram desabilitadas. Reinicie o Codex para aplicar.

Para desfazer a desativação, remova apenas o bloco delimitado por `fractal-skill-isolation` da configuração atual. A cópia integral anterior é uma alternativa somente se nenhuma outra configuração mudou desde o backup. Para restaurar pastas, confira conflitos: as duas cópias de `.system` têm o mesmo destino original, e o aplicativo pode tê-lo recriado novamente. Não restaure ambos os backups sobre esse destino sem reconciliar o conteúdo.
