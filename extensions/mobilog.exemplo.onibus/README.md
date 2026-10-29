# Ônibus em linha · Bus line

Uma linha de ônibus circular desenhada no mapa, com ônibus rodando no meio do trânsito da simulação: intervalo, tempo de parada, tempo de volta e regularidade.

A circular bus line drawn on the map, with buses running in the traffic of the simulation: headway, dwell time, round-trip time and regularity.

- **id:** `mobilog.exemplo.onibus` · **versão / version:** 1.0.2 · **licença / license:** MIT
- **linguagem / language:** C (só o SDK, sem biblioteca C) / C (the SDK only, no C library)
- **permissões / permissions:** mapa.ler, simulacao.ler, mapa.desenhar, mapa.ferramentas, veiculos.controlar
- **manual:** [help.md](help.md) · [help.en.md](help.en.md) · **novidades / changes:** [CHANGELOG.md](CHANGELOG.md)

```sh
mobilog_ext build extensions/mobilog.exemplo.onibus
mobilog_ext test extensions/mobilog.exemplo.onibus
mobilog_ext check extensions/mobilog.exemplo.onibus
```

Uma extensão oficial do Mobilog: só o mantenedor publica versões novas. Sugestões e correções por
pull request são bem-vindas. / An official Mobilog extension: only the maintainer publishes new
versions. Suggestions and fixes by pull request are welcome.
