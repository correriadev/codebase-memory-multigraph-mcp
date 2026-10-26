# Decisões e pendências — Graph Engineering

## Proveniência

Data: 2026-10-23. Projeto aplicável a todas as entradas: `codebase-memory-multigraph-mcp`. O plano aprovado autoriza prosseguir no refinamento e preservar lacunas; não converte recomendações em decisão humana. Requisitos de negócio vêm do PBB; mecanismos novos são propostas `model` até implementação/validação. Esta coleta antecede a geração de 001–004; não há entrevista humana adicional nesta execução autorizada.

| ID | Categoria | Questão | Recomendação | Resposta final / fonte | Evidência |
|---|---|---|---|---|---|
| Q01 | boundary | Visão inteira ou só protótipo? | Visão inteira, slice mínimo primeiro. | “Visão completa + slice inicial priorizado.” — human, PBB Q01 | PBB §9 Q01 |
| Q02 | architecture | Onde vive a orquestração? | Pacote TS novo consumindo MCP, sem escrita direta no store C. | “Novo pacote TS neste repositório.” — human, PBB Q02; linguagem TS é decisão humana registrada | PBB §8 e Q02 |
| Q03 | business | Agentes são personas? | Operador, Builder, Adversarial, Gatekeeper e Meta-Agent como atores. | “Humanos + agentes como personas.” — human | PBB Q03 |
| Q04 | integration | Qual contrato executa os agentes e garante isolamento/medição? | Refinar ADR-005 e especificar spike com cinco critérios; mediação de acesso ao MCP é proposta, não RBAC existente. | Unresolved — unresolved / Deferred; ADR-005 permanece PROPOSED | PBB Q04; ADR-005; ORIENTATION achados 1,3,6 |
| Q05 | performance | Quais defaults de orçamento e parada? | Exigir configuração explícita válida antes de iniciar enquanto faltam medições; reservar consumo pendente; números só após experimento. | Unresolved — unresolved / Deferred; exigência de configuração é proposta model, não default humano | PBB Q05 e PBI-02/03 |
| Q06 | business | Qual cadência da evolução? | Semanal como hipótese do briefing; suportar acionamento configurado sem fixar frequência de produto. | Unresolved — unresolved / Deferred | PBB Q06 |
| Q07 | data | Por quanto tempo reter auditoria? | Retenção configurável e proteção de registros ainda não consumidos na retrospectiva; sem prazo inventado. | Unresolved — unresolved / Deferred | PBB Q07 e PBI-22/27 |
| Q08 | acceptance | Qual referência e limiar comprovam valor? | Mesmo escopo/aceite para baseline simples e orquestração; limiares definidos pelo operador antes da medição. | Unresolved — unresolved / Deferred | PBB Q08 e PBI-32 |

## Escopo refinado e regras transversais

O escopo original permanece o PBB v3, integral. O refinamento separa mecanismo proposto, capacidade observada e prova pendente. Questões Q04–Q08 não são resolvidas por inferência. O desenho pode usar políticas parametrizadas, mas sua ativação depende dos valores e evidências exigidos.

- **INV-01:** nenhuma promoção sem evidência independente verde para a versão candidata atual; correção invalida evidência/aprovação antiga. Aprovação humana não contorna gate.
- **INV-02:** Builder não promove, inclusive por chamada direta ou acesso alternativo no ambiente controlado; bloquear entrega autônoma até isolamento ser demonstrado.
- **INV-03:** journal TS é fonte da execução; CBM é fonte do conhecimento; divergência não autoriza promoção.
- **INV-04:** resultado externo incerto não é reexecutado às cegas; tratar como ocorrido, registrar e escalar se material.
- **INV-05:** risco desconhecido/evidência incompleta não usa fast path.
- **INV-06:** descarte remove estado retomável sem destruir auditoria necessária nem alegar desfazer efeitos externos.
- **INV-07:** configuração sem orçamento medível/limites válidos não inicia autonomia; consumo em voo continua contabilizado.
- **INV-08:** evolução só é aplicada após qualidade provada, aprovação explícita, ativação versionada e caminho de reversão; habilitação de capacidade é condição separada.

## Propostas de arquitetura a verificar

| ID | Proposta model | Condição / risco | Responsável pela resolução |
|---|---|---|---|
| D01 | `packages/cbm-orchestrator/`, módulos TS com ports/adapters | Caminho proposto; não impõe framework/runner não escolhido | CTO + implementação futura |
| D02 | Proxy/host confiável autoriza por perfil e controla transporte/credenciais, sem expor conexão direta ao Builder | Processo separado sozinho não impede bypass; prova no spike antes de autonomia | Frente 3 + spike Q04 |
| D03 | SQLite journal TS separado, transações locais + reconciliação, sem transação distribuída presumida | WAL do journal não implica atomicidade com banco do core nem com tools externas | Frentes 1 e 5 |
| D04 | Versão candidata identifica conteúdo/patch e horizonte com revisões do grafo/regras/cobertura | Hash apenas do grafo não identifica código alterado; mudança de dependência invalida avaliação | Frentes 1,2,3 |
| D05 | Event outbox/inbox TS com deduplicação e limites; fonte MCP validada por adapter | Watcher/ingest_traces não oferecem por si fila confiável de eventos de negócio | Frentes 2 e 5 |

## Bloqueios de compromisso de implementação

- Spike Q04 e mapeamento de capacidades antes de comprometer tasks do slice; o refinamento descreve tasks condicionais, não autoriza executá-las.
- Se a barreira de autoridade ou uma capacidade essencial não puder ser implementada fora do core, retornar ao operador com gap comprovado e opções; não ampliar silenciosamente o escopo C.
- Deep link exato por nó/aresta, sinais de anomalia e admissão real de evolução exigem contrato/prova própria; ausência de capacidade mantém a entrega correspondente bloqueada, não substituída por comportamento mais fraco.
