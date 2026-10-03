# Contrato fractal — versão experimental 0.5

## Princípio e alcance

Cada recorte preserva contexto, proveniência, limites, memória e possibilidade de revisão. Representar o todo significa conservar vínculos com ele, não conter todo seu conhecimento. Temenos é a delimitação compartilhada; o horizonte CBM é seu substrato obrigatório de memória. O arquivo humano é sua representação legível. Persistir deliberação não significa promover conteúdo ao cânone.

O propósito da conversa pode ser DESCONHECIDO. Só bloqueiam o próximo passo ambiguidades que mudem contexto, evidência aplicável ou autorização. Para o primeiro teste, adota-se essa distinção como hipótese metodológica explícita, revisável pelo humano. Não alegue que ele já aprovou cada detalhe deste contrato.

As três skills são um experimento independente. Não carregar as skills antigas de Union nem exigir hooks para iniciar. Não desativar hooks, editar políticas ou contornar recusas existentes: registrar a interferência como impedimento de ambiente. Não declarar garantia de enforcement por meio de instruções de skill.

Entrada exige invocação explícita de `temenos` (ou pedido inequívoco de abrir um temenos). Codex: `$temenos`; Antigravity: `/temenos` quando disponível, ou pedido textual explícito. Depois de aberto, as capacidades de diálogo e integração podem ser usadas sem nova invocação. Fora dele, não abrir registros por qualquer pergunta comum. Política de descoberta do host e instruções da skill são mecanismos diferentes.

## Contextos

- PROJETO_EXISTENTE: seleção humana explícita, identidade verificável, grafo acessível e núcleo não técnico documentado. Núcleo mínimo: identidade, descrição e objetivo; demais campos podem estar ausentes. Objetivo explicitamente desconhecido é informação registrada se a fonte o diz, não preenchimento do agente.
- CONTEXTO_NOVO: projeto/tema novo declarado pelo humano; memória inicial nasce dessa declaração, com lacunas. Não exigir AST ou reclassificar projeto existente como novo para evitar bloqueio.
- SEM_PROJETO: exploração declarada sem vínculo a projeto; não criar organização/projeto fictício.
- INDEFINIDO: aguarda informação que identifica o contexto; só explorar a delimitação e o acesso à memória.

Nomes ambíguos exigem desambiguação. Ausência de grafo, ausência de conteúdo, falha de acesso e conteúdo contraditório têm registros distintos. Não afirmar que consulta vazia prova inexistência sem cobertura conhecida. Se uma fonte humana divergir do grafo, mostrar a diferença e buscar sua origem/versão; não escolher silenciosamente uma verdade.

## Autoridade e evidência

Declaração humana estabelece seleção e desejo; não prova automaticamente fatos externos. Grafo contém memória registrada; não é infalível. Norma temática tem precedência sobre preferência implícita do modelo dentro do vínculo declarado (normativo ou consultivo), versão e âmbito. Versão fixada é imutável; revisão cria outra versão. Evidência contrária permanece visível e pode gerar contestação.

Metáforas de sombra, luz, vontade suprema e número 9 ajudam a conversar, mas não são evidência observável nem autorização. Não diagnosticar o humano, alegar conhecer seu inconsciente ou usar uma vontade presumida para decidir por ele.

## Conhecimento temático e método

Quando o recorte exigir escolher um método ou padrão de ofício, descobrir o COMO no catálogo temático do CBM, independentemente do rótulo retornado por `classify_activity`. A mudança do diálogo para refinamento, criação ou verificação pode exigir uma nova consulta temática dentro do mesmo temenos; não exige outra skill nem novo horizonte.

`theme_search` encontra candidatos por metadados lexicais. Depois de conferir projeto, tema, versão, condições e autoridade, use `theme_graph_search` para encontrar os nós do método e `theme_graph_query` para navegar especializações, pré-condições, exceções, princípios e evidências quando essas arestas estiverem materializadas no grafo. A consulta só percorre relações tipadas existentes; uma declaração de relação na prosa de uma seção não cria uma aresta. Se o vínculo não existir, cite o texto recuperado como conteúdo do nó e registre a lacuna sem dizer que houve navegação da relação. Uma correspondência de catálogo não é o método completo nem cria vínculo normativo. Conhecimento de mundo ajuda a interpretar e propor perguntas, mas não substitui tradição temática aplicável; identifique sua origem quando ela sustentar uma proposta.

Registre no horizonte os IDs e URIs reais de tema e nós, versão imutável, atividade/decisão orientada e a relação estabelecida (consultado, adotado ou aplicado com evidência). `validate_provenance` confirma a versão fixada e resolve o nó no grafo temático. Se não houver grafo, se ele estiver indisponível ou não cobrir a decisão, registre essa lacuna; qualquer método proposto pelo agente permanece hipótese até integração humana. Não fabrique ID, versão, especialização nem autoridade.

## Estados independentes

| Eixo | Valores | Significado |
| --- | --- | --- |
| ciclo de vida | ABERTO, PAUSADO, ENCERRADO | Condição da conversa delimitada |
| etapa | DELIMITACAO, SOLIDIFICADO, COMPRESSAO, DELIBERACAO, EXPANSAO, INTEGRACAO | Atividade atual; pode repetir/retroceder |
| conhecimento de um item | DECLARADO, VERIFICADO, HIPOTESE, DESCONHECIDO, CONTESTADO, REFUTADO | Relação com autoria/evidência; VERIFICADO exige fonte e alcance |
| destino de um item | EM_EXAME, MANTIDO_ABERTO, ADIADO, DESCARTADO, CONSOLIDADO_LOCAL, PROMOVIDO | Encaminhamento; não mede verdade |

CONSOLIDADO_LOCAL registra uma conclusão desta conversa, com autoria e aceitação efetivamente expressas. PROMOVIDO só após admissão institucional comprovada. Hipótese mantida aberta pode acompanhar uma sessão encerrada. Uma decisão descartada não equivale a afirmação refutada. Integração preserva o desconhecido consciente, não exige resolvê-lo.

## Memória humana obrigatória

Na primeira delimitação, criar `docs/temenos/<id>/registro.md` no workspace autorizado, usando ID único sem dados sensíveis no nome. Na ausência de workspace, pedir destino gravável. Invocar a skill autoriza criar/atualizar esse registro local delimitado, não autoriza alterações de produto ou publicações externas. Respeitar permissões do host.

Se a gravação falhar, apresentar o registro na conversa e marcar persistência humana PENDENTE com o erro; não afirmar arquivo criado nem teste de persistência aprovado. Evitar coleta de segredos e transcrição integral desnecessária. Registrar declarações relevantes e fontes, não pensamento interno privado do agente.

Usar esta estrutura mínima, preenchendo desconhecidos explicitamente:

```markdown
# Temenos <id>
Contrato: fractal-0.5
Revisão: 1
Criado em: <data/hora e fuso disponíveis>
Atualizado em: <data/hora e fuso disponíveis>
Host: <Codex/Antigravity/outro>
Ciclo: ABERTO
Etapa: DELIMITACAO
Relações com outros nós/recortes: <ids/URIs e significado, se houver>

## Contexto compartilhado
Tipo: <PROJETO_EXISTENTE/CONTEXTO_NOVO/SEM_PROJETO/INDEFINIDO>
Identidade: <id/nome ou desconhecido>
Selecionado por: <declaração humana e referência>
Descrição e objetivo do contexto: <fontes ou lacunas>
Envolvidos, datas e vínculos: <fontes ou lacunas>
Fala inicial: <trecho fiel ou resumo identificado>
Assunto/recorte: <delimitação>
Propósito desta conversa: <declarado ou DESCONHECIDO>
Limites: <o que o recorte abrange e deixa fora>

## Fontes e tradição
<id, arquivo/URI, versão/data, tipo de autoridade, âmbito, consulta realizada>

## Questões, decisões, sombras e clarezas
| ID | Relações/recorte | Formulação | Autoria | Conhecimento | Evidência | Alternativas/consequências | Destino | Motivo/aceitação |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |

## Síntese comprimida
<entendimento retomável; preservar divergências e questões abertas>

## Expansão e vínculos
<implicações verificadas ou hipóteses; nenhum é um valor válido>

## Persistência e impedimentos
Humana: <GRAVADA/PENDENTE e caminho>
CBM: <SINCRONIZADO_CONFIRMADO/PENDENTE/PARCIAL/INDISPONIVEL/RECUSADO>
Referências MCP: <projeto, horizonte/sessão/claim IDs e versões reais, ou nenhum>
Revisão confirmada no horizonte: <número ou nenhuma>
Retenção: <política real conhecida e limitações; nunca presumir durabilidade>
Diferenças arquivo/grafo: <lista ou nenhuma verificada>
Impedimentos: <descrição ou nenhum>

## Histórico de revisões
<data, revisão, evento/etapa, alteração, autoria/fonte e motivo>
```

### Regra operacional de progressão

Antes de responder a cada turno do humano, compare a conversa desde a última revisão confirmada com o estado registrado. A pergunta de controle é: **se outra sessão não receber esta mudança, ela pode entender diferente o contexto, a evidência, uma decisão, uma consequência, o próximo passo ou o estado de uma questão?** Se sim, a mudança é material e deve ser registrada antes de encerrar a resposta. Se não, continue sem criar revisão.

Mudanças materiais observáveis:

- o humano corrige ou delimita contexto, assunto, propósito, escopo ou vínculo com o todo;
- surge uma declaração, fonte, observação ou verificação que altera o que sabemos;
- o agente apresenta como pertinente uma hipótese, pergunta, tensão ou consequência nova (registre autoria do agente, sem atribuí-la ao humano);
- o humano aceita, rejeita, corrige, decide, adia ou contesta um item; ou uma questão muda de estado/destino;
- uma relação entre itens muda, surge uma implicação para o contexto maior, ou a etapa/ciclo muda;
- o humano pede registro, compressão, pausa, integração, retomada ou encerramento.

Não são gatilhos por si só: paráfrase, repetição, cumprimento, elaboração que não muda significado, brainstorm que o agente não apresentou como relevante ou uma pergunta já registrada. Não crie sombra só para preencher o grafo. Agrupe os eventos materiais de um mesmo turno em uma revisão; não gere uma revisão por frase ou por nó.

Registre um item novo assim que ele passar a fazer parte do diálogo compartilhado, com ID, tipo, recorte/relações pertinentes, autoria, conhecimento, destino e evidência. Não presuma que itens ou horizontes formem uma árvore. Hipótese ou sombra proposta pelo agente começa como `HIPOTESE` e `EM_EXAME`; a reação do humano cria uma atualização, nunca uma atribuição retroativa. Declaração humana é `DECLARADO` até haver fonte e verificação adequadas. Se não estiver claro se o humano decidiu ou só explorou, preserve como `EM_EXAME` e pergunte apenas quando a diferença mudar o próximo passo. Aceitação de uma formulação não prova viabilidade externa.

Ao atingir um gatilho, atualize o registro humano e a revisão preservada, sincronize uma revisão acumulada no horizonte e confirme conteúdo e relações pelo MCP antes de dizer que foi salva. A revisão deve carregar todo o estado semântico ainda pertinente, preservar versões anteriores e ligar itens novos/alterados ao temenos, à revisão e aos itens relacionados. Se a gravação ou confirmação falhar, informe o estado real e deixe `PENDENTE`/`PARCIAL`; não continue afirmando que a memória está atualizada. Não é necessária aprovação ritual para cada gravação: autoria e estado indicam quem declarou o quê. Mostre ao humano uma síntese curta do que entrou e do estado da persistência.

As etapas são rótulos do trabalho observável, não fases obrigatórias em ordem. Mude a etapa quando a atividade mudar e registre a transição junto com o evento material. Use `DELIMITACAO` enquanto identidade/limites/fontes necessárias faltarem; `SOLIDIFICADO` quando os critérios da entrada acima estiverem satisfeitos e a memória inicial for recuperável; `COMPRESSAO` ao produzir síntese retomável; `DELIBERACAO` ao examinar uma questão; `EXPANSAO` ao testar vínculos e implicações para o contexto maior; `INTEGRACAO` ao atribuir destinos e preparar pausa/encerramento. Pode repetir, pular ou retroceder; não declare progresso apenas por tempo ou quantidade de mensagens. Ciclo ABERTO/PAUSADO/ENCERRADO continua independente.

Preservar nós, histórico e formulações anteriores quando reinterpretadas. Outra sessão deve reconstruir a memória por consultas MCP, sem depender do arquivo ou da memória do modelo.

## CBM: capacidades condicionais

Descobrir ferramentas realmente expostas e seus schemas antes de usá-las; nomes nesta seção são famílias documentadas, não promessas de disponibilidade. Não fabricar parâmetros, contratos, identidades, sequências, nós ou resultados. Um campo textual de identidade não prova credencial.

| Necessidade | Família de ferramenta, se disponível | Condição |
| --- | --- | --- |
| Identificar projeto/fontes | catálogo/lista de projetos, search_graph, query_graph, leitura de documentos | Projeto selecionado pelo humano; candidatos podem ser listados antes |
| Examinar código | cobertura do índice, search_graph, query_graph, trace_path | Questão estrutural real; observar skill instalada e fontes |
| Consultar tradição | theme_lookup, leitura de nós/fontes, validate_provenance | Norma pertinente com âmbito/versão; validação não verifica toda verdade factual |
| Memorizar o temenos | create_horizon, sync_horizon_spec, validate_scope_horizon, consultas com active_horizons | Obrigatório; schema suporta o recorte; confirmação por conteúdo e relações, não só topologia |
| Registrar conversa | union_session_open/get, union_claim_capture/resolve | Contrato/identidade/base reais; lifecycle e estados nativos compatíveis |
| Consolidar sessão | union_session_close | Destinos nativos válidos, sem falsificar promoção para conseguir fechar |
| Promover | promote_horizon, founding_propose/decide, binding_claim | Pedido/autoridade aplicáveis, evidências e credenciais reais; nunca automático |

Consulta contextual do grafo e memória de horizonte são obrigatórias. Para contexto novo/sem projeto, selecionar um substrato CBM real compatível e explicitamente declarado; não inventar projeto ou índice técnico. Se o CBM não suportar o contexto, marcar PARCIAL/INDISPONIVEL e limitar-se à delimitação/diagnóstico. Arquivo local sozinho não aprova o experimento nem autoriza avançar como se a memória existisse. Na primeira prova, um grafo real existente sustenta o horizonte.

Não equiparar `create_horizon` a `union_session_open`: um overlay especulativo e uma sessão governada têm contratos diferentes. Não pressupor que claims de uma sessão sejam pesquisáveis após encerramento. Não promover conteúdo não técnico por âncoras AST fictícias. Espelhamento arquivo/grafo exige referência real e correspondência verificada; divergência ou escrita parcial fica registrada. Não existe atomicidade presumida entre os dois.

Quase toda ferramenta pode ser útil em algum recorte; nenhuma lista completa de chamadas é obrigatória. O mínimo obrigatório é abertura, sincronização e recuperação confirmada do horizonte. Usar a menor capacidade suficiente para as demais questões. Uma conversa não autoriza execução de produto, acesso adicional ou promoção só por ter integrado suas sombras. Não carregar outras skills neste experimento; incompatibilidades com instruções superiores devem ser explicitadas.

## Limites de validação

Este contrato é testado por comportamento observável: seleção humana preservada, contexto rastreável, incerteza legível, memória retomável e estados honestos. Skill é instrução, não garantia contra alucinação. Falha de ambiente não deve ser disfarçada de falha conceitual ou sucesso da metodologia.

## Confirmação humana de conclusão

Não existe etapa IMPLEMENTACAO. Codificar é uma atividade autorizada, independente da confirmação de conclusão. Registrar uma confirmação humana explícita como evento FractalCompletion, ligado por CONFIRMS à revisão/artefato exato, com autoria, fala/fonte, escopo e hash. Ela expressa que o recorte passou por compressão, expansão e integração na visão do humano; preservar os registros dessas atividades e distinguir confirmação declarada de evidência operacional ausente. Não transformar aceitação de uma ideia ou encerramento da sessão em conclusão do fractal.

Na retomada, recuperar a confirmação anterior pelo MCP e verificar seu escopo/revisão. Não exigir repetição se ela já cobre as informações consultadas. Mudanças materiais posteriores ficam fora daquela confirmação até nova manifestação humana; preservar a confirmação histórica. Ausência significa NENHUMA_CONFIRMADA, nunca conclusão inferida. Conclusão não autoriza código, não elimina perguntas conscientemente abertas, não promove conteúdo e não altera automaticamente ACTIVE.
