# Busca local 2-opt / Or-opt

Um resolvedor de roteirização feito como extensão, para servir de exemplo de quem quer escrever o seu.
Com a extensão ligada, ele aparece como **Busca local (extensão)** em **Extensões → Algoritmos** e na
**Bancada**, ao lado dos resolvedores do Mobilog.

## Como funciona

1. **Plano inicial:** cada veículo sai do depósito e vai sempre ao cliente mais próximo que ainda cabe nele.
2. **Busca local:** dentro de cada rota, inverte trechos enquanto o tempo cai (**2-opt**); depois tenta
   levar cada cliente para a melhor posição, na mesma ou noutra rota (**Or-opt**), sem passar da capacidade.
3. **Busca local iterada:** enquanto houver tempo, muda alguns clientes de lugar ao acaso e busca de novo,
   ficando com o melhor plano.

Cada plano melhor vai ao Mobilog, que confere a viabilidade e calcula o custo com as próprias contas.

## Parâmetros (em Algoritmos)

- **Clientes movidos por perturbação:** quanto maior, mais longe a busca vai a cada rodada.
- **Piora aceita (%):** uma rodada que deixa o plano até este tanto pior continua dela; acima disso volta ao melhor.

## O que ele resolve

Problemas com capacidade (CVRP) e frota mista. Janelas de horário, coleta e entrega e veículos já na
rua ficam com os resolvedores do Mobilog (ALNS, HGS).

O código está em [extensions/mobilog.exemplo.busca-local](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.busca-local) e usa só o SDK em C (`sdk/include/mobilog_ext.h`).
