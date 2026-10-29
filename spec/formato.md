# O formato `.mlx`, versão 1

Implementação de referência: [`mlx/`](../mlx) (C++, MIT). O app Mobilog e o `mobilog_ext` a usam: um
pacote que ela aceita instala no app. English: [format.md](format.md).

## Pacote

Um `.mlx` é um **zip** com estas restrições:

- membros só armazenados ou deflate: sem criptografia, zip64, vários discos ou links;
- até 256 membros, 64 MB por membro e 128 MB descomprimidos;
- razão de compressão até 200;
- 48 MB no total.

Os nomes dos membros são caminhos relativos em UTF-8, com `/`, sem `.`, `..`, `\` ou `:`, únicos mesmo
ignorando maiúsculas e com até 160 bytes.

| Membro | |
|---|---|
| `manifest.json` | obrigatório (veja o [manifesto](../docs/pt/manifesto.md)) |
| o módulo de `entry` | obrigatório: um módulo WebAssembly |
| os arquivos que o manifesto cita | `icon`, `help`, `help@*`, `changelog`, `screenshots` |
| `signature.json` | opcional |

Quem escreve gera zips determinísticos: `manifest.json` primeiro, o resto em ordem de nome, datas
1980-01-01 00:00, sem atributos extras.

## Módulo

O módulo é WebAssembly 1.0 com bulk memory, sign extension, conversões float→int sem trap e mutable
globals. Nada além disso: sem SIMD, threads, exceções, reference types, multi-value, memory64, várias
memórias ou várias tabelas.

Além disso, o módulo:

- define e exporta a `memory`, com máximo de até `limits.memoryMB`;
- não tem função de início;
- importa só as funções de [`mlx/include/mlx/Abi.h`](../mlx/include/mlx/Abi.h) (módulo `mobilog` e
  algumas de `wasi_snapshot_preview1`);
- exporta `mlx_api_version`, `mlx_alloc`, `mlx_free` e `mlx_init`. Opcionais: `mlx_event`, `mlx_task`,
  `mlx_shutdown`, `_initialize`.

## Assinatura (`signature.json`)

```json
{"version": 1, "key": "<chave pública Ed25519, base64>", "signature": "<base64>",
 "files": [{"path": "...", "size": 123, "sha512": "<hex>"}]}
```

O texto assinado é `"mobilog-ext-signature-v1\n"` seguido de uma linha `caminho\ttamanho\tsha512\n` por
membro, menos o `signature.json`, em ordem de caminho (bytes). Todo membro tem de estar listado, com o
tamanho e o SHA-512, e nada além deles.

Arquivos de chave (texto):

- `mobilog-ext-key-v1 <chave pública base64> [nome]`;
- `mobilog-ext-secret-v1 <segredo de 64 bytes em base64>` (o segredo Ed25519 do Monocypher).

## Catálogo (`catalog.json`)

```json
{"version": 1, "server": "<nome>", "serverKey": "<linha da chave ou vazio>",
 "extensions": [{"id", "name", "version", "description", "author", "permissions": ["..."],
                 "file": "<id>-<versão>.mlx", "size", "sha512", "signerKey": "<base64 ou vazio>",
                 "example": false,
                 "summary", "category", "keywords", "license", "homepage", "repository",
                 "minAppVersion", "apiVersion", "updated", "icon": "<texto SVG>", "screenshots": 2,
                 "name@<idioma>", "summary@<idioma>", "description@<idioma>"}]}
```

`file` é um nome simples: os pacotes ficam ao lado do índice (`<pasta da URL do índice>/<file>`). O
cliente instala um pacote baixado só quando o tamanho e o SHA-512 conferem com a entrada. Quem lê o
catálogo pula as entradas que não pode usar e ignora os campos que não conhece.

## Assinatura do índice da loja (`catalog.json.sig`)

```
mobilog-catalog-signature-v1 <chave pública, base64> <assinatura, base64>
```

É uma assinatura Ed25519 de `"mobilog-catalog-v1\n"` seguido dos bytes exatos do `catalog.json`. O
índice da Loja Mobilog só vale assinado por uma das chaves da loja ([`keys/`](../keys)).

## Confiança

- Um pacote assinado por uma chave da loja pode usar os ids reservados (`mobilog.*`). Com um desses
  ids, ele é uma extensão do próprio Mobilog.
- Os outros pacotes assinados pela loja são "verificados pela loja".
- Pacotes assinados por outras chaves são verificados quando o usuário confia na chave.
- Pacotes sem assinatura são não verificados.
