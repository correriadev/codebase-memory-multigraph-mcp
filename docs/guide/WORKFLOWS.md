# Fluxos para validar a ferramenta

## 1. CONEXÃO E GRAFO

Confira versão/caminho do executável, `tools/list` e `list_projects`. Indexe um repositório pequeno conhecido. Faça uma pergunta com resposta verificável: localizar uma função, seguir um chamador e ler seu fonte. Compare com busca/leitura direta. Registre geração, coverage e eventuais lacunas. Esse é o teste de utilidade mais simples e dispensa hooks/skills.

## 2. HORIZONTE ISOLADO

Use projeto de teste. Crie um horizonte com um nó identificável; procure-o com e sem `active_horizons`. Valide escopo e conflitos. A base sem overlay deve continuar distinguível. Não interprete o overlay como resultado exato do mesmo filtro/Cypher: há [limitações conhecidas](../feature/multi_graph_federation.md). Não promova nessa primeira experiência.

## 3. SESSÃO E CLAIMS

Abra sessão com `identity`, confira `restricted` e retenha o ID retornado. Capture um claim com predicado válido, tente fechar antes da resolução e observe a recusa aplicável. Resolva com destino e metadados corretos, depois feche. A operação registra o protocolo; não prova retenção de todo o raciocínio do agente.

## 4. TEMA VERSIONADO

Indexe um repositório separado com conteúdo de conhecimento pequeno. Registre tema com `target_uri` de projeto inteiro e versão de teste, consulte catálogo e conteúdo. Reindexe esse alvo e verifique a recusa de geração/pin obsoleto. Registro é persistente e não há tool de exclusão de tema anunciada; use cache isolado quando precisar descartar todo o experimento.

Bindings normativos e decisões de founding exigem autoridade do host e têm lacunas de schema. Validar somente `theme_lookup` não valida esses caminhos. Ver [temas](../feature/tradition.md).

## 5. BUNDLE E FAST PATH

Selecione um símbolo real, extraia bundle e avalie-o sem alterar os hashes. Inspecione motivos de incerteza em vez de esperar elegibilidade. Reextraia depois de mudanças de geração/regra/coverage. Um manifest persiste informação; confira a [diferença entre evidência declarada e verificada](../feature/harness_v2.md) antes de chamá-lo de aprovação.

## 6. ADMISSÃO E HOOKS

Promoção exige sessão e âncoras reais, altera o grafo e pode recusar por diferentes razões. Exercite primeiro as suítes nativas de [admissão](../feature/cross_horizon_admission.md). Não invente hash nem habilite override só para fazer um exemplo passar.

Hooks têm teste próprio e ativação opcional. A validação do servidor não exige instalá-los no ambiente de trabalho; consulte [política de hooks](../feature/mutation_gate.md).

## CRITÉRIO DE SUCESSO

Separe três resultados: mecanismo funciona no teste, agente usa o mecanismo corretamente e tarefa final melhora. Os dois últimos exigem execuções comparáveis com tarefas reais. Registre tempo, consultas, falhas e qualidade da resposta; [como medir](../MEASURING_SAVINGS.md). A documentação não apresenta uma campanha completa de validação como já realizada.

## REFERENCES

- [Testes e comandos](../adr/TESTS.md), [tools](TOOLS_REFERENCE.md), [consumo](CONSUMPTION.md).
