# Guia rápido: sua primeira extensão em 10 minutos

Uma extensão do Mobilog é um módulo **WebAssembly** num pacote `.mlx`. Ela acrescenta ao app painéis,
camadas no mapa, ferramentas, gráficos, indicadores ou algoritmos de roteirização, sem mudar o app. Roda
isolada e só faz o que as permissões aceitas deixarem.

## 1. Prepare o ambiente

**Opção A: no navegador, sem instalar nada.** Abra este repositório no GitHub Codespaces (botão
**Code → Codespaces → Create codespace**). O ambiente (`.devcontainer`) já baixa o wasi-sdk e compila o
`mobilog_ext`.

**Opção B: no seu computador** (Linux ou macOS; no Windows, `scripts\setup.ps1`):

```bash
git clone https://github.com/edcarllos/mobilog-extensions.git
cd mobilog-extensions
scripts/setup.sh                      # baixa o wasi-sdk 34 (conferido por SHA-256) e compila o mobilog_ext
export PATH="$PWD/build/tools/mobilog_ext:$PATH"
```

Precisa de `git`, `curl`, `cmake` (3.24+) e um compilador C++20 (gcc, clang ou Visual Studio).

## 2. Crie a extensão

```bash
mobilog_ext new io.github.seu-usuario.minha-extensao --name "Minha extensão" --author "Seu nome"
```

Isso cria `extensions/io.github.seu-usuario.minha-extensao/` a partir do modelo em C++ (`--c` para C):

```
manifest.json      identidade, versão, permissões, o que ela põe na interface
icon.svg           o ícone (48×48, só formas)
help.md            o manual (aparece na Ajuda do app e na loja); help.en.md em inglês
CHANGELOG.md       as novidades de cada versão
README.md          o que é, como compilar
build.json         como compilar (opcional)
src/extensao.cpp   o código
test/roteiro.json  o que o teste faz (opcional)
```

O **id** usa notação reversa e precisa ser único. Use `io.github.<seu-usuário-do-GitHub>.<nome>`.

## 3. Compile, teste e confira

```bash
mobilog_ext build extensions/io.github.seu-usuario.minha-extensao   # → dist/<id>-<versão>.mlx
mobilog_ext test  extensions/io.github.seu-usuario.minha-extensao   # roda sem o app
mobilog_ext check extensions/io.github.seu-usuario.minha-extensao   # tudo o que a loja confere
```

- `test` liga a extensão num interpretador, sem mapa nem simulação, manda os eventos de um uso normal
  e mostra o que ela chamou, quanto demorou e a memória que usou. Uma chamada sem a permissão no
  manifesto é erro, como no app. Resolvedores de rotas rodam numa instância pequena e as soluções são
  conferidas.
- `check` compila e confere o pacote como a loja: manifesto, módulo, ícone, resumo, categoria,
  licença, ajuda, novidades da versão, capturas.

## 4. Veja no app

No Mobilog do computador: **Sistema → Extensões → Avançado → Desenvolvimento → Escolher a pasta…** e
escolha a pasta `dist/` da sua extensão. Ela é instalada na hora, com o selo **Em desenvolvimento**.

Deixe o compilador observando:

```bash
mobilog_ext build extensions/io.github.seu-usuario.minha-extensao --watch
```

A cada arquivo salvo, o pacote é refeito e o app o recarrega sozinho.

Também dá para instalar o `.mlx` à mão: **Sistema → Extensões → Instaladas → Instalar de um arquivo**.

## 5. Publique na loja

Faça um fork deste repositório, ponha a pasta da extensão em `extensions/` e abra um pull request.
O CI confere tudo; depois da revisão e do merge, a loja compila, assina e publica. Veja
[publicar.md](publicar.md).

## Próximos passos

- [conceitos.md](conceitos.md): ciclo de vida, eventos, ganchos e tarefas, limites.
- [manifesto.md](manifesto.md) e [permissoes.md](permissoes.md).
- [interface.md](interface.md): painéis, gráficos, camadas, ferramentas.
- [api.md](api.md): todas as funções.
- [resolvedores.md](resolvedores.md): algoritmos de roteirização.
- [desempenho.md](desempenho.md), [testes.md](testes.md), [faq.md](faq.md).
- As extensões oficiais em `extensions/mobilog.*` são exemplos completos.
