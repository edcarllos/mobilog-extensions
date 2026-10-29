# Pontos críticos

Mostra onde o trânsito está mais lento agora na simulação.

- **No mapa** (camada *Pontos críticos*): os trechos mais lentos em vermelho, numerados do pior para o
  menos pior. Um por rua: se a mesma avenida tem vários trechos lentos, aparece o pior.
- **No painel** (Extensões → Pontos críticos): a lista com a velocidade de cada trecho e quanto ela é do
  limite; o botão com o número leva o mapa até ele.
- **Lentidão da rede:** quanto da velocidade permitida se perde, pesado pelo comprimento e pelas faixas
  das vias. Aparece na barra de cima e num gráfico que dá para fixar no mapa.

A lista é refeita a cada 3 segundos de simulação. Um trecho entra quando está abaixo da metade do limite
e tem pelo menos 40 m.

Permissões: ler o mapa (nomes e trechos), ler a simulação (velocidades) e desenhar no mapa.

O código está em [extensions/mobilog.exemplo.pontos-criticos](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.pontos-criticos) e usa só o SDK em C (`sdk/include/mobilog_ext.h`).
