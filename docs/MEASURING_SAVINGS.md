# Medir utilidade, qualidade e custo

## EXPERIMENTO MÍNIMO

Escolha algumas tarefas reais e delimitadas com resposta que você consegue conferir: localizar implementação, identificar impacto de mudança e cumprir uma regra explícita. Prepare um gabarito por leitura/teste independente antes de comparar os agentes.

Execute condições separadas: agente com ferramentas comuns; agente com grafo; depois, se relevante, grafo com um mecanismo opcional do fork. Mantenha tarefa, snapshot, modelo e critérios iguais. Use sessões independentes e alterne a ordem para reduzir aprendizado acidental entre condições.

## REGISTRO

| Dimensão | O que observar |
|---|---|
| Qualidade | Acertos, erros materiais, omissões e referências realmente verificadas. |
| Trabalho | Tempo até resultado aceitável, correções manuais e recusas indevidas. |
| Custo | Tokens e chamadas do cliente; consultas do CBM são apenas parte do custo. |
| Mecanismo | Chamadas efetivamente feitas, geração, coverage e efeitos no store. |

Registre build, hashes/commit, configuração de hooks/skills, projeto, tarefa, transcrição de chamadas e resultado final. Repetições ajudam a perceber variação; um caso bem-sucedido não estabelece uma taxa geral.

## INTERPRETAÇÃO

Separe “a tool respondeu”, “o agente usou corretamente” e “o trabalho ficou melhor”. Mais metadados, mais contexto ou um manifest admitido não provam o terceiro. Compare também a carga de operar o protocolo: ele pode consumir mais tempo do que economiza em uma tarefa simples.

Não há nesta documentação uma medição atual que sustente “quase impossível alucinar”, zero overhead ou vantagem corporativa demonstrada. Essa ausência não elimina o valor das funções; delimita o que ainda precisa de experimento.

## REFERENCES

- [Estado das medições](BENCHMARK.md), [validação](guide/WORKFLOWS.md), [testes](adr/TESTS.md).
