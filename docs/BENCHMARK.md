# Estado das medições

## ESCOPO

As medições antigas desta página eram resultados herdados de versões específicas do upstream. Não foram reproduzidas nesta revisão e não medem o conjunto das extensões CBM-MGH. As tabelas detalhadas foram retiradas da documentação operacional para evitar que desempenho antigo pareça uma validação atual do fork.

A comparação entre código e `tools/list` confirmou a superfície de 45 tools em 21/10/2026. Isso mede disponibilidade anunciada, não latência, economia de tokens, qualidade de resposta nem correção de todas as tools.

## COMO PRODUZIR RESULTADOS ATUAIS

Fixe build, cache, geração e tarefa. Compare respostas com um gabarito independente e conte falhas/omissões. Meça latência separadamente e use dados do cliente para tokens; métricas do servidor não revelam consumo total do modelo. Publique comando, seleção, ambiente e dados brutos junto ao resultado.

## REFERENCES

- [Método de comparação](MEASURING_SAVINGS.md), [testes](adr/TESTS.md), [fluxos práticos](guide/WORKFLOWS.md).
