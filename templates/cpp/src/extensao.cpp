// Um ponto de partida: um painel com o número de veículos na simulação e um botão que marca o centro
// do mapa numa camada própria. Veja docs/pt/guia-rapido.md (en: docs/en/quick-start.md).
#define MLX_DEFINE_ALLOCATOR
#include "mobilog_ext.hpp"

struct Extensao : mlx::Extension<Extensao> {
    int marcas = 0;  // a extensão é um objeto estático: só inicializadores constantes

    int32_t init() {
        mlx::log(1, "{{NAME}} pronta");
        return 0;
    }

    // O painel está na tela: mande o conteúdo dele.
    void panel(mlx::Str id) {
        mlx::SimInfo sim;
        const bool rodando = mlx::simInfo(sim);
        mlx::Ui ui;
        ui.section(1, "Agora");
        ui.kpi("Veículos", rodando ? mlx::Str(mlx::Num(sim.active)) : mlx::Str("—"));
        ui.kpi("Velocidade média", rodando ? mlx::Str(mlx::Text<32>() << mlx::Num(sim.meanSpeedKmh) << " km/h") : mlx::Str("—"));
        ui.endSection();
        ui.button(1, "Marcar o centro do mapa", true);
        if (marcas) ui.muted(mlx::Text<48>() << mlx::Num(marcas) << (marcas == 1 ? " marca" : " marcas"));
        ui.send(id);
    }

    // O usuário clicou num botão do painel.
    void ui(mlx::Str, const mlx::UiAction& a) {
        if (a.id != 1) return;
        mlx::MapInfo m;
        if (!mlx::mapInfo(m)) return;
        ++marcas;
        mlx::Layer layer("marcas");
        layer.marker((m.minLon + m.maxLon) / 2, (m.minLat + m.maxLat) / 2, "flag", 0, "centro");
        layer.send();
        mlx::toast("centro marcado no mapa");
    }
};

MLX_EXTENSION(Extensao)
