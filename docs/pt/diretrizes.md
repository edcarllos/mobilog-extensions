# Diretrizes da loja

A loja quer extensões úteis, honestas e leves. A revisão segue estas regras.

## Identidade

- **Id:** `io.github.<seu-usuário>.<nome>` (ou um domínio seu, ao contrário: `br.ufsb.<nome>`). Não use
  o nome de outra pessoa ou organização. `mobilog.*` é das oficiais.
- **Nome:** curto (até 40 caracteres), diz o que faz. Não use "Mobilog" no nome.
- **Resumo:** uma frase, até 80 caracteres, sem "a melhor", "a única" e outros superlativos.
- **Categoria:** a que descreve o principal uso.
- **Palavras-chave:** as que alguém usaria para procurar; sem repetir o nome.

## Ícone

- `icon.svg`, quadrado (sugerido `viewBox="0 0 48 48"`), até 32 KB.
- **Só formas** (`rect`, `circle`, `path`...): texto, imagens embutidas e scripts não são desenhados pelo
  app.
- Um fundo de cor sólida (cantos arredondados, raio ~11 de 48) com um símbolo claro por cima funciona
  nos temas claro e escuro.
- Legível em 24 px: poucos detalhes, traços de 3 px ou mais.
- O modelo `templates/icon.svg` é um ponto de partida. O `check` desenha o ícone com o mesmo
  renderizador do app (nanosvg) e recusa um que não apareça.

## Capturas

Até 4, PNG ou JPG, até 300 KB cada, de preferência 1280×720 (16:9). Mostre a extensão funcionando no
mapa, não uma tela vazia. Uma captura de tela do sistema serve: reduza para 1280×720 e salve em JPG
com qualidade ~80.

## Manual e novidades

- `help.md` explica o que faz, como usar e o que cada permissão libera. `help.en.md` em inglês
  (recomendado).
- `CHANGELOG.md` com uma seção por versão (`## 1.2.0`), em linguagem de usuário.

## O que é recusado

- Pedir permissões que a extensão não usa.
- Código ofuscado, gerado sem o fonte ou dependências binárias sem fonte.
- Texto enganoso (o que não faz), conteúdo ofensivo ou ilegal.
- Coleta de dados pessoais: as extensões não têm rede; não tente contornar isso (por exemplo, pedindo ao
  usuário que copie dados para fora).
- Extensões que travam ou deixam o app lento (ganchos longos, desenhos a cada quadro sem mudança).
- Cópias de outra extensão sem acréscimo, ou sem o crédito que a licença dela exige.

## Licença

Diga a licença em `license` (e ponha um `LICENSE` na pasta se ela não for MIT). O código das extensões
deste repositório é público. Sem `LICENSE` próprio, vale a licença MIT do repositório.
