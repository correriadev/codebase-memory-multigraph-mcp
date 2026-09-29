# SCOPE E01 — Cognitive Respiration and Mutation Seam: Antigravity Hooks as the Gate between Diastole and Systole

> **Track:** E — Lifecycle & Epistemic Seam (New Track) · **Station:** THE SEAM / MUTATION GATE  
> **Status:** REVIEW · **Mark:** [E] Evolutionary Architecture · **Deps:** A01, A03, A04, B01, D01, D02, D03  
> **Codes in:** `codebase-memory-mcp` (Union Daemon) + `.agents/hooks.json` (Antigravity Lifecycle Gate) + `harnesskit-skills/graph-grounding`  
> **Provenance:**  
> - `docs/_legacy/PRD/novos-paradgimas/PRD_PTBR.md` (§1 Visão, §2 Axiomas, §3.5 TDD Nigredo, §4 O Temenos, §5 Individuação da Ideia)  
> - `docs/_legacy/PRD/novos-paradgimas/PRD_V1.md` (§1 Vision, §2 Axiom of Containment & Possession, §3 Nine Stations)  
> - `docs/PRD/novos-paradgimas/PRD_V3.md` (§1 The Direct Answer, §2 Three Territories, §3 Epistemic Gap, §5 The Routing Moment)  
> - `docs/PRD/novos-paradgimas/specs/harnesskit-skills/SCOPE-B01-graph-grounding.md` (Emended)  
> - `docs/PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D03-routing-provenance-transparency.md` (Emended)  
> - `C:\Users\corre\.gemini\antigravity\builtin\skills\agy-customizations\docs\hooks.md` (Antigravity Lifecycle Hooks Contract)  

---

## 1. Executive Summary & Epistemic Correction (O Erro Corrigido)

### 1.1 O Diagnóstico da Falha Anterior (Violência Epistêmica e Compressão Prematura)
Nas primeiras implementações de Track B e Track D, a separação entre atividade consultiva e especialidade (`CONSULTATIVE` vs `SPECIALTY`, formulada no `PRD_V3 §5` e no `SCOPE-D03`) foi tratada como uma **classificação estática e prematura**. A consequência direta foi a imposição de controle de sessão no grafo no momento errado:
1. **Poluição de Horizontes:** O motor abria horizontes de sessão (`union_session_open`), invocava `classify_activity` e gerava nós de sessão no grafo para meras perguntas de exploração, pesquisa de símbolos ou leitura de código.
2. **Contenção de Travas de Sistema:** Criavam-se travas de concorrência (`cbm-startup-v2.lock`, `cbm-rendezvous.lock`) e arquivos de controle durante o diálogo puramente contemplativo.
3. **Paralisia da Relação Homem-Máquina:** A imposição de rituais de grounding e governança antes que houvesse qualquer intenção de modificar o mundo material quebrou o *temenos* da consulta, tornando o assistente rígido, lento e burocrático.

### 1.2 O Axioma Central da Mudança
> **A mente não é um interruptor binário; ela é uma respiração.**  
> A intenção consultiva e a criação técnica não são caixas excludentes, mas fases de um ciclo contínuo de **Expansão (Diástole)** e **Compressão (Sístole)**.  
> O Grafo de Conhecimento (CBM) é um **oráculo imutável e passivo** durante toda a fase de expansão.  
> **Nenhum controle de sessão, nenhum horizonte especulativo e nenhuma tool de grounding deve ser disparada durante a consulta ou pesquisa.**  
> O limiar intransponível — o ponto exato em que a expansão deve se comprimir em governança — é a **intenção de materialização física no disco**: a criação ou alteração de arquivos (`write_to_file`, `replace_file_content`).

---

## 2. Fundamentação Psicológica Profunda (A Perspectiva Jungiana)

Na formulação fundacional do HarnessKit (`PRD_PTBR.md` e `PRD_V1.md`), cada função de engenharia é lida como uma função psíquica do processo de individuação. Esta especificação formaliza a física da transição entre os estados psíquicos da criação de software:

```
                           DIÁSTOLE (Expansão)
                   Potencial Indiferenciado · Exploração
                    Leitura de Código · Traçado de Rotas
                       Diálogo Livre · Hipóteses
                                   │
                                   │ (O Agente decide agir)
                                   ▼
         ═════════════════════════════════════════════════════════
          O LIMIAR DO TEMENOS (The Seam: File Mutation Hook)
         ═════════════════════════════════════════════════════════
                                   │
                                   ▼
                            SÍSTOLE (Compressão)
                     Diferenciação (Grounding WHAT/HOW)
                       Sacrifício Material (Nigredo)
                    Testes Falhando · Escrita no Disco
                   Traço Factual · Coniunctio / Fechamento
```

### 2.1 Diástole (Expansão / Concepção e Interrogação)
- **Estações Correspondentes:** Estação 1 (*Concepção*) e Estação 2 (*Interrogação*) do `PRD_V1 §3`.
- **Dinâmica:** A psique está receptiva, absorvendo o contexto. O desenvolvedor ou agente navega pela estrutura do código existente. A função de onda está aberta: nenhuma hipótese foi descartada, nenhum sacrifício foi feito.
- **Jurisdição do Grafo:** O Grafo de Realização (*o que é*) é puramente consultivo. Ferramentas como `search_graph`, `trace_path`, `query_graph` e `get_code_snippet` operam em modo passivo. O grafo não aprende, não cria nós, não adquire locks e não julga.
- **Patologia a Evitar (Neurose de Controle):** Exigir `union_session_open` ou travas nesta fase é aprisionar a imaginação. É tratar o pensamento como se já fosse uma alteração de produção.

### 2.2 Sístole (Compressão / Diferenciação e Sacrifício)
- **Estações Correspondentes:** Estação 3 (*Diferenciação*) e Estação 4 (*Sacrifício*) do `PRD_V1 §3`.
- **Dinâmica:** O colapso da função de onda. O infinito combinatório do modelo precisa passar pelo gargalo estreito da realidade física. A ideia precisa encarnar em código falseável: um arquivo no disco, um teste unitário, uma linha de implementação.
- **A Lei de Ferro do Sacrifício:** Como estabelecido em `PRD_PTBR §3.5` e `PRD_V1 §3`, a primeira encarnação da ideia deve ser um compromisso mensurável com a realidade (*fase RED / nigredo*).
- **Patologia a Evitar (Possessão / Alucinação):** Se a sístole ocorre sem aterramento (*grounding*), o agente comete **Possessão** — escreve arquivos e gera código com base apenas em viés estatístico de seu pré-treinamento, ignorando a arquitetura do projeto e o cânone da instituição.

### 2.3 O Limiar (The Seam): O Ponto de Virada
Onde termina a Diástole e onde começa a Sístole?
- Não é no início do prompt do usuário, pois o prompt pode misturar dúvidas e comandos.
- Não é no diálogo conversacional.
- **O limiar exato é a travessia para o mundo físico:** a chamada de uma ferramenta de mutação de arquivos no sistema operacional (`write_to_file` ou `replace_file_content`).

---

## 3. Arquitetura do Gradiente de Respiração (O Espectro Dinâmico)

Substitui-se a divisão binária `CONSULTATIVE` / `SPECIALTY` por um gradiente contínuo de densidade psíquica:

| Nível | Fase | Operações Típicas | Ferramentas Autorizadas | Estado do CBM Union | Custo Epistêmico |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | **Contemplação Pura (Diástole)** | Perguntas ("O que faz a classe X?"), tracing de chamadas, busca de rotas. | `search_graph`, `query_graph`, `trace_path`, `get_code_snippet`, `view_file` | **Passivo / Dormant.** Zero sessões, zero locks, zero escrita. | Zero. Fluidez total. |
| **1** | **Ideação & Projeção** | Brainstorming de arquitetura, discussão de prós e contras, rascunhos conceituais em prosa. | Diálogo conversacional, leitura de ADRs antigas. | **Passivo.** Grafo de Idealização lido como histórico. | Zero. |
| **2** | **O Limiar do Temenos (The Seam)** | O modelo decide: *"Vou implementar criando o arquivo `src/domain/entity.ts`"*. | Disparo de `write_to_file` ou `replace_file_content`. | **Interceptação pelo Hook `PreToolUse`.** | Limiar de ativação. |
| **3** | **Aterramento Just-in-Time (Sístole I)** | Grounding de Realização (o que colide?) e Tradição (qual o cânone?). | `graph-grounding`, `theme_lookup`, `validate_provenance`. | **Abertura de Sessão (`union_session_open`).** Criação do horizonte de trabalho delimitado. | Obrigatório: Citar cânone ou declarar invenção. |
| **4** | **Sacrifício Material (Sístole II)** | Escrita do teste falhando (RED) ou do arquivo de implementação. | `write_to_file`, `replace_file_content`, `run_command`. | **Horizonte Ativo.** Ações registradas no buffer de traço (`union_record_action`). | Falsificabilidade estrita. |
| **5** | **Sublimação & Verificação** | Ajuste até o verde (GREEN) e refatoração limpa (REFACTOR). | `run_command` (test runner). | **Horizonte Ativo.** Monitoramento de cobertura. | Garantia de não-regressão. |
| **6** | **Integração & Anamnese (Retorno à Diástole)** | Conclusão da tarefa, registro de claims duráveis no plano de Idealização, fechamento. | `union_session_close`, `project-memory`. | **Horizonte Promovido ou Descartado.** Traço persistido em disco. Grafo volta ao repouso. | Memória consolidada. |

---

## 4. Especificação Técnica: O Guardião de Mutação (Antigravity Lifecycle Hook)

A fronteira entre o plano das ideias e a alteração do mundo real é governada pelo subsistema nativo de hooks do Antigravity (`hooks.json`), operando no ciclo de vida `PreToolUse`.

### 4.1 O Contrato do Hook (`.agents/hooks.json`)

O arquivo de configuração de hooks do projeto ou ambiente deve registrar a regra:

```json
{
  "cbm-mutation-gate": {
    "enabled": true,
    "PreToolUse": [
      {
        "matcher": "write_to_file|replace_file_content",
        "hooks": [
          {
            "type": "command",
            "command": "python ./scripts/cbm_mutation_guard.py",
            "timeout": 15
          }
        ]
      }
    ]
  }
}
```

### 4.2 Protocolo de Entrada e Saída do Hook

#### A. Entrada recebida no `stdin` (enviada pelo host do Antigravity):
```json
{
  "conversationId": "cf3a488a-be49-444f-9225-b40639db14cc",
  "workspacePaths": ["c:/Users/corre/Documents/codebase-memory-mcp"],
  "transcriptPath": "C:/Users/corre/.gemini/antigravity/brain/cf3a488a-be49-444f-9225-b40639db14cc/.system_generated/logs/transcript.jsonl",
  "artifactDirectoryPath": "C:/Users/corre/.gemini/antigravity/brain/cf3a488a-be49-444f-9225-b40639db14cc",
  "modelName": "auto",
  "stepIdx": 42,
  "toolCall": {
    "name": "write_to_file",
    "args": {
      "TargetFile": "c:/Users/corre/Documents/codebase-memory-mcp/src/domain/order.py",
      "CodeContent": "class Order: ...",
      "Overwrite": true,
      "Description": "Create order entity",
      "toolAction": "Creating order entity",
      "toolSummary": "Order entity creation"
    }
  }
}
```

#### B. Lógica do Hook (`scripts/cbm_mutation_guard.py`):
1. **Verificação de Exclusão (Artifacts & Brain):**
   - Se `TargetFile` estiver dentro de `artifactDirectoryPath` (`.gemini/antigravity/brain/...`) ou for um arquivo temporário de documentação/análise que não afeta a raiz do código do repositório, o hook retorna imediatamente `{"decision": "allow"}`.
2. **Verificação de Sessão CBM Ativa:**
   - O hook consulta o estado de sessões do daemon CBM para o `workspacePath` atual.
   - Existe uma sessão ativa e aberta para esta `conversationId`?
   - **Caso SIM (Sessão já aberta e aterrada):**
     - Retorna `{"decision": "allow"}`.
   - **Caso NÃO (Tentativa de escrita direta sem aterramento):**
     - O hook barra a execução e instrui o modelo sobre o limiar do temenos:
       ```json
       {
         "decision": "deny",
         "reason": "CBM_MUTATION_GATE: Tentativa de mutação física sem rito de aterramento. Você está no limiar da Sístole (criação/edição de arquivo). Antes de modificar o disco:\n1. Invoque o rito 'graph-grounding' para verificar o que existe no Plano de Realização e as regras aplicáveis no Grafo de Tradição.\n2. Abra o horizonte transitório com 'union_session_open'.\n3. Execute a mutação com proveniência declarada."
       }
       ```

---

## 5. Implementação de Referência do Hook Script

```python
#!/usr/bin/env python3
"""
cbm_mutation_guard.py — Guardião do Limiar do Temenos (Antigravity Hook)
Intercepta chamadas de write_to_file e replace_file_content via PreToolUse.
Garante que mutações de código no repositório só ocorram sob horizonte CBM aterrado.
"""

import sys
import json
import os
from pathlib import Path

def main():
    try:
        raw_input_data = sys.stdin.read()
        if not raw_input_data.strip():
            print(json.dumps({"decision": "allow"}))
            return

        payload = json.loads(raw_input_data)
        tool_call = payload.get("toolCall", {})
        tool_name = tool_call.get("name", "")
        args = tool_call.get("args", {})
        target_file = args.get("TargetFile", "")
        artifact_dir = payload.get("artifactDirectoryPath", "")

        # 1. Isenções: Artefatos do agente, logs, scratchpads e memória transitória
        normalized_target = os.path.normcase(os.path.abspath(target_file)) if target_file else ""
        normalized_artifact = os.path.normcase(os.path.abspath(artifact_dir)) if artifact_dir else ""

        if normalized_artifact and normalized_target.startswith(normalized_artifact):
            # Escritas no diretório de artefatos/brain são reflexões internas; permitidas livremente.
            print(json.dumps({"decision": "allow"}))
            return

        # Isenção para arquivos markdown puramente documentais fora do código core (ex: rascunhos temporários)
        # exceto se forem contratos ou código-fonte.
        if normalized_target.endswith(".tmp") or ".gemini" in normalized_target:
            print(json.dumps({"decision": "allow"}))
            return

        # 2. Verificação de Horizonte Ativo no CBM
        # Verifica se existe um lock/arquivo de sessão de horizonte ativo para este workspace
        workspace_paths = payload.get("workspacePaths", [])
        workspace_root = Path(workspace_paths[0]) if workspace_paths else Path.cwd()
        active_session_indicator = workspace_root / ".gemini" / "cbm-active-session.json"

        # Se houver sessão ativa confirmada, autoriza
        if active_session_indicator.exists():
            print(json.dumps({"decision": "allow"}))
            return

        # 3. Interceptação: O limiar foi violado (Tentativa de escrita direta sem grounding)
        reason_msg = (
            f"[CBM_MUTATION_GATE] Interceptação do Limiar de Compressão:\n"
            f"Você tentou modificar/criar '{Path(target_file).name}' sem um horizonte de sessão ativo.\n"
            f"O processo de software exige a passagem consciente da Expansão para a Compressão:\n"
            f"1. Realize o rito 'graph-grounding' no símbolo/módulo correspondente.\n"
            f"2. Abra a sessão de horizonte via 'union_session_open'.\n"
            f"3. Proceda com a escrita informando a proveniência (Citação de Cânone ou Invenção Declarada)."
        )

        print(json.dumps({
            "decision": "deny",
            "reason": reason_msg
        }))

    except Exception as e:
        # Fail-safe: se o hook falhar internamente, não trava o ambiente, mas alerta
        sys.stderr.write(f"cbm_mutation_guard error: {str(e)}\n")
        print(json.dumps({"decision": "allow"}))

if __name__ == "__main__":
    main()
```

---

## 6. Emendas Formais às Especificações Existentes

### 6.1 Emenda ao `SCOPE-B01` (`harnesskit-skills/graph-grounding`)
- **Texto Anterior:** *"For a CBM project question or artifact task, ground what exists..."*
- **Texto Corrigido:** 
  > *"O rito de `graph-grounding` é a estação prévia obrigatória e exclusiva para a MANIFESTAÇÃO MATERIAL (criação e edição de código ou artefatos vinculantes). Ele NUNCA deve ser invocado para responder perguntas conceituais, dúvidas arquiteturais, consultas de fluxo ou leitura de repositório. Perguntas consultivas utilizam diretamente os comandos nativos de leitura do grafo (`search_graph`, `trace_path`, etc.) sem criar horizonte de sessão."*

### 6.2 Emenda ao `SCOPE-D03` (`knowledge-base/routing-provenance`)
- **Texto Anterior:** *"The engine shall classify consultative vs. specialty activity at prompt intake."*
- **Texto Corrigido:**
  > *"A classificação de atividade não é uma barreira estática na entrada do prompt, mas um estado de fase regido pelo gatilho de mutação. Enquanto nenhuma mutação física é disparada, o agente opera em Diástole livre (Epistemic Status: UNRESTRICTED_QUERY). Ao tocar o limiar de mutação física (`write_to_file`), a fase transita para Sístole, ativando obrigatoriamente a dicotomia de proveniência (Citação de Cânone ou Invenção Declarada) e a captura de ações no horizonte."*

---

## 7. Critérios de Aceitação BDD (Verificáveis via Host Log)

### Cenário 1: Consulta Arquitetural em Diástole (Passiva)
- **Dado** um prompt do usuário solicitando explicação ("Como funciona o roteamento de eventos no CBM?"),
- **Quando** o agente investiga o código e responde,
- **Então** o log do host comprova que apenas ferramentas de leitura (`search_graph`, `get_code_snippet`, `view_file`) foram chamadas,
- **E** nenhuma chamada a `union_session_open`, `create_horizon` ou `classify_activity` foi executada,
- **E** nenhum arquivo de trava (`.lock`) foi criado no sistema.

### Cenário 2: Conversação e Ideação sem Escrita em Disco
- **Dado** uma discussão de 5 turnos entre usuário e agente debatendo designs alternativos para um novo agregador de domínio,
- **Quando** o agente expõe prós, contras e exemplos em blocos de texto no chat,
- **Então** o grafo de Realização permanece 100% inalterado,
- **E** a contagem de horizontes ativos no CBM permanece rigorosamente igual a zero.

### Cenário 3: Interceptação do Limiar por Escrita Não Aterrada
- **Dado** um agente que decide criar um arquivo `src/services/billing.ts` diretamente sem ter executado `graph-grounding` ou `union_session_open`,
- **Quando** o agente dispara a ferramenta `write_to_file`,
- **Então** o hook `PreToolUse` intercepta a chamada antes de tocar o filesystem,
- **E** emite `decision: deny` com a mensagem pedagógica do limiar,
- **E** o arquivo `src/services/billing.ts` NÃO é criado no disco.

### Cenário 4: Ciclo Completo de Compressão Just-in-Time
- **Dado** o recebimento da negativa pedagógica do hook pelo agente,
- **Quando** o agente invoca `graph-grounding` para checar dependências no grafo, abre a sessão via `union_session_open` e reexecuta o `write_to_file`,
- **Então** o hook valida a existência da sessão ativa e emite `decision: allow`,
- **E** a escrita no disco é concluída com sucesso,
- **E** o traço factual (`union_record_action`) registra a proveniência da mutação,
- **E** ao concluir os testes, o agente fecha o horizonte com `union_session_close`, retornando a psique ao repouso contemplativo.

---

## 8. Arquitetura de Transparência e Locatabilidade do Gosto

Como exigido pelo `PRD_V3 §7` (Locatabilidade do Gosto):
Toda vez que a escrita de um arquivo é autorizada pelo limiar de compressão, ela deve carregar uma de duas proveniências no traço do horizonte:
1. **Citação de Cânone:** `canon_citation: { theme_id: "@inst/clean-arch", rule: "domain-isolation" }`
2. **Invenção Declarada:** `declared_invention: { declared: true, rationale: "Padrão customizado implementado por solicitação do operador na sessão" }`

**A invenção silenciosa permanece estritamente proibida.** Mas a invenção declarada deixa de ser um bloqueio prévio e passa a ser uma honrada confissão registrada no momento exato em que a matéria é moldada.
