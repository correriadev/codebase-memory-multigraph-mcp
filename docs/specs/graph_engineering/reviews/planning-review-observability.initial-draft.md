# Rascunho arquivado — revisão preliminar de Observabilidade e Valor

**SUPERADO.** Esta cópia preliminar foi preservada apenas como registro da revisão. As dependências aqui descritas foram corrigidas depois; use `planning-review-observability.md` nesta pasta e o encerramento do CTO em `009-review-verification.md`. Este rascunho não define requisitos nem status atual.

**Data:** 2026-10-23  
**Escopo:** revisão documental de PBI-31/PBI-32 em `docs/specs/graph_engineering/fronts/05-observability-value.md` contra C01–C08, 003, 007, PBB e DECISIONS.  
**Limite:** leitura de artefatos; sem runtime, testes executáveis, benchmark, spike, consulta a índice global ou leitura de `harness-history`.

## Achado

| ID / severidade | Evidência antes da correção | Risco | Correção aplicada | Status |
|---|---|---|---|---|
| OBS-REV-01 — Média | A frente declarava que S4 “completa PBI-31” e OV-R03 exigia gatilhos, falhas, risco/decisões/intervenções e custos de revisão; porém as seis tasks originais cobriam somente o mínimo S1 (OV-01–04) e comparação S6 (OV-05/06). Assim, o critério S4 de 007 não tinha task rastreável e poderia ser omitido ao consolidar a lista. | PBI-31 poderia ser marcado completo sem integrar as fontes das demais frentes ou expor cobertura incompleta. | Acrescentada OV-07 para integração de cobertura S4, somente leitura, com aceite de fonte/revisão, ausência de escrita cruzada e lacuna explícita. A tabela `requires` liga a task a ER-12, PE-10/12, AD-05/06/07 e EV-12. OV-06 exige OV-07 para conclusão completa de PBI-32. S1 OV-01–04 permanece independente dessas fontes e continua consultável mesmo se S4 estiver incompleto. | Corrigido; verificação pós-correção abaixo. |

## Verificação pós-correção

- **PBI-31/S1:** mínimo de veredictos, custo/uso e reservas por ciclo; origem local journal/outbox e leitura sem stack externa. OV-R01/02 e tasks OV-01–04 preservam que cada contexto produtor grava seus próprios fatos; Observabilidade apenas lê e mantém suas projeções.
- **PBI-31/S4:** OV-07 e `requires` dão caminho de entrega para gatilhos/supressões (PE-10/12), risco e decisões/intervenções (AD-05/06/07), fatos de execução (ER-12) e evolução (EV-12). OV-S15/S16 cobrem conclusão e falha de cobertura sem atrasar S1.
- **C01–C03/C06–C08:** IDs e revisões correlacionam os fatos; estados/motivos não são redefinidos; orçamento/consumo confirmado, reserva, pendência e estimativa continuam distintos; envelope e entrega ao menos uma vez usam dedupe; projeções podem ser reconstruídas da fonte retida; `MEASURED|ESTIMATED|UNKNOWN`, moeda, unidade/taxa e desconhecido são preservados; comparação exige protocolo e todos os resultados.
- **C04/C05:** veredicto declarado ou manifest não vira execução observada; consulta/exportação tem identidade/escopo e minimização; métricas e relatórios são read model e não concedem autorização ou alteram execução/promoção.
- **PBI-32/Q08:** população inclui falhas, retries, interrupções e revisão; referência e aceite devem ser equivalentes e pré-registrados; limiar é decisão humana anterior à medição, sem número inventado. Zero aceitas deixa custo por aceita indefinido/`UNKNOWN`; cobertura incompleta permite somente descrição/limitações; a hipótese de valor pode ser refutada.
- **C01–C08/003/007/PBB:** não encontrei conflito de contrato. O encadeamento `OV-01 → ER-07` é dependência cross-front válida no DAG global: ER-07 é a task de journal/outbox mínimo da Execução. As dependências S4 foram registradas em tabela separada e não foram propagadas ao ramo mínimo S1.

## Checagem documental

O JSON da frente contém sete tasks OV-01–07, todas mapeadas a PBI-31 ou PBI-32; estimativas 2–4h, `scope` com 2–4 itens, `acceptance` com 1–3 itens e referências `depends_on` válidas no DAG global. A frente especifica 16 cenários com nível, PBI/regra e Given/When/Then. Esta é validação estrutural do documento, não execução de teste.

## Limites remanescentes

Q04 permanece Deferred e ADR-005 PROPOSED: não se provou que hosts/runtime entregam custos, tokens, identidade e eventos completos. Q05 não fornece tarifas, moedas padrão ou limites numéricos. Q07 não define retenção temporal. Q08 não escolhe referência nem limiares humanos. A presença da task OV-07 não prova que as fontes das outras frentes estejam implementadas ou que uma futura projeção cubra a população real; cobertura ausente precisa continuar marcada incompleta.

**Veredito após correção:** PASS COM LIMITES DOCUMENTAIS. A única lacuna material de rastreabilidade encontrada foi corrigida. Nenhuma evidência de runtime ou valor de produto é afirmada.
