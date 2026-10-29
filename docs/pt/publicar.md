# Publicar na loja

A **Loja Mobilog** é a pasta `mobilog_store/` deste repositório. O app lê o índice dela
(`catalog.json`, assinado em `catalog.json.sig`) e baixa os pacotes. **Ninguém envia `.mlx`:** você
envia o código, e a loja compila, confere, assina e publica. O que está na loja é sempre o build do
código revisado.

## Passo a passo

1. **Fork** deste repositório e um branch (`minha-extensao`).
2. **Crie a pasta** com `mobilog_ext new io.github.<seu-usuário>.<nome>` (ou copie um modelo).
   - O id é para sempre: os usuários atualizam por ele.
   - Use o seu usuário do GitHub no id. Ids `mobilog.*` são só das oficiais.
3. **Escreva, teste e confira:**
   ```bash
   mobilog_ext build extensions/<id>
   mobilog_ext test extensions/<id>
   mobilog_ext check extensions/<id> --store mobilog_store
   ```
   Não faça commit de `dist/` (o `.gitignore` já ignora).
4. **Abra o pull request** para `main`, com o checklist do modelo preenchido. Só a pasta
   `extensions/<id>/` deve mudar.
5. **O CI do PR** (`pr.yml`) roda, para cada extensão alterada:
   - `check --require-bump` (versão nova em relação à loja);
   - `build` duas vezes, conferindo que os bytes são iguais (build reprodutível);
   - `test`.

   Ele não tem acesso a chaves: nada é assinado num PR.
6. **Revisão.** Um mantenedor lê o código, as permissões pedidas e o texto da loja. Veja o que é
   recusado em [diretrizes.md](diretrizes.md).
7. **Merge.** O CI de publicação (`publish.yml`) compila de novo, assina com a chave da loja, grava
   `mobilog_store/<id>-<versão>.mlx`, tira a versão anterior e refaz o índice. Em até alguns minutos
   (o GitHub guarda o índice por até 5 min), a extensão aparece na **Sistema → Extensões → Loja**
   de todos os apps, com o selo **Verificada · Loja**.

## Atualizar

Mude o código, **suba a versão** em `manifest.json` e acrescente a seção `## <versão nova>` no
`CHANGELOG.md`. O CI recusa um PR que muda a extensão sem subir a versão. Quem tem a extensão instalada
recebe um aviso, e uma atualização que não pede permissões novas pode ser instalada com
**Atualizar todas**.

## Quem pode mudar a pasta

O `.github/CODEOWNERS` põe o autor como dono da pasta da extensão: PRs de outras pessoas nela precisam
da aprovação dele (e de um mantenedor).

## Tirar da loja

Abra um PR que remove a pasta. Na publicação, o pacote sai da loja; quem já instalou continua com a
versão instalada.

## Fora da loja

Você também pode distribuir o `.mlx` direto (site, e-mail) ou num `mobilog_server` da sua organização
(`mobilog_server ext add`). Assine com a sua chave (`mobilog_ext keygen`, `mobilog_ext sign`) e
divulgue o `.pub`: quem confiar nela vê a extensão como **Verificada**.
