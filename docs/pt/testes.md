# Testar

## `mobilog_ext test`: sem o app

```bash
mobilog_ext test extensions/io.github.seu-usuario.minha-extensao
```

Liga a extensão num interpretador (WAMR) do jeito que o app liga (`_initialize`, `mlx_api_version`,
`mlx_init`), manda os eventos de um uso normal (a malha e o cenário uma vez, os painéis e gráficos na
tela a cada quadro, um toque no mapa e nas ferramentas) e a desliga. Mostra:

- o pior tempo de cada gancho;
- as funções do app chamadas, e as que não estão disponíveis ali (sem mapa nem simulação, como no app
  antes de abrir uma área: a extensão tem de lidar com isso);
- a memória usada;
- o registro da extensão.

São **erros**: trap (acesso fora da memória, `unreachable`...), `mlx_init` diferente de 0, gancho que
passa de 2 s e chamada a uma função cuja permissão não está no manifesto.

Resolvedores de rotas declarados rodam num CVRP de 20 clientes; cada um precisa oferecer soluções
viáveis.

### Roteiro

Um `test/roteiro.json` na pasta da extensão (ou `--script arquivo.json`) troca o uso padrão:

```json
[
  {"network": 1},
  {"frames": 30},
  {"panel": "principal"},
  {"click": 10, "in": "principal"},
  {"tool": "marcar", "action": "tap", "at": [-39.27, -14.78]},
  {"tap": [-39.28, -14.79]},
  {"chart": "historico"},
  {"scenario": true},
  {"frames": 30}
]
```

## No app: a pasta de desenvolvimento

**Sistema → Extensões → Avançado → Desenvolvimento** (computador): escolha a pasta `dist/` da extensão.
Todo `.mlx` novo ou alterado ali é instalado de novo na hora, ligado, com as permissões que pede, e roda
com o selo **Em desenvolvimento**. Com:

```bash
mobilog_ext build extensions/io.github.seu-usuario.minha-extensao --watch
```

salvar o código já atualiza o app. A simulação continua; a extensão recomeça do zero (os dados
guardados ficam).

Para automatizar (capturas, testes de ponta a ponta), o app aceita `MOBILOG_EXT_DEV=<pasta>`.

## Depurar

- **Sistema → Extensões → (a extensão) → Registro:** o que ela escreveu com `mobilog_log` (ou `printf`,
  com a biblioteca C) e os erros: chamada lenta, travou, trap.
- `mobilog_ext inspect pacote.mlx`: o manifesto, as permissões, as contribuições, o módulo (funções,
  laços, memória, importações) e a assinatura.
- Um módulo recusado diz por quê: memória sem máximo, um recurso que o app não aceita (SIMD, threads,
  exceções...) ou uma importação desconhecida.
