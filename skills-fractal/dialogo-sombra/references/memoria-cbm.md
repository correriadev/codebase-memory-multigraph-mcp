# Memória CBM do temenos — protocolo experimental 0.5.3

## Descobrir e navegar o COMO temático

Ao surgir uma atividade em que o método possa mudar uma decisão material, consultar o catálogo mesmo que `classify_activity` a rotule CONSULTATIVE. Use `theme_search` com a atividade e tecnologias/fatos já confirmados no grafo do projeto; `theme_list` permite percorrer o catálogo quando a consulta lexical não encontrar candidato. A busca usa ID, nome, namespace, descrição, aliases e tags e exige que todos os termos ocorram; ausência de resultado não prova que o método inexiste.

Resolver cada candidato com `theme_lookup(theme_id, version)` e examinar estado, descrição e URI do grafo. Uma especialização é candidata apenas quando projeto e condições de aplicação combinam. Use `theme_graph_search(theme_id, version, query)` para achar nós do método e `theme_graph_query` para navegar as relações e recuperar seus princípios, pré-condições, exceções e evidências. Essas duas tools reaproveitam busca e consulta read-only dos grafos de projeto CBM apontados pelo catálogo. `theme_graph_query` atravessa apenas arestas tipadas que já existam no grafo indexado: um rótulo de relação escrito em prosa Markdown não cria uma aresta. Confira os tipos e os resultados reais; até uma relação ser materializada, trate-a como texto pesquisável da seção e não afirme que o grafo a navegou. Se `target_uri` não apontar para um projeto CBM existente, o tema continua registrado mas não é navegável por essas tools.

Antes de usar um nó como tradição, chame `validate_provenance` com ID, versão exata e `node_uri` devolvido em `citation_candidates`. A confirmação inclui `anchor_verified=true`. Grave no horizonte um `FractalThemeReference` com os valores reais, atividade/decisão relacionada, autoridade (se houver binding explícito) e destino: CONSULTADO, ADOTADO ou APLICADO_COM_EVIDENCIA. Arestas para grafos temáticos externos ainda não são arestas físicas entre tenants; preserve a URI verificada no payload do horizonte. A recuperação posterior segue a URI versionada e pode revalidá-la.

No `tactical-spec`, represente essa referência como nó local com `symbol`, `type: "FractalThemeReference"`, `cbm_uri` único e `description` contendo JSON serializado com `theme_id`, `version`, `node_uri`, `decision_id`/`activity_id`, `disposition` e evidência. Ligue a decisão local ao nó de referência por `INFORMED_BY`. O compilador atual guarda esses detalhes como texto em `description`, não como propriedades graph nativas; não use a URI externa como endpoint de aresta cross-tenant.

`classify_activity` é heurística e nunca serve como gate da descoberta. Se não houver método aplicável, registre ausência/indisponibilidade e proponha alternativas como hipóteses do agente, identificando uso de conhecimento de mundo. O agente não registra ou promove um tema apenas porque o consultou.

## Dois acessos à mesma memória

O humano acessa `registro.md` e revisões Markdown; o agente recupera conteúdo e relações pelo MCP. Ambos preservam os mesmos IDs, contexto, autoria, evidência e estados. O horizonte não é opcional. Um arquivo gravado sem sincronização confirmada é PENDENTE/PARCIAL, nunca sucesso equivalente.

Antes de chamar uma tool, descobrir seu schema real. O suporte verificado no código do CBM é `create_horizon`, `sync_horizon_spec`, `validate_scope_horizon` e consultas com `active_horizons`. A versão instalada pode diferir: guardar seus retornos e limitações.

## Abrir e sincronizar

Após seleção humana e verificação do contexto, consulte `list_horizons` para o projeto exato. Se o humano escolher retomar um horizonte, use o ID retornado pelo catálogo e apenas consultas; nunca use `create_horizon` para procurar ou retomar, pois isso pode reativar e alterar um horizonte antigo. Se o humano escolher um recorte novo, crie um `horizon_id` novo (ASCII, letras/dígitos/underscore/hífen) e use o ID retornado.

Para grafo de projeto existente, `project` é o nome real retornado pelo catálogo. Outros contextos dependem de um substrato real compatível. Não criar um projeto fictício nem indexar repositório sem autorização só para vencer essa condição.

Guardar a geração/base efetivamente retornada; não fabricar `based_on_seq`. Não reutilizar credenciais, contratos ou `union_session_open` para simular um overlay. Sessão Union e horizonte de nós são mecanismos distintos.

Cada revisão material, conforme o gatilho operacional do [contrato](contrato-fractal.md), produz:

- `docs/temenos/<id>/registro.md`: síntese humana atual, com horizonte, revisão e estados de persistência.
- `docs/temenos/<id>/revisoes/rNNN.md`: versão preservada, com o documento humano integral; o envelope de transporte correspondente fica em `revisoes/rNNN.cbm.md`.
- `docs/temenos/<id>/cbm-log.md` (ou JSON): argumentos/retornos reais das operações, com revisão, hora disponível, erros e consultas de confirmação. Resumos de conversa devem ser identificados como resumos, não transcrição integral.

Enviar o envelope gerado a `sync_horizon_spec` com `horizon_id`, `project`, `file_path` relativo estável e `content`. O caminho deve identificar a revisão; `content` evita depender de acesso do servidor ao arquivo. Falha em qualquer representação fica explícita. Não presumir transação atômica arquivo/MCP.

## Formato interpretado pelo compilador

Markdown com títulos cria nós Section e índice de seções. Tabela de decisões, YAML frontmatter e bloco `graph` não substituem declaração semântica no compilador de horizonte. Para isso usar uma fence literal `tactical-spec` cujo corpo é um objeto JSON com `nodes` e `edges`.

Campos de nó lidos: `symbol`, `type`, `cbm_uri`/`uri`, `target_path`/`path`, `description` e `methods`. Campos de aresta: `source`, `target`, `type`/`edge_type`. Usar `symbol`, `type`, `cbm_uri` e `description`; cada ponta deve resolver para um nó conhecido do documento ou URI real verificada. Campos adicionais não são automaticamente persistidos como propriedades.

Compatibilidade observada no binário instalado em 2026-10-01: JSON com linhas longas retornou `success: true`, mas compilou zero nós e zero arestas. Usar JSON indentado e linhas físicas com até 480 bytes UTF-8; dividir registros grandes em nós vinculados, sem truncar evidências. Conferir contagens e conteúdo recuperado sempre. Uma resposta MCP nula ou sem `result.content` é falha, mesmo sem `isError`.

Exemplo ilustrativo; substituir projeto, IDs e evidência reais:

````markdown
# Temenos t-001
## Revisao 1
Contexto selecionado: projeto-x. Propósito desconhecido.
## Decisao D-01
O humano aceitou explorar cards conversacionais; implementação não verificada.
## Grafo
```tactical-spec
{
  "nodes": [
    {
      "symbol": "T-001",
      "type": "FractalTemenos",
      "cbm_uri": "cbm://projeto-x/docs/temenos/t-001/registro.md#T-001",
      "description": "{\"id\":\"T-001\",\"context\":\"projeto-x\",\"cycle\":\"ABERTO\",\"stage\":\"DELIBERACAO\",\"purpose\":\"DESCONHECIDO\",\"revision\":1}"
    },
    {
      "symbol": "D-01-R1",
      "type": "FractalDecision",
      "cbm_uri": "cbm://projeto-x/docs/temenos/t-001/registro.md#D-01-R1",
      "description": "{\"id\":\"D-01-R1\",\"predicate\":\"O humano aceitou explorar cards conversacionais\",\"knowledge\":\"DECLARADO\",\"destination\":\"CONSOLIDADO_LOCAL\",\"author\":\"humano\",\"evidence\":\"turno 4: essa abordagem esta boa\",\"implementation\":\"NAO_VERIFICADA\"}"
    }
  ],
  "edges": [
    {"source": "T-001", "target": "D-01-R1", "type": "CONTAINS"}
  ]
}
```
````

Esses tipos e relações são convenções do experimento, não validações semânticas novas no CBM. Usar nós explícitos para contexto, temenos, revisões, fatos/hipóteses, decisões, perguntas, restrições, consequências e evidências quando materialmente relevantes. Todos os itens precisam de ligação com o recorte; uma sobra isolada denuncia vínculo perdido. Não inventar itens só para preencher categorias.

## Estados e autocontenção

`description` contém um JSON serializado com `id`, `temenos_id`, `predicate` (ou descrição contextual), `knowledge`, `destination`, `author`, `evidence`, `revision` e consequências/referências pertinentes. Os campos são convenção recuperável como conteúdo textual, não propriedades Cypher individuais. Cada nó deve ser compreensível sem consultar o diálogo original. Evidências podem incluir trechos mínimos, origem, versão/hash quando disponível e seu alcance.

O compilador atual grava estado nativo `PROPOSED` e `is_dangling=1`, inclusive para decisões locais aceitas. Preservar os estados conceituais no payload. Não chamar ACCEPTED de verdade verificada nem promover para obter status. Aprovação da abordagem comprova aceitação declarada; não comprova viabilidade/implementação.

Manter payload de cada nó abaixo de 3000 bytes UTF-8 nesta versão: o transporte atual de overlays tem buffer de aproximadamente 4 KB por item. Dividir conteúdo longo em nós ligados, sem perder evidência. Verificar retorno integral; conteúdo truncado é falha de recuperação.

## Especificação recuperável e ligação com código

`sync_horizon_spec` já indexa o corpo Markdown sob títulos em nós `Section`; `search_graph` com o horizonte ativo pode recuperar esses trechos como conteúdo dos overlays. Isso prova armazenamento de texto, mas não uma cópia fiel automática do arquivo: o parser não preserva a hierarquia dos títulos, ignora texto antes do primeiro título e limita cada linha lida. Para que uma sessão nova reconstrua uma especificação sem abrir o arquivo físico, grave SEMPRE uma representação completa e ordenada de cada revisão material no horizonte, mesmo quando não solicitada explicitamente.

Use um nó `FractalArtifact` para identificar o artefato e nós `FractalArtifactPart` para suas partes. O nó raiz registra `artifact_id`, nome/caminho humano, revisão, autoria, estado de completude, quantidade/ordem das partes e SHA-256 dos bytes UTF-8 exatos. Cada parte guarda o cabeçalho com nível, conteúdo exato, índice, total, hash da parte e política de junção. Inclua preâmbulo, títulos e quebras finais na sequência reconstruível. Relacione raiz/partes com `CONTAINS_PART` e partes sucessivas com `NEXT_PART`; mantenha nós semânticos separados para telas, componentes, tokens visuais, dimensões/unidades, estados, interações, responsividade, acessibilidade e critérios quando materialmente relevantes. Preserve cada valor como declarado, hipótese ou desconhecido, com autoria e evidência; não converta sugestão visual do agente em decisão humana.

### Fidelidade à fonte

Quando o artefato recuperar uma conversa ou documento existente, valide a fidelidade antes da primeira escrita no horizonte. Um hash prova que os bytes preparados foram preservados; não prova que uma transcrição ou resumo foi copiado corretamente da fonte. Compare citações literais e caracteres semanticamente relevantes — acentos, pontuação, setas, aspas e grafias originais — com a fonte decodificada explicitamente como UTF-8. Preserve também a atribuição de falante conforme a fonte; se normalizar rótulos ou timestamps, identifique isso como transformação editorial e não use `?` como marcador de substituição. Se o trabalho for uma síntese, rotule-a como síntese/reconstrução concisa e verifique as afirmações materiais contra a fonte. Não normalize silenciosamente uma grafia original; preserve-a na citação e explique separadamente qualquer forma normalizada. Corrija discrepâncias antes de criar ou sincronizar o horizonte.

Prepare o Markdown completo e ordene as partes antes da primeira escrita. Grave os bytes UTF-8 exatos no arquivo humano e calcule seu SHA-256 antes de chamar `create_horizon`; use leitura binária e `hashlib.sha256(Path(...).read_bytes())` para não depender da codificação do terminal ou da normalização de quebras de linha. Registre no nó raiz o hash esperado, tamanho em bytes, total/ordem das partes e regra de junção. Cada parte deve reconstruir a sequência integral do arquivo, incluindo preâmbulo, títulos e LF final. Só então crie os nós/arestas e sincronize o envelope correspondente com `sync_horizon_spec`. Não dependa de uma atualização posterior por URI: o schema de `create_horizon` pode aceitar os nós no primeiro envio, mas atualização no mesmo URI só deve ser usada quando o contrato vivo a garantir explicitamente. Se o hash ainda não puder ser calculado, marque o artefato como incompleto e não declare validação integral.

Cada payload continua abaixo de 3000 bytes UTF-8 e cada linha JSON dentro do limite de compatibilidade observado. Após sincronizar, uma conexão nova deve recuperar todas as partes via `search_graph`, ordená-las e reconstruir os bytes do artefato. A prova integral exige igualdade entre bytes/hash do arquivo, reconstrução MCP e digest esperado guardado no nó raiz. A sessão leitora não abre o arquivo; o escritor compara posteriormente os resultados e registra a igualdade. Se partes, ordem, total ou hash não corresponderem, declarar recuperação incompleta. O arquivo humano permanece como cópia legível/versionada; a recuperação do agente pelo MCP não depende dele.

Não crie um horizonte filho só para expressar que código implementa uma especificação. Quando um nó de código já existir e estiver indexado, use sua URI real e relacione-o ao nó de especificação (`IMPLEMENTS` ou `REALIZES`); os dois nós podem estar no mesmo horizonte ou em horizontes diferentes. Não imponha uma localização da aresta antes de conhecer o schema e a validação de endpoints disponíveis: registre-a onde a ferramenta e o grafo conseguem persistir e recuperar os dois extremos. Se o código só está planejado, registre o caminho pretendido como plano e aguarde a indexação antes de afirmar ou ligar um nó de código existente. Quando os nós estiverem em horizontes diferentes, consulte os IDs retornados pelo MCP e valide a aresta e ambos os endpoints no conjunto de horizontes ativos; não presuma que o validador estrito resolva dependências entre overlays. A relação semântica só está confirmada quando o nó de código, o nó de especificação e a aresta são recuperados.

Um recorte de implementação pode usar o mesmo horizonte da ideação; um horizonte separado só é uma opção quando limites, autoria, autorização ou retomada realmente precisarem ser independentes. Horizontes atuais não têm metadado nativo de hierarquia. Se registrar procedência ou relações entre recortes, guarde IDs/URIs reais em nós e payloads explícitos; não trate `based_on_seq` como ID de horizonte.

## Gatilho, revisões e concorrência

Avaliar o delta desde a última revisão confirmada a cada turno, antes da resposta. Sincronizar quando a falta do delta puder mudar a reconstrução do contexto, evidência, decisão, consequência, próximo passo, estado de uma questão, vínculo ou etapa/ciclo. Eventos típicos são: delimitação/correção contextual; declaração ou evidência nova; hipótese, pergunta, sombra ou consequência que o agente efetivamente apresentou como pertinente; aceitação, rejeição, contestação, decisão, adiamento ou mudança de destino pelo humano; ligação nova com o contexto maior; compressão, integração, pausa ou encerramento solicitado. Uma revisão agrupa os eventos materiais acumulados do turno.

Não sincronizar por mera paráfrase, repetição, cortesia, elaboração sem mudança semântica ou alternativas que ficaram apenas como exploração interna. Não perder um item relevante só porque ainda não foi aceito: registrar a proposta do agente como `HIPOTESE`/`EM_EXAME`, com autoria e limites. Se a fala humana admite leitura como decisão ou exploração, não escolher a mais forte; preservar como `EM_EXAME` até a diferença ser necessária para o próximo passo.

Na revisão, atualizar `registro.md`, criar `revisoes/rNNN.md` e acrescentar as operações reais ao log. Incluir estado acumulado, IDs e relações necessários, sem apagar versões ou sombras anteriores. Sincronizar a mesma revisão e verificar conteúdo e relações como descrito abaixo. Só depois da confirmação marcar `SINCRONIZADO_CONFIRMADO`. O agente pode gravar sem pedir aprovação para cada operação porque autoria/estado ficam explícitos; deve comunicar a síntese do delta e o resultado real. Em falha, registrar `PENDENTE`, `PARCIAL` ou `INDISPONIVEL` e continuar limitado pelo contrato.

O valor de etapa descreve a atividade atual, não é indicador de qualidade nem sequência obrigatória. Registrar mudança de etapa junto com revisão material; manter a anterior no histórico. `SOLIDIFICADO` só após contexto/limites/fontes ou lacunas legíveis e memória inicial recuperável. `COMPRESSAO` corresponde a síntese retomável; `DELIBERACAO`, exame de questão; `EXPANSAO`, exame de ligações maiores; `INTEGRACAO`, atribuição de destinos/estado de continuidade. Ciclo da conversa é outro eixo.

## Revisões e concorrência

IDs de afirmações/revisões são estáveis e versionados (`D-01-R1`, `D-01-R2`); revisão nova aponta à anterior por `SUPERSEDES`. Nunca eliminar sombras, pais ou alternativas da memória só porque houve compressão. Incluir no bloco os nós/arestas necessários para a memória acumulada. Antes de escrever num horizonte retomado, recuperar seu conteúdo; se houver outro escritor ativo, combinar responsabilidade explicitamente. Não alegar isolamento transacional da conversa ou locks que não existem.

O compilador substitui nós pela mesma URI, mas não remove automaticamente todos os nós/arestas semânticos omitidos na nova revisão. Não usar omissão como exclusão. Estados de descarte/refutação ficam registrados em revisão nova. Ressincronização pode restaurar PROPOSED nativo; não usá-la para apagar contestação real.

## Confirmar e recuperar por outra sessão

Depois de cada sincronização:

1. Conferir contagens de nós, relações e seções compiladas; contar nós explícitos separadamente de Section.
2. Executar `validate_scope_horizon` com conectividade estrita; isso é verificação estrutural, não prova da verdade de decisões.
3. Executar `search_graph` com `project`, `active_horizons=[id]`, paginação e formato JSON. Conferir URIs e payloads esperados no retorno de overlay, não só linhas do grafo base.
4. Executar `query_graph` com o mesmo horizonte e uma consulta válida do projeto. Conferir nós e arestas no retorno de overlay. Não presumir que filtros/agregações Cypher do grafo base sejam aplicados integralmente ao horizonte.
5. Só então marcar SINCRONIZADO_CONFIRMADO e revisão confirmada; em falha registrar o estado real e manter o impedimento.

Outra sessão recebe apenas projeto e horizon_id e deve recuperar contexto, uma decisão, uma pergunta aberta, consequência, proveniência, estados e suas ligações sem ler Markdown. Na primeira prova o ID pode ser fornecido explicitamente; descoberta autônoma precisa de catálogo MCP real e é validada separadamente. Não chamar list_projects de catálogo de temenos.

Quando o ID não for fornecido, descobrir o schema de `list_horizons` e consultar com o nome exato do projeto. Paginar por `offset`/`limit` até `has_more=false`; `partial=true` denuncia catálogo incompleto. O padrão lista armazenamento ACTIVE; `status=ALL` inclui outros estados nativos. Compare `context_preview`, `context_uri` e associação; se o preview não identifica o assunto, consulte por MCP o conteúdo e as relações de cada candidato usando apenas IDs devolvidos pelo catálogo. Não use data/horário como critério de identidade. `explicit_project` indica vínculo persistido; `legacy_node_uri` indica associação obtida da autoridade exata das URIs antigas. Se a inspeção ainda deixar mais de um recorte pertinente, apresente opções e peça seleção. O catálogo não ativa, reabre, promove nem garante retenção. Após a seleção, recupere conteúdo e relações com `active_horizons` usando somente o ID MCP escolhido. Se a ferramenta não existir ou o catálogo for parcial, registre a limitação; não procure IDs em arquivos nem adivinhe nomes.

`search_graph` atualmente transporta conteúdo dos nós; `query_graph` transporta seus IDs/tipos e relações separadamente do resultado base. Paginar e conferir a cobertura. Não usar get_code_snippet como substituto presumido para leitura de payload do horizonte.

## Encerramento e retenção

Registrar PAUSADO/ENCERRADO no nó do temenos e sincronizar antes da saída. Não descartar o horizonte nem promover deliberação ao grafo base só para fechar a conversa. O estado de armazenamento ACTIVE pode coexistir com ciclo ENCERRADO; explicar a distinção.

O coletor atual pode remover horizontes de proprietário morto quando sua idade ultrapassa 3600 segundos. `client_pid` deve representar processo real; não usar 0/PID fictício para escapar da retenção. Registrar proprietário/base reais retornados. Outro cliente consultar com sucesso imediatamente não comprova memória durável após TTL, reinício do daemon ou atualização do aplicativo.

Prova mínima aprova recuperação paralela e por cliente novo; recuperação durável e descoberta autônoma são gates separados. Se falharem, a lacuna é do suporte CBM, não pode ser compensada dizendo que Markdown está salvo.

## Geração determinística obrigatória

Usar `scripts/build_horizon_document.py` desta skill. O documento humano e o envelope são arquivos diferentes: o envelope contém partes exatas do documento e o grafo semântico; não tentar embutir recursivamente o hash/partes do envelope nele mesmo. Entrada: Markdown UTF-8 exato e JSON semântico com nodes/edges. Exemplo:

```text
python <skill>/scripts/build_horizon_document.py --input revisoes/r005.md --graph revisoes/r005.graph.json --project <projeto-real> --path docs/temenos/<id>/revisoes/r005.md --artifact-id ART-R005 --output revisoes/r005.cbm.md
```

O helper preserva todos os bytes, incluindo preâmbulo, linhas longas, BOM e CRLF; divide conteúdo conforme o limite da linha JSON já escapada, valida payloads e rejeita excesso sem truncar. Ligar o FractalArtifact aos nós semânticos com DESCRIBES e ao recorte/revisão. Sincronizar usando file_path do envelope. Conferir expected_nodes/expected_edges retornados pelo helper contra o compilador. Sucesso de escrita, Sections ou hash local isolados não bastam.

Uma conexão leitora nova recupera raiz e partes exclusivamente por MCP, valida quantidade, índices contíguos, hashes de cada parte, tamanho e hash integral. Recuperar também CONTAINS_PART/NEXT_PART e as relações semânticas; conferir todos os nós esperados. O escritor compara depois os bytes reconstruídos com o arquivo humano. Hash igual demonstra transporte fiel, não extração semântica completa nem veracidade do texto; revisar separadamente afirmações/decisões e suas ligações. Logs posteriores ficam fora da revisão imutável testada.
