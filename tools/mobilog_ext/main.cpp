// mobilog_ext: creates, builds, checks, tests, packs and signs Mobilog extensions (.mlx), and
// writes the store's index. The checks are the app's own (mlx/): a package this tool accepts
// installs in the app. Run with no arguments for the list of commands.
#include "Tool.h"

#include <cstdio>
#include <cstring>

namespace mlxtool {

int usage() {
    std::fprintf(stderr, "%s",
                 tr("mobilog_ext: extensões do Mobilog (.mlx)\n"
                    "\n"
                    "Escrever uma extensão:\n"
                    "  new <id> [--c|--cpp] [--name \"Nome\"] [--author \"Quem\"]   cria extensions/<id> a partir do modelo\n"
                    "  build [pasta] [--wasi-sdk <dir>] [--watch]                compila e empacota em <pasta>/dist/<id>-<versão>.mlx\n"
                    "  check [pasta|pacote.mlx] [--store <mobilog_store>] [--require-bump]\n"
                    "                                                           tudo o que a loja confere\n"
                    "  test [pasta|pacote.mlx] [--frames N] [--script roteiro.json] roda a extensão sem o app\n"
                    "\n"
                    "Pacotes e chaves:\n"
                    "  pack <pasta> [-o saida.mlx] [--key <nome>.secret]          empacota uma pasta como está\n"
                    "  keygen <nome> [--name \"Quem assina\"]                     cria <nome>.secret e <nome>.pub\n"
                    "  sign <pacote.mlx> --key <nome>.secret [-o saida.mlx]\n"
                    "  verify <pacote.mlx> [--trust <nome>.pub ...]\n"
                    "  inspect <pacote.mlx>\n"
                    "  meter <modulo.wasm> -o <saida.wasm>                        o módulo como o app o executa (depuração)\n"
                    "\n"
                    "Loja:\n"
                    "  catalog <mobilog_store> [--key <loja>.secret] [--name \"Loja Mobilog\"] [--updated-from-git <repo>]\n"
                    "                                                           escreve catalog.json (e .sig)\n"
                    "\n"
                    "Documentação: docs/pt (guia rápido, manual, FAQ).\n",
                    "mobilog_ext: Mobilog extensions (.mlx)\n"
                    "\n"
                    "Writing an extension:\n"
                    "  new <id> [--c|--cpp] [--name \"Name\"] [--author \"Who\"]     creates extensions/<id> from the template\n"
                    "  build [folder] [--wasi-sdk <dir>] [--watch]               compiles and packs <folder>/dist/<id>-<version>.mlx\n"
                    "  check [folder|package.mlx] [--store <mobilog_store>] [--require-bump]\n"
                    "                                                           everything the store checks\n"
                    "  test [folder|package.mlx] [--frames N] [--script script.json] runs the extension without the app\n"
                    "\n"
                    "Packages and keys:\n"
                    "  pack <folder> [-o out.mlx] [--key <name>.secret]            packs a folder as it is\n"
                    "  keygen <name> [--name \"Who signs\"]                         creates <name>.secret and <name>.pub\n"
                    "  sign <package.mlx> --key <name>.secret [-o out.mlx]\n"
                    "  verify <package.mlx> [--trust <name>.pub ...]\n"
                    "  inspect <package.mlx>\n"
                    "  meter <module.wasm> -o <out.wasm>                          the module as the app runs it (debugging)\n"
                    "\n"
                    "Store:\n"
                    "  catalog <mobilog_store> [--key <store>.secret] [--name \"Mobilog Store\"] [--updated-from-git <repo>]\n"
                    "                                                           writes catalog.json (and .sig)\n"
                    "\n"
                    "Documentation: docs/en (quick start, manual, FAQ).\n"));
    return 2;
}

}  // namespace mlxtool

int main(int argc, char** argv) {
    using namespace mlxtool;
    if (argc < 2 || !std::strcmp(argv[1], "-h") || !std::strcmp(argv[1], "--help") || !std::strcmp(argv[1], "help")) return usage();
    const std::string cmd = argv[1];
    Args a;
    if (!parseArgs(argc, argv, 2, a)) return usage();
    if (cmd == "new") return cmdNew(a);
    if (cmd == "build") return cmdBuild(a);
    if (cmd == "check") return cmdCheck(a);
    if (cmd == "test") return cmdTest(a);
    if (cmd == "catalog") return cmdCatalog(a);
    if (cmd == "keygen") return cmdKeygen(a);
    if (cmd == "pack") return cmdPack(a);
    if (cmd == "sign") return cmdSign(a);
    if (cmd == "verify") return cmdVerify(a, false);
    if (cmd == "inspect") return cmdVerify(a, true);
    if (cmd == "meter") return cmdMeter(a);
    std::fprintf(stderr, "%s: %s\n\n", tr("comando desconhecido", "unknown command"), cmd.c_str());
    return usage();
}
