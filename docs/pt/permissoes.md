# Permissões

Uma extensão pede no manifesto o que precisa; o usuário vê a lista ao instalar, como num navegador,
pode negar e pode revogar depois. Peça só o necessário: cada permissão a mais é um motivo a menos para
instalar.

| Permissão | Libera | Sensível |
|---|---|---|
| `mapa.ler` | malha, nomes, lugares, zonas, bairros, área, rotas e matrizes de tempo, resumo do cenário | |
| `simulacao.ler` | estatísticas, velocidades, tempos e passagens por trecho, posições dos veículos | |
| `mapa.desenhar` | as próprias camadas: linhas, marcadores, áreas, cores nas vias | |
| `mapa.ferramentas` | ligar a própria ferramenta de mapa | |
| `cenario.propor` | propor mudanças no cenário, que o usuário vê no Assistente e aplica, ou não | |
| `cenario.alterar` | aplicar mudanças no cenário na hora (com desfazer) | sim |
| `veiculos.controlar` | criar veículos próprios na simulação, mandá-los por rotas e removê-los | sim |
| `simulacao.ensaiar` | ensaios: a simulação copiada com e sem uma mudança, comparadas | sim |
| `arquivos.exportar` | reservada | |
| `arquivos.importar` | reservada | sim |

As **sensíveis** mudam o trabalho do usuário ou a simulação e aparecem destacadas na instalação.

## Quando uma permissão falta

A função que precisava dela responde `MLX_ERR_DENIED` na hora, também quando o usuário revoga a
permissão com a extensão rodando. Trate isso: mostre no painel o que não dá para fazer e por quê. Sem
nenhuma permissão, a extensão ainda tem o próprio painel, indicadores, dados guardados e tarefas.

O `mobilog_ext test` acusa como erro toda chamada a uma função cuja permissão não está no manifesto.

## O que nenhuma permissão libera

- **Rede:** extensões não acessam a internet nem a rede local.
- **Arquivos do aparelho:** não há sistema de arquivos; só os dados guardados da própria extensão.
- **Outras extensões:** cada uma roda isolada, com a própria memória.
