# Memória CBM do temenos — protocolo experimental 0.2

## Dois acessos à mesma memória

O humano acessa `registro.md` e revisões Markdown; o agente recupera conteúdo e relações pelo MCP. Ambos preservam os mesmos IDs, contexto, autoria, evidência e estados. O horizonte não é opcional. Um arquivo gravado sem sincronização confirmada é PENDENTE/PARCIAL, nunca sucesso equivalente.

Antes de chamar uma tool, descobrir seu schema real. O suporte verificado no código do CBM é `create_horizon`, `sync_horizon_spec`, `validate_scope_horizon` e consultas com `active_horizons`. A versão instalada pode diferir: guardar seus retornos e limitações.

## Abrir e sincronizar

Após seleção humana e verificação do contexto, criar um horizon_id novo (ASCII, letras/dígitos/underscore/hífen) e usar o ID retornado. Nunca usar `create_horizon` para testar a existência de um horizonte antigo: pode reativá-lo e alterar sua base. Na retomada usar apenas consultas.

Para grafo de projeto existente, `project` é o nome real retornado pelo catálogo. Outros contextos dependem de um substrato real compatível. Não criar um projeto fictício nem indexar repositório sem autorização só para vencer essa condição.

Guardar a geração/base efetivamente retornada; não fabricar `based_on_seq`. Não reutilizar credenciais, contratos ou `union_session_open` para simular um overlay. Sessão Union e horizonte de nós são mecanismos distintos.

Cada revisão material produz:

- `docs/temenos/<id>/registro.md`: síntese humana atual, com horizonte, revisão e estados de persistência.
- `docs/temenos/<id>/revisoes/rNNN.md`: versão preservada, com prosa e bloco JSON `tactical-spec` correspondente.
- `docs/temenos/<id>/cbm-log.md` (ou JSON): argumentos/retornos reais das operações, com revisão, hora disponível, erros e consultas de confirmação. Resumos de conversa devem ser identificados como resumos, não transcrição integral.

Enviar o mesmo Markdown a `sync_horizon_spec` com `horizon_id`, `project`, `file_path` relativo estável e `content`. O caminho deve identificar a revisão; `content` evita depender de acesso do servidor ao arquivo. Falha em qualquer representação fica explícita. Não presumir transação atômica arquivo/MCP.

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

Quando o ID não for fornecido, descobrir o schema de `list_horizons` e consultar com o nome exato do projeto. Paginar por `offset`/`limit` até `has_more=false`; `partial=true` denuncia catálogo incompleto. O padrão lista armazenamento ACTIVE; `status=ALL` inclui outros estados nativos. Comparar `context_preview`, `context_uri`, datas, proprietário e `association_source`, sem confundir uma prévia truncada com recuperação integral. `explicit_project` indica vínculo persistido; `legacy_node_uri` indica associação obtida da autoridade exata das URIs antigas. Se houver mais de um recorte pertinente, apresentar candidatos e pedir seleção; não escolher o mais recente silenciosamente. O catálogo não ativa, reabre, promove nem garante retenção. Após selecionar, recuperar conteúdo e relações com `active_horizons` usando somente o ID retornado pelo MCP. Se a ferramenta não existir, registrar limitação; não procurar IDs nos arquivos nem adivinhar nomes.

`search_graph` atualmente transporta conteúdo dos nós; `query_graph` transporta seus IDs/tipos e relações separadamente do resultado base. Paginar e conferir a cobertura. Não usar get_code_snippet como substituto presumido para leitura de payload do horizonte.

## Encerramento e retenção

Registrar PAUSADO/ENCERRADO no nó do temenos e sincronizar antes da saída. Não descartar o horizonte nem promover deliberação ao grafo base só para fechar a conversa. O estado de armazenamento ACTIVE pode coexistir com ciclo ENCERRADO; explicar a distinção.

O coletor atual pode remover horizontes de proprietário morto quando sua idade ultrapassa 3600 segundos. `client_pid` deve representar processo real; não usar 0/PID fictício para escapar da retenção. Registrar proprietário/base reais retornados. Outro cliente consultar com sucesso imediatamente não comprova memória durável após TTL, reinício do daemon ou atualização do aplicativo.

Prova mínima aprova recuperação paralela e por cliente novo; recuperação durável e descoberta autônoma são gates separados. Se falharem, a lacuna é do suporte CBM, não pode ser compensada dizendo que Markdown está salvo.
