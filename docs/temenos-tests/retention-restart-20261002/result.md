# Reten??o ap?s sa?da do propriet?rio e rein?cio do daemon

Data: 2026-10-02
Resultado: **PASSOU** em `20261002-161234-6916`
Horizonte sint?tico: `h_retention_restart_20261002_161234_6916`
Projeto-base: `C-Users-corre-Documents-harness-kit`

## Pergunta testada

Um horizonte rec?m-gravado continua recuper?vel pelo MCP depois que o processo dono encerra e um daemon novo ? iniciado, sem o cliente leitor abrir o artefato humano?

## Procedimento e resultado

O teste criou um cache privado a partir de um snapshot SQLite online do grafo-base real do HarnessKit e usou diret?rios privados de cache e runtime com o bin?rio CBM instalado. O daemon escritor (PID 19200) criou um horizonte sint?tico, sincronizou tr?s n?s sem?nticos e tr?s arestas, e passou `validate_scope_horizon` com conectividade estrita (`VALID`, zero n?s isolados e zero depend?ncias n?o resolvidas).

Depois que o ?ltimo cliente MCP saiu, o daemon escritor encerrou automaticamente. Uma leitura SQLite offline confirmou que o banco individual do horizonte ainda existia, com `integrity_check=ok`, estado `ACTIVE`, sete n?s simb?licos (tr?s sem?nticos e quatro se??es indexadas) e tr?s arestas virtuais. O daemon isolado n?o estava rodando nesse ponto.

Um processo daemon novo (PID 5404) iniciou sobre o mesmo cache de teste. Um cliente MCP independente reencontrou o horizonte no cat?logo como `ACTIVE`, com `owner_alive=false`; recuperou as partes e rela??es pelo grafo; reconstruiu o artefato de 82 bytes com SHA-256 `d1b75ba4a8468e6a58149c1c51d032145aa05b0049e1bca4b7089e72b7b7b24`; e passou novamente a valida??o estrita. O cliente leitor n?o leu `artifact.txt` nem outro arquivo humano para obter o conte?do.

O daemon compartilhado do usu?rio (PID 4932) permaneceu ativo. O SHA-256 do arquivo SQLite do grafo HarnessKit compartilhado foi igual antes e depois: `6b6dcb5ee3aeed5bfbca71f01b659d7a2eb60edb504c0066f15fe0d3e87d655d`. As grava??es do teste ocorreram no cache isolado.

## O que isso prova

Prova, para esse bin?rio e nesse snapshot isolado, que o armazenamento do horizonte sobreviveu ? sa?da do daemon propriet?rio e foi redescoberto e consultado pelo MCP ap?s iniciar outro daemon. Tamb?m prova a reconstru??o exata de um artefato sint?tico pequeno e a recupera??o das rela??es registradas.

## Limites

- O horizonte era sint?tico e continha um marcador de 82 bytes; este teste n?o reabriu nem validou o horizonte R3 da conversa real de UI.
- A base era uma c?pia isolada do grafo HarnessKit. O teste n?o altera nem prova reten??o no cache compartilhado de produ??o.
- O teste cobriu uma sa?da do propriet?rio e um rein?cio do daemon. N?o cobriu expira??o do TTL de 3.600 segundos, rein?cio do dispositivo, atualiza??o do aplicativo ou reten??o por prazo longo.
- O teste n?o prova que o reaper de TTL esteja agendado ou opere em produ??o.
- A prova n?o mede a fidelidade nem a completude da especifica??o de UI; cobre a persist?ncia e reconstru??o do marcador criado pelo pr?prio teste.

## Evid?ncias

- [Relat?rio de m?quina](20261002-161234-6916/report.json)
- [Transcri??o MCP](20261002-161234-6916/mcp-transcript.json)
- [Snapshot offline ap?s a sa?da do propriet?rio](20261002-161234-6916/horizon-after-owner-exit.json)
- [Artefato humano de refer?ncia](20261002-161234-6916/artifact.txt)
- [Especifica??o do horizonte de teste](20261002-161234-6916/horizon-spec.md)
- [Script reprodut?vel do teste](run_retention_restart.py)

As duas tentativas anteriores est?o preservadas como diagn?stico do harness. Seus erros no parser de payload e na gera??o do relat?rio foram corrigidos antes da execu??o can?nica aprovada acima; elas n?o contam como resultados aprovados.

## Pr?ximo gate

O pr?ximo teste de reten??o deve exercitar a expira??o real por TTL e o ciclo de coleta no c?digo/runtime de produ??o, em ambiente isolado. O reaper Python aproximado n?o substitui essa prova. At? esse gate, limitar a conclus?o ? sa?da do processo propriet?rio seguida de um rein?cio do daemon.
