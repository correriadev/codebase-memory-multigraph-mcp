---
name: temenos
description: "Inicie explicitamente um temenos: delimite o contexto compartilhado de uma conversa, recupere memória e registre fatos, hipóteses e lacunas sem presumir uma finalidade. Use quando o humano invocar temenos; não abra sessões fractais automaticamente."
---

# Temenos

Antes de atuar, leia [o contrato fractal](references/contrato-fractal.md) e [o protocolo de memória CBM](references/memoria-cbm.md). Esta skill é a entrada explícita do experimento; não depende das skills antigas nem de hooks. O horizonte é obrigatório; o arquivo é a representação humana correspondente.

O temenos é o menor espaço compartilhado em que humano e agente distinguem o dado, o suposto, o desconhecido e as consequências exploráveis, preservando vínculos com o contexto maior. Pode abrigar curiosidade, inquietação ou decisão; não exige tarefa, entrega técnica nem intenção conhecida.

## Delimitar

1. Registre a fala inicial sem convertê-la em objetivo oculto. Separe contexto, assunto e propósito. O propósito pode ser desconhecido.
2. Se o humano declarou um projeto, use sua declaração como seleção, mas verifique a identidade do projeto no catálogo disponível. Diretório atual, nomes semelhantes e histórico sugerem candidatos; não confirmam seleção. Quando faltar contexto, faça uma pergunta curta que permita escolher projeto existente, contexto novo ou exploração sem projeto. Não repita perguntas já respondidas.
3. Para projeto existente, consulte o CBM por operações de descoberta disponíveis e leia as fontes humanas associadas. Apresente a árvore inicial não técnica: identidade, descrição, objetivos, envolvidos, datas, vínculos e normas aplicáveis. Marque cada campo ausente; não o complete com conhecimento de mundo. Registre fontes e versão/data quando disponíveis.
4. Projeto existente sem grafo ou sem núcleo contextual verificável permanece em DELIMITACAO com impedimento explícito. Pode-se discutir como obter ou corrigir o contexto, mas não responder como se ele existisse. Um projeto novo nasce como contexto declarado pelo humano; indexação de código não é pré-requisito. Exploração sem projeto é um contexto válido, não um projeto fictício.
5. Resolva ambiguidades que mudariam a identidade do contexto, a evidência aplicável ou a autorização do próximo passo. Outras ficam registradas. Proponha um recorte mínimo em linguagem simples e permita correção; não exija confirmação ritual quando a declaração do humano já o estabelece.
6. Após selecionar e verificar o contexto, abra um horizonte novo via `create_horizon`. Antes de deliberação substantiva, grave e sincronize a primeira revisão com os nós de temenos/contexto e suas relações. Verifique o retorno por consultas com `active_horizons`. Sem isso, registre o impedimento e permaneça na delimitação; não substitua memória CBM por arquivo local.
7. Persista o registro humano e a evidência das operações conforme o protocolo. Só declare o contexto SOLIDIFICADO quando identidade, limites e fontes/lacunas estiverem legíveis e a memória inicial estiver recuperável no horizonte. SOLIDIFICADO qualifica a delimitação, não a verdade de todas as proposições.

## Comprimir e continuar

Produza uma síntese curta que preserve a fala motivadora, contexto, limites, fontes, divergências e questões abertas. Compressão não remove incerteza, evidência ou os nós das sombras anteriores. Indique projeto, horizon_id, revisão confirmada e arquivo da memória.

Dentro do temenos explicitamente aberto, use `dialogo-sombra` quando houver exame de pressupostos/consequências; use `integrar-temenos` quando houver consolidação, interrupção ou retomada. Leia o SKILL.md correspondente se disponível. Sua ausência não bloqueia conversa comum: siga o contrato e declare a limitação. Não use essas capacidades para iniciar outro temenos implicitamente.

Pare a delimitação quando houver contexto suficiente para o próximo passo ou quando a informação necessária depender do humano/MCP. Não force busca de intenção, implementação, backlog ou promoção institucional.
