# Rotas e entregas

O pacote dos algoritmos de roteirização do Mobilog. Com ele instalado e ligado, a aba **Extensões** mostra **Problemas** (roteirização no mapa real: CVRP, janelas de horário, coleta e entrega, frota mista), **Aplicações** (delivery em tempo real com reotimização contínua), **Algoritmos** (os parâmetros de cada um) e **Bancada** (compara algoritmos em várias sementes).

Os algoritmos:

- **Vizinho mais próximo:** guloso, sempre o cliente mais próximo que cabe.
- **Clarke-Wright:** a construtiva clássica por economias (variante assimétrica).
- **CW + busca local (paralelo):** reinícios com economias sorteadas, melhorados por relocate, swap e 2-opt.
- **ALNS (paralelo):** destrói e reconstrói com pesos adaptativos e simulated annealing; resolve janelas, coleta e entrega, veículos já na rua e frota mista.
- **HGS · genético híbrido (paralelo):** tour gigante + Split, crossover OX e busca local granular.

Os paralelos rodam uma cópia por núcleo do processador, cada uma com uma semente, e o Mobilog fica com a melhor solução; o trabalho é feito em fatias curtas, para a simulação continuar fluida. Toda solução é conferida pelo próprio app antes de ser usada.

O código está em [extensions/mobilog.rotas](https://github.com/edcarllos/mobilog-extensions/tree/main/extensions/mobilog.rotas) (C++ com o wasi-sdk e o modelo de roteirização do SDK).
