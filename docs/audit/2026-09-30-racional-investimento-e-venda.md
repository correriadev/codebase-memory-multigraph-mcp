# Racional de investimento e venda — Codebase Memory MCP

Data: 30/09/2026. Perspectiva: investidor interessado na proposta e na sua transformação em negócio.

## 1. Conclusão de investimento

**Eu me interessaria pelo projeto como uma infraestrutura de contexto e memória para desenvolvimento com agentes de IA.** A entrada comercial mais convincente é ajudar agentes a entender sistemas existentes com menos exploração repetitiva. A oportunidade de longo prazo é conectar código, intenção e padrões institucionais em uma memória que a equipe consegue consultar, revisar e governar.

A frase que eu levaria ao mercado seria:

> **Dê aos seus agentes um mapa do sistema e uma memória das decisões da equipe. Entenda dependências, encontre o contexto relevante e evolua o software com evidências.**

O projeto reúne uma proposta técnica atraente, distribuição local e uma visão de produto mais ampla que a busca de código. Entretanto, a decisão de investir dependeria de comprovar adoção recorrente, melhoria em tarefas reais e disposição de pagamento. A documentação examinada não estabelece receita, retenção, clientes pagantes ou tamanho de mercado. Portanto, esta é uma tese de potencial, não uma avaliação financeira da empresa.

Meu interesse seria condicionado a dois marcos: demonstrar valor econômico no núcleo de exploração e demonstrar operação reproduzível nas capacidades de governança que sustentariam a expansão empresarial.

## 2. A dor que merece orçamento

A hipótese central é simples: dar a um agente acesso aos arquivos não significa dar a ele entendimento do sistema. Uma mudança localizada pode depender de chamadas em outros módulos, serviços externos e decisões que não aparecem no trecho editado.

| Dor do cliente | Consequência prática | Como a proposta responde |
|---|---|---|
| O agente reconstrói contexto a cada tarefa ou sessão | Leituras repetidas, consumo de contexto e maior espera | Grafo persistente consultável por símbolos, relações e arquitetura |
| Dependências estão espalhadas pelo repositório | Revisão difícil e possibilidade de ignorar efeitos indiretos | Rastreamento de chamadas e análise estrutural de impacto |
| Conhecimento fica concentrado nas pessoas | Onboarding lento e interrupções aos especialistas | Código navegável e registro de decisões arquiteturais |
| Código e intenção evoluem separadamente | Explicações desatualizadas e decisões sem justificativa acessível | Visão de conectar realização, intenção e evidência |
| Agentes aplicam convenções inconsistentes | Retrabalho e divergência dos padrões da equipe | Visão de bases temáticas versionadas e proveniência das escolhas |
| A empresa precisa controlar onde o código é processado | Fricção para adotar ferramentas externas | Motor de análise local, sem serviço hospedado obrigatório |

Essas dores devem ser confirmadas em entrevistas e pilotos. A força da venda aumenta quando o comprador consegue mostrar exemplos de retrabalho, tempo de investigação ou custo de exploração em sua própria operação.

O processamento local do CBM é relevante, mas não significa que todo o fluxo com IA seja local: o agente conectado pode enviar contexto ao provedor do modelo. A arquitetura completa do cliente precisa ser considerada ao apresentar essa vantagem.

## 3. O que está sendo vendido

Eu separaria a oferta em três camadas, com promessas proporcionais à evidência disponível.

**Camada de entrada: entendimento do código.** O README descreve indexação com Tree-sitter, enriquecimento semântico, armazenamento persistente, consultas MCP, busca, arquitetura, chamadas e ligações entre serviços. O benefício é substituir parte dos ciclos de busca e leitura por perguntas estruturais. Essa é a porta de entrada para uma demonstração e um piloto.

**Camada de expansão: memória de engenharia.** O projeto já descreve ADRs, grafos compartilháveis e capacidades entre repositórios. A visão de realização e idealização amplia isso para relacionar o que existe com o que foi pretendido. O benefício pretendido é reduzir a perda de contexto entre pessoas, tarefas e sessões. A extensão dessa memória precisa ser demonstrada por capacidade, especialmente quanto a persistência e atualização.

**Camada estratégica: governança do trabalho dos agentes.** Union, horizontes especulativos, claims, contestações, proveniência e bases temáticas propõem tornar explícitos o fundamento e o destino das decisões. A documentação atual também descreve gates de mutação em hosts específicos. Essa camada pode sustentar uma oferta empresarial, desde que se prove o ciclo completo e seus limites de cobertura.

O cliente compra tempo recuperado, entendimento confiável e controle operacional. Termos como “horizonte cognitivo” e “autoridade epistêmica” podem explicar a arquitetura aos especialistas; na apresentação comercial, eu os traduziria em exemplos de trabalho.

## 4. Diferenciais que eu defenderia

### Relações explícitas como contexto para agentes

A proposta organiza símbolos e relações para responder perguntas como “quem chama esta função?”, “quais rotas dependem deste módulo?” e “o que pode ser afetado por esta mudança?”. Busca textual e busca semântica continuam úteis; o grafo acrescenta uma forma direta de consultar estrutura.

**Valor de venda:** encontrar contexto relevante com menos exploração manual. O limite é que a relação extraída depende da linguagem, do padrão de código e da resolução disponível. Um resultado vazio não prova ausência de dependências.

### Operação local e instalação nativa

O README descreve um executável nativo com ativos de runtime verificados, sem runtime de linguagem, Docker ou chave de API obrigatórios para o motor. Isso reduz requisitos de infraestrutura e cria uma hipótese favorável de adoção por desenvolvedores.

**Valor de venda:** experimentar no próprio ambiente com pouca preparação. Instalação, atualização, compatibilidade e consumo de recursos precisam continuar previsíveis para preservar essa vantagem.

### Integração com ferramentas de agentes

MCP e os mecanismos de instalação permitem oferecer contexto ao agente no seu fluxo de trabalho. O projeto documenta várias superfícies de clientes, com condições de suporte diferentes.

**Valor de venda:** melhorar o trabalho onde a equipe já atua. A matriz de compatibilidade deve deixar claros os recursos disponíveis por cliente; não se deve apresentar integração como uniforme em todos os hosts.

### Conexão entre fatos, decisões e padrões

O PRD V3 organiza três perguntas: o que existe, o que se pretende e como a instituição trabalha. Essa combinação é o diferencial estratégico mais interessante: padrões e decisões podem se tornar conhecimento consultável e contestável, com origem explícita.

**Valor futuro:** agentes que conseguem justificar escolhas com referências da equipe e declarar quando estão propondo algo novo. O PRD marca partes dessa visão como evolutivas; elas devem permanecer identificadas como tal na venda.

### Transparência sobre o conhecimento

A proposta de distinguir evidência, hipótese, convenção e invenção responde a um problema de confiança. O comprador poderia perguntar de onde veio uma recomendação e o que ainda precisa ser confirmado.

**Valor futuro:** decisões mais revisáveis. Isso exige validar referências, versões, identidades e persistência; registrar uma declaração não garante sua veracidade.

## 5. Cliente inicial, comprador e abordagem de venda

Meu cliente inicial seria uma equipe que já usa agentes de código e trabalha em um sistema existente com dependências difíceis de navegar. Eu priorizaria repositórios em linguagens cuja qualidade possa ser demonstrada, evitando prometer profundidade equivalente em toda a cobertura anunciada.

O usuário seria o desenvolvedor. O patrocinador interno provavelmente seria um tech lead ou responsável por plataforma. O comprador econômico seria a liderança de engenharia, quando o ganho aparecer em tempo, custo ou previsibilidade. Segurança participaria da avaliação do fluxo de dados e da instalação.

A venda começaria por uma tarefa que o cliente reconhece:

1. Selecionar uma investigação real, como localizar o fluxo de um endpoint ou avaliar uma alteração em um componente compartilhado.
2. Executar condições comparáveis com e sem assistência do grafo, mantendo repositório, modelo e tarefa fixos.
3. Medir tempo, tokens, chamadas de ferramentas, qualidade da resposta e correções humanas.
4. Mostrar as referências que sustentam a resposta e as dependências que exigem confirmação.
5. Expandir para outras tarefas somente se o primeiro ganho se repetir.

Uma demonstração convincente acompanha uma pergunta até as evidências que permitem agir. A visualização do grafo ajuda a comunicar o sistema, mas o resultado da tarefa deve ocupar o centro da apresentação.

### Pitch de aproximadamente um minuto

> Quando um agente entra em um sistema existente, ele precisa descobrir onde estão as peças e como elas se conectam. O Codebase Memory MCP organiza o código em um grafo persistente que o agente consulta no próprio fluxo de trabalho. A equipe pode investigar chamadas, arquitetura e dependências com menos leitura repetitiva. A evolução do projeto conecta esse mapa às decisões e aos padrões da organização, para que o trabalho tenha contexto e justificativa rastreáveis. Começamos provando o ganho nas suas tarefas e expandimos conforme os resultados.

## 6. Modelo de negócio proposto

**Minha hipótese seria manter um núcleo aberto e monetizar operação e governança de equipes.** Isso aproveita a adoção individual como canal de entrada e preserva a auditabilidade do motor.

| Oferta proposta | Valor cobrado | Evidência necessária |
|---|---|---|
| Núcleo aberto | Adoção e distribuição | Uso recorrente e instalação confiável |
| Plano de equipe | Administração, colaboração e memória compartilhada | Demanda por coordenação entre usuários e projetos |
| Plano empresarial | Políticas, identidade, trilhas auditáveis, suporte e implantação controlada | Controles verificáveis e comprador com orçamento |
| Integração com plataformas | Motor incorporado a ferramentas e workflows | Parceiros e ganho demonstrável para seus usuários |

Essas ofertas são propostas comerciais, não um catálogo disponível. A licença MIT anunciada favorece adoção e também facilita incorporação por terceiros. O negócio precisaria construir valor contínuo em operação, suporte, integrações e capacidades empresariais.

Eu testaria cobrança por equipe ou desenvolvedor ativo antes de escolher um modelo definitivo. Cobrar por consulta poderia desencorajar o uso que produz valor; uma oferta local com licença e suporte recorrentes merece ser testada. Não há base nesta análise para fixar preço, margem ou receita projetada.

## 7. Superações necessárias para sustentar a tese

### Converter benchmarks em resultados do cliente

O README apresenta números expressivos, incluindo redução de tokens em cenários distintos. Eles não devem ser combinados numa promessa única: o exemplo de 120 vezes menos tokens e a avaliação citada de 10 vezes têm contextos diferentes. Esta análise não reproduziu nenhum benchmark.

O documento de benchmark também apresenta uma inconsistência entre a contagem declarada de linguagens e a decomposição da metodologia. Eu corrigiria e versionaria essas evidências antes de usá-las em material comercial.

**Superação:** publicar experimentos reproduzíveis, com versões, workloads, qualidade, falhas e variação entre repositórios. A métrica comercial deve ser o custo por tarefa concluída corretamente.

### Demonstrar governança de ponta a ponta

A auditoria de 28/09 identificou lacunas em fechamento, autoridade, persistência, proveniência e integração. A feature Union atual, datada de 29/09, descreve avanços posteriores, incluindo persistência de propostas, sweep e gates de mutação. Isso indica evolução documental; não permite concluir, sem nova verificação, quais achados foram resolvidos.

**Superação:** demonstrar um ciclo por transporte real, com reinício, evidências persistidas, recusa de operações fora de escopo e cobertura explícita por host. A própria documentação informa que hooks têm limites e não constituem uma fronteira do sistema operacional.

### Evitar que amplitude dilua qualidade

Muitas linguagens e clientes ampliam o alcance, mas também aumentam manutenção e expectativas. Parsing, resolução semântica e compreensão de frameworks representam níveis diferentes de suporte.

**Superação:** publicar níveis de qualidade e limites, aprofundando primeiro os casos mais usados pelos clientes iniciais.

### Provar que o agente usa o contexto adequadamente

Disponibilizar uma consulta não garante que o agente a faça, interprete o resultado ou perceba um índice incompleto. A integração depende de instruções, cobertura e atualização.

**Superação:** avaliar comportamento real dos agentes e acompanhar tarefas em que a consulta é ignorada ou induz uma conclusão incorreta.

### Construir vantagem durável

Grafos, parsers e protocolos abertos podem ser incorporados por outras ferramentas. Minha hipótese de defesa seria combinar qualidade de resolução, confiabilidade operacional, integrações profundas e memória institucional útil ao cliente.

**Superação:** tornar o produto recorrente no trabalho da equipe, com formatos portáveis e benefício demonstrado. Aprendizado com clientes deve respeitar a proposta local e ocorrer por dados autorizados, não por coleta presumida de código.

## 8. Visão de futuro e sequência de execução

**Primeiro horizonte: contexto estrutural confiável.** Consolidar exploração, atualização e compatibilidade. O marco seria o uso semanal em tarefas reais, com resultados comparáveis e melhoria de eficiência sem perda de qualidade.

**Segundo horizonte: memória compartilhada da engenharia.** Conectar decisões, intenções e código com persistência, referências válidas e tratamento de desatualização. O marco seria outra pessoa ou sessão recuperar o motivo de uma decisão e reconhecer quando ele deixou de valer.

**Terceiro horizonte: governança do trabalho dos agentes.** Transformar propostas, contestações, proveniência e autorização em controles verificáveis nos ambientes suportados. O marco seria distinguir o que foi solicitado, permitido, executado e observado, incluindo falhas e resultados desconhecidos.

**Quarto horizonte: padrões institucionais reutilizáveis.** Expandir as bases temáticas para vários projetos, com versões, curadoria, vínculos e desvios explícitos. O marco seria atualizar um padrão e identificar quais projetos precisam rever suas decisões.

A visão é uma infraestrutura que preserve continuidade entre agentes, pessoas e projetos. Cada horizonte deve entregar valor próprio e financiar o próximo; o produto inicial não pode depender da concretização de toda a visão.

## 9. O que eu exigiria antes de investir

Eu buscaria evidências em um conjunto pequeno de equipes, durante um piloto de seis a oito semanas. Esse prazo é uma proposta de experimento, não um dado do projeto.

- **Adoção:** instalação concluída, tempo até o primeiro resultado e retorno semanal.
- **Eficiência:** tokens, latência, chamadas e custo por tarefa correta.
- **Qualidade:** respostas verificadas, dependências omitidas e correções humanas.
- **Retenção:** continuidade de uso após o efeito inicial da demonstração.
- **Pagamento:** comprador identificado e compromisso concreto com piloto pago ou contratação.
- **Operação:** compatibilidade, atualização, consumo de recursos e esforço de suporte.
- **Governança:** persistência, cobertura dos controles e comportamento em tentativas de contorno.

Não financiaria a expansão apenas com base em número de ferramentas, linguagens ou testes anunciados. Buscaria uma capacidade específica que produza valor repetido e crie motivo para a equipe permanecer.

## 10. Parecer final

**A tese é atraente: vender entendimento imediato do código e construir, a partir dele, memória e governança para equipes que trabalham com IA.** O núcleo oferece uma entrada demonstrável; a união entre código, intenção e padrões oferece uma direção estratégica diferenciada.

Eu avançaria para diligência e pilotos. Meu entusiasmo aumentaria com melhoria reproduzível em tarefas reais, retenção e pagamento. A possibilidade de uma empresa relevante está na combinação entre utilidade diária e continuidade institucional. O principal risco é apresentar a visão de governança como garantia pronta antes de provar suas integrações e limites.

## Base e limites desta análise

Análise documental do workspace em 30/09/2026, com HEAD `1447bc84` e alterações locais preexistentes. Não foram executados testes, reproduzidos benchmarks ou realizadas entrevistas e pesquisa externa de mercado. Capacidades descritas são evidências documentais; propostas de venda, segmentação, monetização e investimento são hipóteses do autor. A auditoria anterior corresponde a outra revisão e serve como referência histórica, não como diagnóstico automaticamente atual.

Fontes internas consultadas:

- [README do projeto](../../README.md): proposta, distribuição, recursos e métricas anunciadas.
- [Benchmark de linguagens](../BENCHMARK.md): metodologia e variação de resultados.
- [Guia de medição](../MEASURING_SAVINGS.md): separação entre qualidade, desempenho e economia do agente.
- [Federação de grafos](../feature/multi_graph_federation.md): horizontes, consultas federadas e admissão.
- [Workflow Union](../feature/union_workflow.md): sessões, claims, proveniência e limites de hooks.
- [PRD V3](../PRD/novos-paradgimas/PRD_V3.md): bases temáticas e visão institucional evolutiva.
- [Auditoria de 28/09](2026-09-28-auditoria-relatorio-union.md): evidências históricas e lacunas de integração.
