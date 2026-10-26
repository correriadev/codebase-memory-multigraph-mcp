# Revisão cruzada e verificação da entrega

Data: 2026-10-23. Escopo: refinamento integral dos 32 PBIs, cinco frentes, documentos 001–008 e registros de decisão/orientação. Este registro verifica documentos; não é resultado de testes do produto nem aprovação do ADR-005.

## Autoria e revisão independente

Todos os subagentes usaram `gpt-6-luna`, effort `xhigh`, conforme solicitado. O CTO carregou orientação e evidência compartilhadas antes da delegação; até três agentes trabalharam simultaneamente. A independência aqui significa autor diferente do recorte revisado; não demonstra diversidade de famílias de modelo ou independência estatística das avaliações.

| Frente / artefato | Autor | Revisor | Registro e parecer |
|---|---|---|---|
| 001/002 estratégico | strategic | CTO | Ordem de selagem/verificação, ramos terminais e definição de Tradição corrigidos antes das frentes |
| 01 execução/recuperação | execution | authority | [Revisão execução/evolução](reviews/authority-review-execution-evolution.md): aprovado documentalmente após correções |
| 02 planejamento/eventos | planning | execution | [Revisão planejamento/autoridade](reviews/execution-review-planning-authority.md): aprovado documentalmente após correções |
| 03 autoridade/decisão | authority | execution | Mesmo registro; caminho mínimo separado das extensões F06 |
| 04 evolução supervisionada | execution | authority | Mesmo registro execução/evolução; qualidade separada de capacidade de ativação |
| 05 observabilidade/valor | strategic | planning | [Revisão observabilidade](reviews/planning-review-observability.md): correções de cobertura e dependências condicionais |
| 003/004/008 consolidados | CTO | planning + auditoria final CTO | Revisão cruzada identificou gates indevidos de F07; consolidado regenerado e auditado após correções |

As evidências `path:linha` nos pareceres apontam para o momento da revisão. Os links levam aos arquivos; as alterações posteriores de consolidação podem deslocar a numeração de linhas. Parecer documental não aceita uma capacidade futura nem transforma Q04–Q08 em resposta humana.

## Principais correções reconciliadas

| Achado | Correção final | Evidência de destino |
|---|---|---|
| Candidato selado antes de sua produção | Builder produz/persiste → host captura/congela/sela → evidência e veredicto | 001; frente 01 ER-R04/ER-S01; frente 03 AD-R01 e cenários |
| Journal mínimo acoplado a checkpoint F05 | ER-07 mínimo; ER-10 diagnóstico pré-F05; ER-14/15/09/16 extensão posterior | Frente 01 tasks/all_requires; 003/007 |
| Promoção mínima aguardava deep link/decisão humana F06 | AD-07/09 caminho mínimo; AD-08/10/11 extensões próprias | Frente 03 task graph; 003 |
| Exclusividade comparava só chaves iguais | Conflito por sobreposição de arquivo/range/âncora e aliases de filesystem, com fencing no boundary | C06; frente 02 regras e cenários PE |
| Ciclos de dependência descartavam planejamento útil | SCC bloqueada para resolução explícita; preservar componentes independentes sem inventar ordem dentro do ciclo | Frente 02 PE-R04 e cenário de SCC |
| TOCTOU pouco explícito | Cenário de mudança entre revalidação final e chamada; sem exclusão comprovada não despachar | C04; frente 03 cenários AD |
| Evento de incerteza confundia fato com escalonamento | Separar evento de efeito incerto do escalonamento material; aliases vinculados aos eventos canônicos de 001 | Frentes 01/02/03/04 |
| Telemetria completa sem task própria | OV-07 integra cobertura das capacidades declaradas; fontes futuras exigem revalidação | Frente 05; C08; 003 |
| Medição S1 aguardava F07/S5 | Fontes condicionais por capacidades/protocolo; medição S1 continua restrita a seu escopo | Frente 05 OV-S15/16; 003 requires_by_capability/protocol; 007/008 |
| Cobertura parcial poderia ser confundida com PBI-31 integral | Conclusão global exige todos os produtores da visão F01–F08; excluir fonte futura só limita comparação atual | C08; 007; frente 05; 008 |
| Predecessor principal omitia outros requisitos | Consolidado une all_requires, dependências adicionais e crosscuts; adapters por alvo e fontes condicionais permanecem separados dos requisitos conjuntivos | JSON 003 |

## Checklist de conclusão do refinamento

| Requisito aprovado | Evidência verificável |
|---|---|
| Fundamentação pelo grafo e fonte, com limites | ORIENTATION: projeto/geração/coverage, consulta exata, paginação e fallback; 006: fatos/gaps e limites estáticos |
| Visão completa preservada | 001/002 e tabela 008: 32 linhas, 32 PBIs únicos; nenhuma funcionalidade excluída |
| Cinco frentes e contratos comuns | Cinco suplementos; C01–C08; propriedade de dados e integrações em 002 |
| Desenho tático por projeto | 003 com estrutura/tipos/ports/usecases/eventos/persistência, cinco suplementos e tasks JSON |
| Cenários normais, falhas, concorrência e recuperação | 004 indexa fontes únicas Unit/Integration/Functional e define sete cenários transversais |
| Rastreabilidade de cada PBI | 008: frente/aceite, regras, contratos, cenários, tasks, slice e dependências |
| Decisões e lacunas explícitas | DECISIONS Q01–Q08; D01–D05 e INV-01–08; 006 capacidades; ADR-005 PROPOSED |
| Sequenciamento e spike definidos | 007 S0–S6; cinco tasks SP no JSON 003; nenhum experimento executado nesta entrega |
| Revisão por autor diferente | Três pareceres vinculados acima e correções reconciliadas |
| Sem implementação | Arquivos desta entrega são Markdown dentro da pasta autorizada; código, entradas e ADRs não foram editados |

## Verificação final

Estado anterior: **PASS estrutural/documental**. A revisão posterior com Ponytail encontrou quatro lacunas semânticas; esse PASS não comprovava aprovação do spike, integração do runtime nem segurança da reversão. As correções estão aplicadas abaixo e sua auditoria final é registrada após a execução das checagens.

- **32/32 PBIs** únicos na matriz, ligados a regras, contratos, tasks e cenários.
- **104 regras de frente**, **64 tasks** (59 das frentes e cinco do spike futuro), **126 cenários** (119 das frentes e sete transversais).
- JSON parseado, IDs numéricos únicos/sequenciais, estimativas/escopos/aceites dentro dos contratos. Dependências conjuntivas acíclicas, em ordem topológica e sem inversões de slice. Nenhuma task S1 depende de slice futuro.
- Requisitos adicionais das tabelas all_requires e crosscuts incorporados. Adapter de evolução condicionado ao alvo; fontes de telemetria/medição condicionadas a capacidade/protocolo, com extensão F07/S5 explícita.
- Títulos, descrições, escopos, aceites e PBIs de tasks devem conferir com as fontes. Os 119 cenários das frentes têm definição canônica com âncora estável; 004 aponta para eles sem copiar os corpos. Os sete transversais têm sua única definição em 004. Todos devem manter Given/When/Then.
- Links locais e âncoras de cenários conferidos; sem marcadores de merge, placeholders de template ou caractere de substituição de encoding. Snippets ilustrativos das frentes respeitam quatro linhas.
- PBB e BRIEFING conservam os tamanhos observados antes da delegação (30.134 e 36.213 bytes). A equipe não os editou. ADR-005 continua PROPOSED e Q04–Q08 continuam Deferred.
- O status Git final preserva os mesmos caminhos preexistentes de alterações fora da pasta; não houve staging/commit. A validação direta dos arquivos ignorados complementa git diff --check, que sozinho não os cobre.

Um parecer preliminar foi inicialmente salvo em `reviews/` na raiz. A cópia foi preservada como [rascunho arquivado e superado](reviews/planning-review-observability.initial-draft.md) dentro da pasta autorizada; o parecer vigente é `reviews/planning-review-observability.md`. O rascunho não é fonte de requisitos nem parecer final. O conjunto contém 20 artefatos finais novos e esse rascunho histórico, além das duas entradas preservadas.

## Correções da revisão Ponytail

| Achado | Correção | Condição de verificação |
|---|---|---|
| Spike concluído podia ser confundido com aprovado | Um único contrato JSON de gate S0_APPROVED cobre S1–S6; SP-05 pode concluir com reprovação sem liberar produção | Casos negativos de FAIL/inconclusão/indisponibilidade/ausência, ADR PROPOSED e configuração divergente; XT-S07 |
| Interfaces não entregavam o runtime concreto | ER-17 integra o primeiro host validado e a composição real; AD-03/ER-06 dependem dele | Grafo de tasks, aceite do adapter e ER-S24/25; prova real fica para implementação |
| Ativação/rollback podiam sobrescrever versão posterior | C08/EV-R22 exigem comparação/troca atômica no destino ou exclusão confiável por alvo cobrindo todos os escritores | Contratos e tasks EV-10/11; EV-S17/18 cobrem ativações concorrentes e rollback obsoleto |
| Cópias de cenários acumulavam divergências | Fonte única nas frentes, âncoras estáveis e 004 como índice; transversais continuam definidos em 004 | Unicidade das definições, destinos das referências e ausência de corpos copiados |

Resultado da nova auditoria: **PASS documental e de conformidade do contrato declarativo**. JSON, DAG e referências foram conferidos sobre os arquivos atuais: 32 PBIs, 64 tasks, 104 regras, 119 cenários canônicos de frente e sete transversais. As definições de cenário e suas âncoras são únicas; 004 contém zero corpos copiados e os 434 links locais verificados resolvem para arquivos/âncoras aplicáveis.

A checagem em memória avaliou o bloco JSON de gates com igualdade estrita de valor/tipo: 222 casos negativos cobrem critérios FAIL/INCONCLUSIVE/UNAVAILABLE/ausentes, booleanos inválidos, ADR PROPOSED e gate não suportado em S1–S6. O contraexemplo original — tasks S0 concluídas com cancelamento FAIL — retorna DENY; a combinação inteiramente válida satisfaz o contrato. Isso verifica o contrato exportado, não a implementação de um consumidor futuro.

O grafo confirma ER-17 como pré-requisito transitivo de AD-03 e ER-06, sem dependência futura em S1; tasks copiadas em 003 correspondem às fontes. EV-10/11 e EV-S17/18 especificam a guarda no destino e os casos concorrentes. PBB/BRIEFING conservaram seus hashes durante as correções. Nenhum resultado desta auditoria constitui teste de runtime, spike aprovado, CAS de destino demonstrado ou ADR aceito.

## Limites materiais que permanecem

Q04–Q08 permanecem Deferred. ADR-005 permanece PROPOSED. A entrega especifica o spike e critérios, sem executá-lo, aceitar ADR, instalar capacidades ou fixar defaults de negócio. O host/proxy, bloqueio de canais alternativos, cancelamento/medição, exclusão de mutações, anchors/seams, fontes de evento, descarte genérico, deep link exato e ativação/reversão real de skills exigem provas futuras. Se a solução exigir alterar core C, retornar ao operador para decisão de escopo antes de comprometer implementação.

Os comandos de validação desta etapa analisam documentos, não exercitam o produto C/TS. Não foram executados testes de runtime, spike, benchmark ou suíte nativa. Esta pasta está ignorada pela regra existente `docs/specs/*`; nada foi staged/force-added e nenhum commit foi criado.
