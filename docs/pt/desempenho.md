# Desempenho

Extensões do Mobilog rodam **compiladas**, não interpretadas:

- no computador e no Android, pelo wasmtime (Cranelift);
- no navegador (e no app de iPhone e iPad, que roda o Mobilog web), pelo motor do navegador.

Medido no Mobilog, o mesmo código fica entre **1,0× e 1,4×** o tempo do C compilado dentro do app.
O pacote Rotas e entregas, por exemplo, faz 1,04× a 1,39× do nativo. O passo da simulação não muda
com as extensões oficiais ligadas.

## Regras

1. **Peça listas inteiras, não item por item.** Cada `mobilog_call` custa algumas dezenas de
   nanossegundos (o mesmo que uma chamada de função lenta). `map.edges` de uma vez, e não um trecho por
   chamada num laço.
2. **O que é pesado e comum, o app faz em nativo.** Rotas, custos a partir de um nó e matrizes de tempo
   (`route.*`) são calculados pelo app, em paralelo. Não reimplemente Dijkstra na extensão.
3. **Ganchos rápidos; o pesado em tarefas.** Um gancho acima de 50 ms vai para o registro; acima de
   250 ms três vezes, a extensão é desligada. Cálculos longos vão para `task.start`, que roda em
   segundo plano.
4. **Mande desenhos só quando mudarem.** `draw.set_layer` substitui a camada inteira; não mande a cada
   quadro se nada mudou.
5. **Painel leve.** `MLX_EV_PANEL` chega a cada quadro com o painel na tela: monte a lista de widgets
   a partir do que você já calculou.
6. **Memória sob demanda.** `limits.memoryMB` é o máximo, não o que é reservado.

## Medir

- `mobilog_ext test` mostra o pior tempo de cada gancho (no interpretador, mais lento que no app, mas
  útil para comparar versões).
- No app: **Sistema → Extensões → (a extensão)** mostra chamadas, último e pior tempo e memória; e
  **Sistema → Extensões → Avançado → Desempenho das extensões** mede o aparelho.
