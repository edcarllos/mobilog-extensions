# Perguntas frequentes

### Por que meu pull request falhou no CI?

Abra o log do passo que falhou. Os motivos mais comuns:

- `version: ... já está na loja`: suba a versão no `manifest.json` e acrescente `## <versão>` no
  `CHANGELOG.md`.
- `icon: quase nada aparece em 48 px` ou `não tem formas`: o ícone usa texto, imagem ou não tem
  `viewBox`/`width`/`height`. Veja [diretrizes.md](diretrizes.md#ícone).
- `... sem a permissão "x" no manifesto`: a extensão chama uma função cuja permissão não pediu.
- `build não reprodutível`: algo no build depende da máquina (data, caminho). Use o `mobilog_ext build`.
- `a pasta deve ter o nome do id`: `extensions/<id>/` com o mesmo id do manifesto.

Rode localmente `mobilog_ext check extensions/<id> --store mobilog_store --require-bump` e
`mobilog_ext test extensions/<id>`: são os mesmos passos.

### Posso escrever em Rust, Zig, AssemblyScript...?

Sim, se sair um módulo WebAssembly 1.0 (com bulk memory, sign-ext, conversões sem trap e mutable
globals; sem SIMD, threads e exceções), com memória exportada com máximo, as exportações `mlx_*` e
importando só o módulo `mobilog` (veja `mlx/include/mlx/Abi.h`). Exemplo mínimo em Rust:

```rust
#[link(wasm_import_module = "mobilog")]
extern "C" { fn log(level: i32, text: *const u8, len: i32); }

#[no_mangle] pub extern "C" fn mlx_api_version() -> i32 { 1 }
#[no_mangle] pub extern "C" fn mlx_init() -> i32 {
    let s = b"oi do Rust";
    unsafe { log(1, s.as_ptr(), s.len() as i32) };
    0
}
// mlx_alloc / mlx_free: exporte um alocador (por exemplo, sobre Vec<u8>).
```

Compile com `--target wasm32-unknown-unknown` e `-C link-arg=--max-memory=67108864`, e empacote com
`mobilog_ext pack`. Para a loja, o `mobilog_ext build` hoje compila C e C++. Para outra linguagem, abra
uma issue: o `build.json` pode ganhar o suporte.

### Como depuro?

`mobilog_log` (ou `printf` com a biblioteca C) vai para **Sistema → Extensões → (a extensão) →
Registro**. Use `mobilog_ext test` com um roteiro para reproduzir uma sequência de eventos, e a pasta de
desenvolvimento para ver no app a cada build. Veja [testes.md](testes.md).

### O app não mostra a minha atualização

O GitHub guarda o índice da loja em cache por até 5 minutos, e o app relê a loja ao abrir e uma vez por
dia. Toque em **Atualizar a lista** na Loja.

### Funciona sem internet?

A loja guarda a última cópia do índice: abre sem internet, mas para baixar precisa de rede. Um
`mobilog_server` na rede local espelha a loja (ligado por padrão) e serve os aparelhos sem internet.

### Minha extensão pode acessar a internet ou arquivos?

Não. Veja [permissoes.md](permissoes.md#o-que-nenhuma-permissão-libera).

### Como guardo dados entre as sessões?

`storage_set`/`storage_get` (até 20 MB, no aparelho), ou a seção da extensão no cenário
(`scenario.data_set`), que vai junto com o cenário. Veja [conceitos.md](conceitos.md).

### Como sei se a função existe nesta versão do app?

`mobilog_has("nome.da.funcao", n)`. Funções novas ganham números novos; os antigos nunca mudam.

### Que licença usar?

A que você quiser, dita em `license`. Sem `LICENSE` próprio na pasta, vale a MIT deste repositório.

### Posso vender ou cobrar por uma extensão?

A loja só tem extensões gratuitas e de código aberto. Fora dela, distribuir o `.mlx` é com você.
