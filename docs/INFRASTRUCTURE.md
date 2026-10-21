# Build e runtime

## IMPLEMENTAÇÃO

O runtime é nativo, majoritariamente C11, com SQLite, Tree-sitter e dependências vendorizadas. O build usa `Makefile.cbm`, não um Makefile genérico. O executável de produção não exige o interpretador Python usado nos testes; a composição com UI exige a toolchain Node durante o build.

## COMPILAÇÃO

Na raiz, em ambiente POSIX com toolchain instalada:

```bash
make -f Makefile.cbm cbm
scripts/build.sh --with-ui
```

No Windows, `scripts/build-windows.ps1` usa MSYS2 CLANG64, GNU Make e Clang. Com a toolchain preparada:

```powershell
pwsh -File scripts/build-windows.ps1
pwsh -File scripts/build-windows.ps1 -WithUI
```

O produto fica em `build/c/`. Confirme o executável que o cliente realmente inicia; recompilar uma árvore não atualiza por si só o caminho configurado no MCP. Para experimentar as extensões, não substitua essa build por download automático do upstream presumindo equivalência.

## ESTADO E CONCORRÊNCIA

A base indexada e os horizontes usam bancos separados. O daemon coordena runtime e operações de índice; um comando CLI de consulta não implica registro permanente no watcher. O pool de horizontes e o reaper gerenciam recursos e ciclo de vida, não uma retenção ilimitada de memória.

Não generalize WAL para toda operação: o store de governança usa WAL, enquanto o caminho de admissão verifica journal DELETE/synchronous FULL para a transação entre bancos anexados. Veja [transação de promoção](feature/atomic_admission_concurrency.md).

## REFERENCES

- [Configuração](CONFIGURATION.md), [arquitetura](adr/ARCHITECTURE.md), [testes](adr/TESTS.md).
