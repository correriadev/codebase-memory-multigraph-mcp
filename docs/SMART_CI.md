# Seleção experimental de testes

## ESCOPO ATUAL

O projeto contém mecanismos e scripts herdados de seleção de lanes e impacto em testes. A seleção por teste tem escopo específico C/C++, depende de mapa de cobertura por teste e convenções de harness; não é seleção universal de testes em qualquer repositório.

`TEST_IMPACT_MODE` distingue uso shadow/report de narrowing em gate nos workflows correspondentes. Consulte a configuração efetiva em `.github/workflows/` e os scripts de test-impact antes de ativá-la em outro projeto. A existência do mecanismo não demonstra que o gate foi ativado neste fork ou que uma campanha de economia já foi executada.

## LIMITES

A lógica tenta ampliar a seleção quando faltam evidências. Isso não é prova geral de impossibilidade de omitir um teste afetado. Relações estáticas incompletas, geração incompatível, mapa ausente e testes não suportados exigem tratamento explícito e execução mais ampla.

Para validar o CBM-MGH, use os [comandos de teste](adr/TESTS.md). Um relatório de impacto não executa esses testes nem valida os caminhos de governança do fork.

## REFERENCES

- [Índice e coverage](feature/index_governance.md), [medição](MEASURING_SAVINGS.md).
