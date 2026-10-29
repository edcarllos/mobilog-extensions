# Localização de bases

Escolhe onde abrir **p bases** — ambulâncias, depósitos, lojas, pontos de apoio — entre candidatos que
você marca no mapa, para atender os lugares da área (os que mais geram viagens).

## Como usar

1. Em **Extensões → Bases**, toque em **Marcar no mapa** e toque nos pontos onde uma base poderia ficar.
   Cada candidato vai para o cruzamento mais próximo. Os candidatos ficam guardados no cenário.
2. Escolha quantas bases abrir e o objetivo:
   - **Menor tempo médio** (p-mediana): o tempo de viagem de cada lugar até a base mais próxima, pesado
     pelas viagens que ele gera, o menor possível;
   - **Mais viagens cobertas** (cobertura máxima): o máximo de viagens que chegam a uma base no tempo de
     cobertura; o tempo médio desempata.
3. **Calcular.** O cálculo roda em segundo plano, com a matriz de tempos de viagem do Mobilog (o trânsito
   de agora, com o cenário).

## O resultado

No mapa, as bases escolhidas (numeradas) e cada lugar ligado à sua base por uma linha da cor dela; os
outros candidatos em cinza. Na barra de cima, o tempo médio até a base e a parte coberta.

## Como a escolha é feita

Inclusão gulosa (abre, uma a uma, a base que mais melhora o objetivo) seguida de trocas (fecha uma e abre
outra enquanto melhorar). É rápido e costuma achar a melhor escolha ou uma muito perto dela.

Permissões: ler o mapa (lugares e tempos de viagem), desenhar no mapa e receber toques na ferramenta.

O código está em [extensions/mobilog.exemplo.bases](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.bases) e usa só o SDK em C (`sdk/include/mobilog_ext.h`).
