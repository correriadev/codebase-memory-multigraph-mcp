# 002 — Mapa de Contextos: Graph Engineering

**Base única do domínio:** [`001-problem-space.md`](./001-problem-space.md) — eventos, subdomínios e linguagem ubíqua. Os contratos canônicos C01–C08 estão em [`005-shared-contracts.md`](./005-shared-contracts.md); o estado conhecido das integrações está em [`006-capability-matrix.md`](./006-capability-matrix.md). Este mapa atribui propriedade e relações sem redefinir esses contratos ou promover capacidade parcial/não comprovada a fato. A organização de times não foi informada; os donos abaixo são responsabilidades sugeridas, não alocações confirmadas.

## 1. Contextos delimitados

| Contexto delimitado | Responsabilidade | Fronteira (fora) | Dono sugerido | Entidades e modelos centrais |
|---|---|---|---|---|
| **Execução** (F01/F05) | Conduz ciclos e iterações sob política explícita, coordena consumo e checkpoints, e registra resultados e incertezas de efeitos. | Não determina topologia nem compila tarefas; não decide risco ou promoção; não escreve diretamente no banco SQLite/C do CBM. Até F05, queda não retoma checkpoint automaticamente. | Equipe do pacote `cbm-orchestrator` (proposta; time formal não definido). | Raízes: `CicloDeExecucao`, `OrcamentoDoCiclo`, `CheckpointDeExecucao`. Modelos: `Iteracao`, `Tentativa`, `RegistroDeEfeitoExterno`, `RelatorioFinal`. |
| **Planejamento e Eventos** (F02/F03) | Traduz o subgrafo relevante em plano de tarefas dependentes e controla assinaturas, despacho, deduplicação e limites de eventos. | Não é fonte do grafo do código, não altera o conhecimento do CBM e não interpreta todo sinal do watcher como evento de negócio válido. A exclusividade de claims precisa ser provada neste pacote. | Equipe do pacote `cbm-orchestrator` (proposta; time formal não definido). | Raízes: `PlanoDeTrabalho`, `ClaimDeTrabalho`, `AssinaturaDeEvento`. Modelos: `Tarefa`, `Dependencia`, `EntregaDeEvento`, `RegraDeDespacho`, `SubgrafoAfetado`. |
| **Autoridade e Decisão** (F04/F06) | Aplica perfis e rotas de autorização propostos, vincula evidência e risco à versão candidata e registra escalonamento, decisão humana ou recusa de promoção. | Não executa raciocínio de autorização em LLM; não administra identidade geral do host nem oferece RBAC nativo completo; não pode dispensar evidência atual em razão de fast path ou aprovação humana. A barreira contra chamadas diretas ainda depende de isolamento validado. | Equipe do pacote `cbm-orchestrator` (proposta; time formal não definido). | Raízes: `VersaoCandidata`, `DossieDeEvidencias`, `DecisaoDePromocao`. Modelos: `VeredictoIndependente`, `AvaliacaoDeRisco`, `PerfilDePermissao`, `DecisaoDoOperador`, `PacoteDeDecisao`. |
| **Evolução Supervisionada** (F07) | Analisa auditorias preservadas, identifica padrões e governa propostas de alteração de Tradições do catálogo/temas do CBM, regras locais do Harness ou contratos de skill, cada qual com seu fluxo de avaliação, aprovação e ativação. | Não é dona do catálogo de temas nem altera automaticamente catálogos, skills, contratos ou core. Mantém proposta e registros de decisão/ativação/reversão. Skills deferred não têm admissão demonstrada; não reduzir a capacidade de evolução de temas a essa pendência. Cadência e retenção continuam Q06/Q07 Deferred. | Equipe do pacote `cbm-orchestrator` (proposta; time formal não definido). | Raízes: `LoteDeRetrospectiva`, `PropostaDeEvolucao`, `VersaoDeEvolucao`. Modelos: `PadraoDeFalha`, `ResultadoDeNaoRegressao`, `RegistroDeAtivacao`, `RegistroDeReversao`. |
| **Observabilidade e Valor** (F08) | Persiste e projeta telemetria própria do orquestrador e calcula comparações sob referência e critérios previamente configurados. | Não autoriza ação, não substitui auditoria do ciclo e não fixa por conta própria retenção, referência ou limiar de sucesso. Q07/Q08 permanecem Deferred. | Equipe do pacote `cbm-orchestrator` (proposta; time formal não definido). | Raízes: `RegistroDeTelemetria`, `ExecucaoDeMedicao`. Modelos: `MetricaDeCiclo`, `IntervencaoHumana`, `ReferenciaComparavel`, `RelatorioDeValor`. |

### Propriedade de dados

- **Execução** é fonte do journal TypeScript: ciclos, orçamento consumido, tentativas, checkpoints, resultados e efeitos externos conhecidos ou incertos. **Observabilidade e Valor** mantém sua própria telemetria e suas medições, correlacionadas por identificadores; não reinterpreta o journal como estado de conhecimento.
- **Planejamento e Eventos** mantém planos, claims e entregas de eventos. O **CBM** permanece fonte do conhecimento persistente e do estado de seus territórios; a camada TS consulta e traduz essa informação via MCP. Claim de trabalho exclusivo é uma garantia proposta no domínio TS, não semântica atribuída a claim Union.
- **Autoridade e Decisão** é fonte da versão candidata avaliada, dossiê, risco e decisões de autorização dentro do orquestrador. A promoção do grafo continua operação do CBM por seu contrato exposto; o pacote não escreve diretamente em SQLite nem substitui o gate C.
- **Evolução Supervisionada** possui propostas e seus registros de avaliação/aprovação/ativação/reversão; não é dona dos artefatos alvo. Tradições pertencem ao catálogo versionado de temas do CBM; regras locais do Harness e contratos de skill são alvos distintos. O contexto consome cópias ou referências auditáveis dos registros de execução; a fonte original e evidência necessária não desaparecem antes do consumo.

**Contratos canônicos por contexto:** Execução — C02/C03/C07; Planejamento e Eventos — C06; Autoridade e Decisão — C01/C04/C05; Evolução Supervisionada — C07/C08; Observabilidade e Valor — C02/C03/C08. Campos, estados, transições, permissões e regras de retenção ficam definidos em C01–C08.

## 2. Mapa de Contextos e integrações

As relações internas usam contratos versionados do domínio; formatos detalhados pertencem aos contratos compartilhados, não a um schema presumido nesta fase. Nas relações externas, a seta indica o provedor de modelo/capacidade para o contexto consumidor. O contexto consumidor envia comandos ao provedor quando necessário e traduz respostas por ACL. Os contratos abaixo são propostas sujeitas a mapeamento de capacidades e validação.

### Relações entre contextos internos

**[Planejamento e Eventos] → [Execução]**

- **Padrão:** Published Language.
- **Direção:** upstream → downstream.
- **Contrato/dados:** plano, dependências, tarefa liberada, atribuição e claim (C06); Execução devolve estado de tarefa para reconciliação pelo plano (C02).
- **Justificativa:** Execução deve consumir tarefas sem consultar ou modificar os agregados de planejamento diretamente. IDs e transições são publicados; não se compartilha armazenamento.

**[Execução] ↔ [Autoridade e Decisão]**

- **Padrão:** Published Language.
- **Direção:** bidirecional.
- **Contrato/dados:** Execução publica a referência do candidato e evidências sob C04; Autoridade é dona da identidade canônica da versão conforme C01 e publica risco, escalonamento, decisão humana e autorização ou recusa sob C04/C05.
- **Justificativa:** ciclo e decisão têm fontes de verdade distintas. Uma correção cria nova versão; mensagens tardias de autorização não podem liberar versão diferente.

**[Execução] → [Evolução Supervisionada]**

- **Padrão:** Published Language.
- **Direção:** upstream → downstream.
- **Contrato/dados:** eventos e referências auditáveis de falha, recusa, descarte e incerteza (C07), com status de consumo e proposta de evolução (C08).
- **Justificativa:** evolução lê a evidência produzida por execução sem possuir ou apagar seu journal. A retenção fica configurável, mas o prazo segue Q07 Deferred.

**[Execução] → [Observabilidade e Valor]**

- **Padrão:** Published Language.
- **Direção:** upstream → downstream.
- **Contrato/dados:** veredictos por iteração, custo, tokens, tentativas, orçamento, encerramentos, checkpoints e intervenções correlacionados ao ciclo (C02/C03/C07/C08).
- **Justificativa:** a telemetria vem do próprio orquestrador e dá suporte ao relatório final e às métricas de F08 sem conceder autoridade operacional à projeção.

**[Planejamento e Eventos] → [Observabilidade e Valor]**

- **Padrão:** Published Language.
- **Direção:** upstream → downstream.
- **Contrato/dados:** evento recebido, regra correspondida, perfil despachado e duplicação/recursão suprimida (C06).
- **Justificativa:** medir tempestades e cobertura de gatilhos sem transformar watcher ou projeção em fonte de verdade de negócio.

**[Autoridade e Decisão] → [Observabilidade e Valor]**

- **Padrão:** Published Language.
- **Direção:** upstream → downstream.
- **Contrato/dados:** fatores de risco, estado de escalonamento, decisão, recusa e versão avaliada (C01/C04/C05/C08).
- **Justificativa:** F08 precisa contar decisões e intervenções mantendo decisão como dado autoritativo do contexto de origem.

**[Evolução Supervisionada] → [Observabilidade e Valor]**

- **Padrão:** Published Language.
- **Direção:** upstream → downstream.
- **Contrato/dados:** período analisado, registros consumidos, propostas, avaliação de qualidade, aprovação, ativação e reversão (C07/C08).
- **Justificativa:** medir a evolução aplicada sem confundir proposta com capacidade habilitada nem resultado com aprovação.

### Fronteiras com sistemas externos

| Provedor → consumidor | Padrão e direção | Dados ou comandos | Limite e condição |
|---|---|---|---|
| **CBM/MCP → Planejamento e Eventos** | Anti-Corruption Layer; upstream CBM, downstream TS | Consultas de topologia, comunidades, dependências e âncoras; possíveis sinais recebidos para despacho. | Adaptador valida origem, cobertura, geração e semântica. O watcher reindexa; isso não equivale a uma fila de eventos de negócio. Claims Union não provam exclusividade AST. |
| **CBM/MCP → Autoridade e Decisão** | Anti-Corruption Layer; upstream CBM, downstream TS | Consulta a territórios/evidências e solicitação de operação determinística de promoção; resposta de admissão ou recusa. | O core C permanece inalterado e é consumido via MCP. Mapear capacidades e pré-condições; não alegar RBAC completo nem que `promote_horizon` sozinho prova a invariante atual. `CBM_ALLOW_LEGACY_PROMOTION=1` é incompatível com a garantia desejada. |
| **Host/runtime de agentes → Execução** | Anti-Corruption Layer; upstream host, downstream TS | Iniciar/cancelar sessão, isolar perfil, medir consumo e recuperar; resultados e efeitos de tools. | Contrato mínimo é Q04, Deferred; ADR-005 segue PROPOSED até spike. Sem medição/limites válidos, não iniciar autonomia; efeito externo incerto não é repetido às cegas. |
| **Harness e verificador Adversarial → Autoridade e Decisão** | Anti-Corruption Layer; upstream verificador, downstream TS | Veredicto, evidências, versão testada e indicação de inconclusão/indisponibilidade. | Evidência declarada no manifest não equivale à execução observada. Parecer ausente, inconclusivo ou de outra versão implica não promoção. A diversidade de família de modelo é hipótese, não garantia. |
| **Autoridade e Decisão → graph-ui** | Anti-Corruption Layer; downstream TS envia navegação à UI externa | Pacote de decisão fornece alvo e contexto para abrir nó/aresta crítica. | Deep link exato ainda não foi comprovado. Se não houver navegação verificável para o alvo, PBI-25 permanece bloqueado; a UI não decide autoridade nem promoção. |
| **Catálogo de temas/Tradições do CBM → Evolução Supervisionada** | Anti-Corruption Layer; upstream catálogo, downstream TS | Ler item, versão e estado do tema alvo para fundamentar uma proposta de evolução. | Catálogo versionado e tools normativas foram localizados, mas a matriz 006 classifica a capacidade como parcial; validar contrato real, autorização do Operador e revisão alvo antes de qualquer mudança. Tradição não é regra local do Harness. |
| **Evolução Supervisionada → Catálogo de temas/Tradições do CBM** | Anti-Corruption Layer; downstream TS envia comando ao upstream | Após avaliação e aprovação, solicitar alteração/ativação versionada ou reversão do tema alvo. | A mutação é condicional à validação das tools e da autoridade. Não alterar store C diretamente; se o contrato não satisfizer a decisão aprovada, manter a proposta sem ativar. |
| **Harness/CBM → Evolução Supervisionada** | Anti-Corruption Layer; upstream Harness/CBM, downstream TS | Resultado de não-regressão para proposta de tema, regra local do Harness ou contrato de skill. | Bundle/evaluate são parciais e não executam, por si, todos os testes requeridos. Usar evidência de execução vinculada à versão. `constellation-individuator` e `level-graduator` seguem deferred; admissão real de skills não está demonstrada. |
| **Runtime de referência → Observabilidade e Valor** | Anti-Corruption Layer; upstream referência, downstream TS | Resultado, custo e regressões de execução comparável. | Escopo/aceite deve ser equivalente; referência e limiares precisam ser definidos antes de medir e seguem Q08 Deferred. |

O journal TypeScript e os territórios/grafo do CBM são stores separados. Não há transação distribuída implícita entre ambos: a integração deve usar IDs correlacionáveis, idempotência, registro de divergências e reconciliação. A integração não escreve no SQLite do core C. Outbox/inbox e deduplicação de eventos são propostas a confirmar no desenho de contratos.

## 3. Destaque do Core Domain

**Contexto:** Execução  
**Razão:** Outer Loop dentro de orçamento, com parada objetiva, evidência e retomada idempotente quando F05 existir, é o mecanismo de autonomia controlada.  
**Investimento:** agregados de ciclo, orçamento e checkpoint com transições explícitas; idempotência e contabilidade de chamadas em voo; registro de efeito incerto; semântica de crash preservada por fase de entrega.

**Contexto:** Planejamento e Eventos  
**Razão:** converter estrutura real do projeto em dependências, tarefas e verificações de eventos dá ao produto sua ligação distintiva entre grafo de conhecimento e grafo de execução.  
**Investimento:** limites de planos e claims, validação de cobertura/freshness, precedência de dependências, integração de falha parcial e deduplicação limitada por orçamento; evento do watcher só entra após contrato explícito.

**Contexto:** Autoridade e Decisão  
**Razão:** geração, verificação independente, autorização determinística e atenção humana precisam convergir numa única invariante por versão candidata.  
**Investimento:** versão candidata imutável, dossiê ligado à versão, rejeição fail-closed, gate fora do LLM, decisão humana com efeito operacional e prova de que chamadas diretas proibidas são recusadas. A prova de isolamento é pré-requisito, não consequência deste desenho.

**Supporting:** Evolução Supervisionada consome registros e evidências produzidas pelos Core; Observabilidade e Valor mede o comportamento do conjunto. Ambos exigem qualidade, proveniência e limites próprios, mas não substituem invariantes de execução ou autoridade.

## 4. Decisões arquiteturais desta definição

**Decisão:** modelar o orquestrador como pacote TypeScript novo, consumindo CBM por MCP e separado do core C/SQLite.  
**Contexto:** PBB Q02 foi respondida pelo operador; o core C determinístico permanece inalterado.  
**Consequências:** preserva a fronteira de dados e evolução do CBM; exige ACL e contratos de capacidade. Não permite inferir que toda capacidade nomeada no briefing exista nem satisfaz Q04 por si só.

**Decisão:** cada contexto possui seus dados; o journal de execução TS não é o grafo de conhecimento CBM nem usa transação distribuída presumida.  
**Contexto:** ciclo/checkpoint, território e estado consolidado têm semânticas diferentes, e falhas parciais podem ocorrer entre MCP, host e journal.  
**Consequências:** permite persistência independente, correlação e reconciliação; aumenta o trabalho de consistência e recuperação. A separação de journal segue C07; divisão SQLite/WAL e protocolo exato continuam propostas D03, sem efeito no core.

**Decisão:** candidato e evidência seguem C01/C04, e toda rota de promoção aplica a mesma invariante de evidência independente verde para a versão atual. Gatekeeper é determinístico, fora do LLM.  
**Contexto:** F04/F06 compartilham a invariante PBB; aprovação humana e fast path não podem autorizar versão alterada.  
**Consequências:** correção reabre avaliação e impede promoção por evidência obsoleta; captura e comparação de revisão/contexto usam os contratos canônicos. Não se presume que as tools existentes expressem ou imponham todas as pré-condições.

**Decisão:** autorização por perfil segue C05 e deve ocorrer numa fronteira mediada e verificada contra chamada direta; o modelo não presume RBAC nativo completo nem isolamento já demonstrado.  
**Contexto:** `restricted`, claims Union e ocultar tools não bastam para a garantia PBI-15/16; ADR-005/Q04 segue PROPOSED/Deferred.  
**Consequências:** Builder não pode alcançar promoção por caminho alternativo; runtime e host ficam condicionados ao spike de cinco capacidades. Sem prova, autonomia de promoção fica bloqueada e não se amplia silenciosamente o core C.

**Decisão:** sinais do grafo entram pelos contratos C06 de eventos, com deduplicação e controle de propagação/orçamento; auditoria permanece ligada à origem.  
**Contexto:** watcher é reindexador, não fila de eventos de negócio; retrigger e temporalidade ainda precisam de desenho.  
**Consequências:** reduz tempestade e facilita replay/reconciliação; exige estado de entrega, limites e contratos de origem. Outbox/inbox D05 é proposta, não capacidade presente.

Q04, Q05, Q06, Q07 e Q08 permanecem **Deferred**. ADR-005 permanece **PROPOSED** até o spike. O slice documentado é PBI-01–05, PBI-16 e telemetria mínima de PBI-31, condicionado a contrato de execução e mapeamento de capacidades. Até F05, crash encerra o ciclo e exige diagnóstico e novo ciclo desde o objetivo, sem retomada automática; o modelo de contexto não altera essa fronteira.
