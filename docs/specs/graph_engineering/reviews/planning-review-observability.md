# Revisão independente — Observabilidade e Valor

Data: 2026-10-23. Escopo: PBI-31/PBI-32 em `fronts/05-observability-value.md`, conferidos contra C01–C08, 003/004/007/008 e PBB. Revisão documental, sem execução de código, testes, benchmark ou spike. 003/004/008 foram inspecionados como leitura cruzada; não foram alterados nesta revisão.

## Veredito

**Front 05: aprovado após correções documentais, com limites explícitos.** O suplemento preserva autoridade dos contratos comuns, separa o mínimo S1 do complemento S4/S6, permite medir o alvo S1 antes de F07 e diz explicitamente que isso não conclui PBI-31 integral nem retira F07 da visão final dos PBIs. A versão atual de 003 já modela os gates por capacidade/protocolo e a revalidação em S5; 004 e 008 ainda precisam de alinhamento textual.

## Evidência de alinhamento

- C01/C02: correlação por IDs/versão; telemetria não cria linguagem própria de estado, fase ou motivo terminal (OV-R04/R07).
- C03: uso, reservas, pendências e retries permanecem distintos e globais; a projeção não libera orçamento nem cria teto por agente (OV-R10–12/R26).
- C04/C05: manifest não prova teste; métricas não aprovam promoção; decisões e consultas respeitam identidade/escopo (OV-R15/R18/R26).
- C06/C07: fatos são gravados pelo produtor, deduplicados por entrega, projetáveis/reconstruíveis sem escrita cruzada e sem expurgo de auditoria ainda necessária (OV-R01/R05/R06/R19).
- C08: `MEASURED|ESTIMATED|UNKNOWN`, origem/unidade/moeda e lacunas permanecem distinguíveis; Q08 exige protocolo/pré-registro humano e não há limiar ou baseline inventado (OV-R08–10/R20–25).
- PBB mantém PBI-31 consultável e PBI-32 falsificável: inclui tentativas/revisões, custo desconhecido e população zero aceita sem custo zero, pode refutar valor e não altera autoridade. Q04–Q08 permanecem Deferred; ADR-005 continua PROPOSED.
- A frente contém 7 tasks PBI-31/32 com estimativas 2–4h, 2–4 alvos propostos por `scope`, aceite e predecessor; contém 16 cenários com nível, Given/When/Then, PBI e regra de origem. Não são provas de execução.

## Achados

### OBS-REV-01 — Dependências entre S4, S5 e S6 precisam ser condicionais

**Severidade:** Alta. **Estado:** gates corrigidos no front e no DAG atual de 003; alinhar a semântica de cobertura integral em 003/007/008 e atualizar cópias de cenários em 004.

**Evidência inicial:** 007 coloca telemetria integral em S4 (linhas 40–44) e evolução/F07 em S5 (linhas 46–50). A versão anterior de 003 tinha task `OV-07` em S4 com requisito conjuntivo para `EV-12`/S5 e `OV-06` exigia `OV-07` para qualquer protocolo.

**Correção no front e 003 atual:** OV-07 agora requer fontes conforme capacidade habilitada e alvo declarado; EV-12 só é exigido se F07 estiver habilitado e a fonte estiver incluída. A visão integral de PBI-31 continua aberta até todos os produtores requeridos na visão final estarem cobertos; habilitar F07 amplia o alvo e requer revalidação em S5. OV-06 sempre depende do protocolo OV-05, mas só exige OV-07/fontes S4/S5 se métricas e população do protocolo as incluírem. Protocolo S1 pode ser medido antes de F07, sem fechar PBI-31 integral nem removê-lo da visão final dos PBIs. A tabela inicia por `Alvo/capacidade` para não ser interpretada como lista de requisitos conjuntivos por task. A versão atual de 003 usa `requires_by_capability`, `revalidation_stage_by_capability` e `requires_by_protocol`; isso remove a inversão conjuntiva e mantém o gate por escopo.

**Limite:** 003 foi atualizado durante a revisão para tornar condicionais os gates. Sua aceitação ainda precisa explicitar que “fora do alvo corrente” significa não coberto naquele slice, não retirado da visão integral de PBI-31; a nota de revalidação para F07 em S5 deve permanecer no DAG final. O texto de 007/42 “completar PBI-31” e a linha PBI-31 de 008 que diz “S4 completo” também devem distinguir cobertura do slice S4 de conclusão integral após todos os produtores da visão final.

### OBS-REV-02 — Cópias GWT em 004 ainda refletem o requisito incondicional

**Severidade:** Média. **Estado:** pendente no artefato consolidado.

**Evidência:** 004/948–963 contém OV-S15/OV-S16 copiados da versão anterior do suplemento: OV-S15 trata a fonte de evolução como parte necessária da população S4; OV-S16 marca a cobertura integral incompleta por fonte ausente sem distinguir capacidade habilitada de capacidade futura, nem a pendência de cobertura integral após futura habilitação. Front 05 agora separa cobertura restrita da visão integral e exige revalidação quando F07 for habilitado. Os IDs continuam válidos e 008 aponta para eles, mas o texto copiado está desatualizado.

**Correção recomendada:** regenerar OV-S15/16 em 004 a partir do front atualizado, preservando os mesmos IDs e âncoras. O S15 atualizado declara que cobertura S4 restrita não conclui PBI-31 integral.

### OBS-REV-03 — Rastreabilidade de PBI-32 não explicita o escopo condicional

**Severidade:** Média. **Estado:** pendente em 008.

**Evidência:** 008/39 diz que PBI-31 tem “S1 mínimo / S4 completo”; isso sugere conclusão integral em S4, embora fontes F07 sejam S5 e a visão final ainda exija revalidação após sua habilitação. 008/40 lista PBI-32, `OV-05/OV-06`, S6 e PBI-31, mas não registra que a população restrita S1 pode ser medida sem OV-07/F07 enquanto métricas S4/S5 dependem das fontes correspondentes, nem que essa medida não conclui PBI-31 integral. 007/54 permite avaliar S1 sem F07. A matriz tem 32 linhas PBI únicas e os 21 links de cenários para PBI-31/32 apontam a âncoras existentes em 004.

**Correção recomendada:** distinguir na linha PBI-31 de 008 cobertura do slice S4 da conclusão integral de PBI-31 após todos os produtores requeridos pela visão final, com revalidação após F07; na linha PBI-32, registrar o gate condicional entre comparação S1 e protocolo que inclui métricas/fontes S4/S5.

## Limites e estado

O resultado documental não confirma disponibilidade real de dados, custo, retenção, autorização ou capacidade do runtime. Projeção parcial continua parcial; conclusão só vale para a população, capacidade e métricas declaradas. Não houve benchmark nem validação de produto. Nenhuma decisão Deferred foi resolvida e nenhum ADR, PBB, briefing, core C ou histórico de harness foi alterado.

## Encerramento da consolidação pelo CTO

Após o parecer independente acima, OBS-REV-01/02/03 foram reconciliados nos arquivos consolidados: JSON 003 usa requisitos condicionais por capacidade/protocolo e distingue alvo corrente de PBI-31 integral; 004 foi regenerado das fontes finais, incluindo OV-S15/16; 007 e a linha PBI-31 de 008 distinguem cobertura S4 da extensão F07/S5; a linha PBI-32 explicita medição S1 sem F07 e gates para métricas futuras. A auditoria final confirmou cópias exatas, 32 PBIs e ausência de inversão conjuntiva. As pendências dos consolidados descritas neste parecer histórico estão encerradas documentalmente. Limites de capacidade e decisões Deferred permanecem inalterados; ver 009 para o resultado final.
