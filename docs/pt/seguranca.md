# Segurança

## O isolamento

Cada extensão é um módulo WebAssembly com a **própria memória** (até o limite do manifesto). Ela não
lê a memória do app nem de outra extensão; só troca bytes pelas funções da API, que conferem cada
pedido (tamanhos, índices, permissões). Não há rede, sistema de arquivos, processos ou relógio do
sistema além de `time_ms`. Um módulo que trava é parado (por época no wasmtime, por contagem de laços no
navegador) e desligado.

O app confere o módulo antes de instalar: só WebAssembly 1.0 com os recursos aceitos, memória com
máximo, só importações da API, sem função de início.

## Assinaturas e selos

| Selo | Quer dizer |
|---|---|
| **Mobilog** | uma extensão oficial (id `mobilog.*`), compilada e assinada pela loja |
| **Verificada · Loja** | compilada a partir do código revisado deste repositório e assinada pela chave da loja |
| **Verificada** | assinada por uma chave em que o usuário confia (Sistema → Extensões → Avançado → Chaves) |
| **Não verificada** | sem assinatura, ou assinada por uma chave desconhecida: o app avisa antes de instalar |
| **Em desenvolvimento** | da pasta de desenvolvimento do próprio usuário |

A assinatura (Ed25519) cobre **todos** os arquivos do pacote: um arquivo trocado, a mais ou a menos a
invalida. O índice da loja também é assinado: um espelho (um `mobilog_server`) não consegue alterar a
lista nem os pacotes.

## A chave da loja

A chave privada fica só nos segredos do GitHub deste repositório (`MOBILOG_STORE_KEY`) e só o
`publish.yml`, na `main`, a usa. Pull requests (inclusive de forks) não têm acesso a ela. A chave
pública está em `keys/loja.pub` e vem embutida no app. Para trocar a chave, a nova entra em `keys/` ao
lado da antiga, sai uma versão do app com as duas, e só então a loja passa a assinar com a nova.

## Relatar um problema

Encontrou uma falha de segurança no formato, na ferramenta ou numa extensão da loja? Não abra uma issue
pública: escreva para o mantenedor pelo perfil do GitHub
([@edcarllos](https://github.com/edcarllos)).
