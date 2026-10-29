# Extensões do Mobilog

[English](README.en.md)

Extensões acrescentam ao [Mobilog](https://github.com/edcarllos) (simulação de trânsito e
roteirização no mapa real) painéis, camadas, ferramentas, gráficos, análises e algoritmos de
roteirização, sem mudar o app. Este repositório tem a **loja**, o **SDK**, a ferramenta **`mobilog_ext`**
e o **código-fonte** de todas as extensões publicadas.

- **Usar:** no app, **Sistema → Extensões → Loja**.
- **Escrever uma:** [guia rápido](docs/pt/guia-rapido.md) (10 minutos, até sem instalar nada, pelo
  Codespaces).
- **Publicar:** um pull request com a pasta da sua extensão. Veja [como publicar](docs/pt/publicar.md).

## O que tem aqui

```
mobilog_store/      A LOJA: os pacotes .mlx publicados e o índice assinado (gerados pelo CI)
extensions/<id>/    o código-fonte de cada extensão (uma pasta por extensão)
sdk/                o SDK: cabeçalhos C/C++, CMake, o modelo de roteirização (VRP)
templates/          modelos em C e C++ (o que `mobilog_ext new` copia)
tools/mobilog_ext/  a ferramenta: new, build, check, test, pack, sign, catalog...
mlx/                a biblioteca do formato .mlx (a mesma que o app usa para conferir)
docs/pt, docs/en    manuais, referência da API, FAQ
spec/               a especificação do formato
keys/               a chave pública da loja
scripts/            preparar o ambiente (wasi-sdk) e publicar a loja
```

## Como uma extensão chega à loja

1. Alguém escreve o código em `extensions/<id>/` e abre um pull request.
2. O CI confere (manifesto, ícone, permissões, build reprodutível, teste sem o app).
3. Um mantenedor revisa e faz o merge.
4. O CI compila de novo, **assina com a chave da loja** e publica em `mobilog_store/`.
5. Os apps veem a extensão na Loja, com o selo **Verificada · Loja**; os `mobilog_server` da rede
   local a espelham.

O que está na loja é sempre o build do código revisado: ninguém envia binários.

## Extensões oficiais

| | |
|---|---|
| [Rotas e entregas](extensions/mobilog.rotas) | os algoritmos de roteirização do Mobilog (CW, ALNS, HGS...) |
| [Pontos críticos](extensions/mobilog.exemplo.pontos-criticos) | onde o trânsito está mais lento agora |
| [Localização de bases](extensions/mobilog.exemplo.bases) | onde abrir bases pelo tempo médio ou pela cobertura |
| [Ônibus em linha](extensions/mobilog.exemplo.onibus) | uma linha de ônibus no trânsito simulado |
| [Busca local 2-opt / Or-opt](extensions/mobilog.exemplo.busca-local) | um resolvedor simples, em C, para estudar |

## Compilar a ferramenta

```bash
scripts/setup.sh          # wasi-sdk 34 (SHA-256 conferido) + build/tools/mobilog_ext/mobilog_ext
ctest --test-dir build    # o autoteste: modelos, builds reprodutíveis, assinaturas, recusas
```

## Licença

[MIT](LICENSE). Cada extensão pode ter a sua (campo `license` e um `LICENSE` na pasta dela).
