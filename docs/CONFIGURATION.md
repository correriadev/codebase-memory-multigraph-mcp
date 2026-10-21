# Configuração em uso

## ARQUIVOS E RESPONSABILIDADES

| Configuração | Local/formato |
|---|---|
| Mapeamento global de extensões | `$XDG_CONFIG_HOME/codebase-memory-mcp/config.json`, fallback `~/.config/codebase-memory-mcp/config.json` |
| Mapeamento do repositório | `.codebase-memory.json`; sobrepõe entradas globais conflitantes |
| Settings de runtime | `_config.db` SQLite no cache; gerenciado por `config set/reset` |
| UI | `config.json` no cache; `ui_enabled` e `ui_port` |
| Integração MCP/skills/hooks | Configuração do cliente, separada dos bancos do CBM |

Use `codebase-memory-mcp config --help` e o help da build para conhecer chaves/valores aceitos antes de alterá-los. Consulte [limites de descoberta](INDEX_RESOURCE_LIMITS.md) para `index_max_files` e `index_max_source_mb`.

Exemplo de extensões adicionais no repositório:

```json
{"extra_extensions":{".custom":"python"}}
```

O nome de linguagem deve ser reconhecido pelo runtime. Ignorar arquivos é outra função, descrita em [.cbmignore](cbmignore.md).

## AMBIENTE

| Variável | Alcance |
|---|---|
| `CBM_CACHE_DIR` | Sobrescreve o diretório de índices/configuração; default `~/.cache/codebase-memory-mcp`. |
| `CBM_RUNTIME_DIR` | Muda o pai do rendezvous do daemon. Não é o mesmo que mudar cache. |
| `CBM_ALLOWED_ROOT` | Restringe raízes de indexação após resolução; não é sandbox geral do agente. |
| `CBM_LOG_LEVEL` | Nível de log; conferir eventos do daemon em `logs/cbm-daemon.log` no cache. |
| `CBM_DIAGNOSTICS` | Habilita artefatos de diagnóstico quando ativado conforme o runtime; a descoberta de seus caminhos é registrada em log. |
| `CBM_WORKERS` | Sobrescreve contagem de workers de indexação. |
| `CBM_ENFORCE_MUTATION_GATE` | Somente `1` ativa checks estritos dos hooks de mutação. Default desligado. Exige também integração do hook no cliente. |
| `CBM_ALLOW_LEGACY_PROMOTION` | Somente `1` permite exceção à exigência de sessão na promoção. Não é necessário para uso básico. |
| `CBM_OPERATOR_IDENTITY`, `CBM_OPERATOR_TOKEN` | Autoridade configurada pelo host para os caminhos normativos; os handlers comparam credenciais. Não são campos de uma skill. |

O runtime também aplica verificações de propriedade/permissão dos diretórios de coordenação e limites de raiz. Se recusar o caminho, leia o erro do executável; não remova controles para fazer uma instalação parecer funcional.

## INSTALAÇÃO NO CLIENTE

O comando `install` herdado pode configurar clientes e escrever skills/perfis/hooks. Para testar somente o servidor, prefira apontar o MCP à build desejada e revisar `tools/list`. Não confundir fontes em `skills/` ou `skills-fractal/` com algo instalado no ambiente.

## REFERENCES

- [Consumo](guide/CONSUMPTION.md), [hooks](feature/mutation_gate.md), [build](INFRASTRUCTURE.md).
