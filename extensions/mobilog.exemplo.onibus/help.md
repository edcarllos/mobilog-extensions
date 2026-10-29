# Ônibus em linha

Uma linha de ônibus circular com ônibus rodando **dentro da simulação**, no meio do trânsito: eles
pegam os mesmos semáforos, filas e congestionamentos que os outros veículos.

## Como usar

1. Em **Extensões → Ônibus**, toque em **Marcar paradas** e toque no mapa, na ordem do percurso. A linha
   liga cada parada à seguinte pelo caminho mais rápido e volta da última à primeira. As paradas ficam
   guardadas no cenário.
2. Escolha quantos ônibus, o **intervalo** entre as saídas e o **tempo de parada**.
3. **Iniciar operação.** Os ônibus saem da primeira parada, um a cada intervalo, e seguem de parada em
   parada.

## Indicadores

- **Volta:** quanto tempo um ônibus leva para dar a volta completa.
- **Intervalo:** o tempo real entre dois ônibus passando pela primeira parada.
- **Variação:** quanto os intervalos variam (desvio padrão sobre a média). Um ônibus que pega trânsito
  atrasa, junta mais passageiros e atrasa mais, e o de trás o alcança: acima de 30%, os ônibus estão
  andando colados.

Se a malha muda (uma via nova, uma rua fechada) ou a simulação recomeça, a operação recomeça sozinha na
malha nova, com os ônibus saindo de novo da primeira parada.

Experimente: inicie a operação, depois feche uma faixa no caminho (Eventos) ou mude o horário, e veja a
variação subir.

Permissões: ler o mapa e a simulação, desenhar no mapa, receber toques na ferramenta e controlar os
ônibus (veículos da extensão).

O código está em [extensions/mobilog.exemplo.onibus](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.exemplo.onibus) e usa só o SDK em C (`sdk/include/mobilog_ext.h`).
