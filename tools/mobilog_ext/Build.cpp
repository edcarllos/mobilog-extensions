// new, build: a new extension folder from the template; compiling one to WebAssembly with wasi-sdk
// and packing it. The flags are the ones the app's checks accept (WebAssembly 1.0 + bulk memory,
// sign extension, non-trapping conversions, mutable globals) and give the same bytes on any machine
// with the same wasi-sdk (no wasm-opt from the PATH, no timestamps, no paths in the module).
//
// An optional build.json next to manifest.json:
//   {"runtime": "freestanding" | "wasi",  freestanding (default): only the SDK, no C library;
//                                         wasi: wasi-sdk's C and C++ libraries (a "reactor")
//    "optimize": "O2" | "O3" | "Os",      default O2
//    "libraries": ["vrp"],                the SDK's VRP model (route solvers)
//    "sources": ["src/a.cpp", ...],       default: every .c/.cc/.cpp under src/ (sorted)
//    "defines": ["NAME=1"]}
#include "Tool.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <thread>

#if defined(_WIN32)
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace mlxtool {

using namespace mobilog::ext;
using nlohmann::json;

namespace {

int run(const std::vector<std::string>& argv, bool verbose) {
    if (verbose) {
        for (const auto& s : argv) std::fprintf(stderr, "%s ", s.c_str());
        std::fprintf(stderr, "\n");
    }
#if defined(_WIN32)
    std::vector<std::wstring> w;
    for (const auto& s : argv) w.push_back(fs::path(s).wstring());
    std::vector<const wchar_t*> p;
    for (auto& s : w) p.push_back(s.c_str());
    p.push_back(nullptr);
    return int(_wspawnv(_P_WAIT, p[0], p.data()));
#else
    std::vector<char*> p;
    for (const auto& s : argv) p.push_back(const_cast<char*>(s.c_str()));
    p.push_back(nullptr);
    pid_t pid;
    if (posix_spawn(&pid, p[0], nullptr, nullptr, p.data(), environ) != 0) return -1;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

struct BuildConfig {
    bool wasi = false;
    std::string optimize = "O2";
    std::vector<std::string> libraries, sources, defines;
};

bool readBuildConfig(const fs::path& dir, BuildConfig& c) {
    std::string text;
    if (!readText(dir / "build.json", text)) return true;  // optional
    const json j = json::parse(text, nullptr, false);
    const auto bad = [&](const char* what) {
        std::fprintf(stderr, "build.json: %s\n", what);
        return false;
    };
    if (!j.is_object()) return bad(tr("deve ser um objeto JSON", "must be a JSON object"));
    const std::string runtime = j.value("runtime", "freestanding");
    if (runtime != "freestanding" && runtime != "wasi") return bad("runtime: freestanding | wasi");
    c.wasi = runtime == "wasi";
    c.optimize = j.value("optimize", "O2");
    if (c.optimize != "O2" && c.optimize != "O3" && c.optimize != "Os" && c.optimize != "O1") return bad("optimize: O1 | O2 | O3 | Os");
    for (const auto& [key, list] : std::map<std::string, std::vector<std::string>*>{
             {"libraries", &c.libraries}, {"sources", &c.sources}, {"defines", &c.defines}})
        if (j.contains(key)) {
            if (!j[key].is_array()) return bad((key + tr(": deve ser uma lista", ": must be a list")).c_str());
            for (const auto& v : j[key])
                if (v.is_string()) list->push_back(v.get<std::string>());
        }
    for (const std::string& l : c.libraries)
        if (l != "vrp") return bad((tr("biblioteca desconhecida: ", "unknown library: ") + l).c_str());
    for (const std::string& s : c.sources)
        if (s.find("..") != std::string::npos || fs::path(s).is_absolute()) return bad(tr("sources: só arquivos da pasta", "sources: files of the folder only"));
    return true;
}

std::vector<fs::path> sourcesOf(const fs::path& dir, const BuildConfig& c) {
    std::vector<fs::path> out;
    if (!c.sources.empty()) {
        for (const auto& s : c.sources) out.push_back(dir / s);
        return out;
    }
    std::error_code ec;
    const fs::path src = fs::is_directory(dir / "src", ec) ? dir / "src" : dir;
    const bool recursive = src != dir;
    const auto take = [&](const fs::path& p) {
        const std::string e = p.extension().string();
        if (e == ".c" || e == ".cc" || e == ".cpp") out.push_back(p);
    };
    if (recursive) {
        for (const auto& it : fs::recursive_directory_iterator(src, ec))
            if (it.is_regular_file()) take(it.path());
    } else {
        for (const auto& it : fs::directory_iterator(src, ec))
            if (it.is_regular_file()) take(it.path());
    }
    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) { return a.generic_string() < b.generic_string(); });
    return out;
}

bool officialFolder(const fs::path& root, const std::string& id) {
    std::string list;
    if (root.empty() || !readText(root / "extensions" / "OFFICIAL", list)) return false;
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find('\n', start);
        if (end == std::string::npos) end = list.size();
        std::string line = list.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line == id) return true;
        start = end + 1;
    }
    return false;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (size_t p = 0; (p = s.find(from, p)) != std::string::npos; p += to.size()) s.replace(p, from.size(), to);
    return s;
}

}  // namespace

bool buildFolder(const Args& a, const fs::path& dirIn, Built& out) {
    std::error_code ec;
    const fs::path dir = fs::absolute(dirIn, ec);
    std::string manifestText;
    if (!readText(dir / kManifestFile, manifestText)) {
        std::fprintf(stderr, "%s: %s\n", tr("erro: a pasta não tem manifest.json", "error: the folder has no manifest.json"),
                     dir.string().c_str());
        return false;
    }
    const fs::path root = repoRoot(dir);
    if (root.empty()) {
        std::fprintf(stderr, "%s\n", tr("erro: o SDK (sdk/include/mobilog_ext.h) não foi achado; rode dentro do repositório mobilog-extensions",
                                       "error: the SDK (sdk/include/mobilog_ext.h) was not found; run inside the mobilog-extensions repository"));
        return false;
    }
    std::vector<std::string> problems;
    Manifest m;
    // Reserved ids ("mobilog.*") only for the folders listed in extensions/OFFICIAL.
    {
        Manifest probe;
        std::vector<std::string> ignored;
        parseManifest(manifestText, {"", true}, probe, ignored);
        const bool official = a.mobilog || officialFolder(root, probe.id);
        if (!parseManifest(manifestText, {"", official}, m, problems)) {
            std::fprintf(stderr, "manifest.json:\n");
            printProblems(problems);
            return false;
        }
    }
    const fs::path sdk = wasiSdk(a, root);
    if (sdk.empty()) {
        std::fprintf(stderr, "%s\n", tr("erro: wasi-sdk não encontrado (scripts/setup.sh baixa a versão fixada; ou --wasi-sdk <dir> / WASI_SDK_PREFIX)",
                                       "error: wasi-sdk not found (scripts/setup.sh downloads the pinned version; or --wasi-sdk <dir> / WASI_SDK_PREFIX)"));
        return false;
    }
    BuildConfig cfg;
    if (!readBuildConfig(dir, cfg)) return false;
    std::vector<fs::path> sources = sourcesOf(dir, cfg);
    if (sources.empty()) {
        std::fprintf(stderr, "%s\n", tr("erro: nenhum fonte (.c, .cc, .cpp) em src/", "error: no sources (.c, .cc, .cpp) in src/"));
        return false;
    }
    const bool vrp = std::find(cfg.libraries.begin(), cfg.libraries.end(), "vrp") != cfg.libraries.end();
    if (vrp) {
        sources.push_back(root / "sdk" / "contracts" / "src" / "Vrp.cpp");
        sources.push_back(root / "sdk" / "contracts" / "src" / "VrpWire.cpp");
    }
    const fs::path work = dir / "dist" / "obj";
    fs::create_directories(work, ec);

#if defined(_WIN32)
    const std::string exe = ".exe";
#else
    const std::string exe;
#endif
    const std::string clang = (sdk / "bin" / ("clang" + exe)).string(), clangxx = (sdk / "bin" / ("clang++" + exe)).string();
    const std::vector<std::string> features = {"-mcpu=mvp", "-mbulk-memory", "-msign-ext", "-mnontrapping-fptoint", "-mmutable-globals"};
    std::vector<std::string> common;
    if (cfg.wasi) {
        common = {"--target=wasm32-wasip1", "--sysroot=" + (sdk / "share" / "wasi-sysroot").string()};
    } else {
        common = {"--target=wasm32", "-nostdlib", "-fno-builtin"};
    }
    common.insert(common.end(), features.begin(), features.end());
    common.push_back("-" + cfg.optimize);
    common.push_back("-DNDEBUG");
    for (const auto& d : cfg.defines) common.push_back("-D" + d);
    const std::vector<std::string> includes = {"-I" + (root / "sdk" / "include").string(), "-I" + (root / "sdk" / "contracts" / "include").string(),
                                               "-I" + (root / "mlx" / "include").string(), "-I" + (dir / "src").string()};
    bool anyCxx = false;
    std::vector<std::string> objects;
    for (size_t i = 0; i < sources.size(); ++i) {
        const fs::path& s = sources[i];
        const bool cxx = s.extension() != ".c";
        anyCxx = anyCxx || cxx;
        const fs::path obj = work / (std::to_string(i) + "-" + s.stem().string() + ".o");
        std::vector<std::string> cmd = {cxx ? clangxx : clang};
        cmd.insert(cmd.end(), common.begin(), common.end());
        if (cxx) {
            cmd.insert(cmd.end(), {"-std=c++20", "-fno-exceptions", "-fno-rtti"});
        } else {
            cmd.push_back("-std=c17");
        }
        cmd.insert(cmd.end(), includes.begin(), includes.end());
        cmd.insert(cmd.end(), {"-Wall", "-Wextra", "-c", s.string(), "-o", obj.string()});
        if (run(cmd, a.verbose) != 0) {
            std::fprintf(stderr, "%s: %s\n", tr("erro ao compilar", "compile error in"), s.string().c_str());
            return false;
        }
        objects.push_back(obj.string());
    }
    out.wasm = dir / "dist" / "extension.wasm";
    std::vector<std::string> link = {anyCxx && cfg.wasi ? clangxx : clang};
    link.insert(link.end(), common.begin(), common.end());
    if (anyCxx) link.insert(link.end(), {"-std=c++20", "-fno-exceptions", "-fno-rtti"});
    if (cfg.wasi) {
        // A reactor (no main); no wasm-opt from the PATH (the bytes would depend on the machine);
        // call_indirect's table index in one byte (wasi-sdk's libraries leave 5, which the app refuses).
        link.insert(link.end(), {"-mexec-model=reactor", "--no-wasm-opt", "-Wl,--compress-relocations"});
    } else {
        link.push_back("-Wl,--no-entry");
    }
    link.insert(link.end(), {"-Wl,--strip-all", "-Wl,--max-memory=" + std::to_string(uint64_t(m.memoryMB) << 20)});
    link.insert(link.end(), objects.begin(), objects.end());
    link.insert(link.end(), {"-o", out.wasm.string()});
    if (run(link, a.verbose) != 0) {
        std::fprintf(stderr, "%s\n", tr("erro ao ligar o módulo", "link error"));
        return false;
    }
    fs::remove_all(work, ec);

    // The package: the manifest as written, the module, the files it names, a LICENSE if there is one.
    std::vector<ZipFile> files;
    files.push_back({kManifestFile, Bytes(manifestText.begin(), manifestText.end())});
    ZipFile wasm{m.entry, {}};
    if (!readFile(out.wasm, wasm.data)) return false;
    files.push_back(std::move(wasm));
    std::vector<std::string> named = m.namedFiles();
    if (fs::is_regular_file(dir / "LICENSE", ec)) named.push_back("LICENSE");
    for (const std::string& f : named) {
        if (std::any_of(files.begin(), files.end(), [&](const ZipFile& z) { return z.name == f; })) continue;
        ZipFile z{f, {}};
        if (!readFile(dir / f, z.data)) {
            std::fprintf(stderr, "%s %s\n", tr("erro: o manifesto cita um arquivo que não existe:", "error: the manifest names a missing file:"),
                         f.c_str());
            return false;
        }
        files.push_back(std::move(z));
    }
    std::sort(files.begin() + 1, files.end(), [](const ZipFile& x, const ZipFile& y) { return x.name < y.name; });
    const Bytes mlx = buildPackage(files, nullptr);
    const bool official = a.mobilog || officialFolder(root, m.id);
    const PackageCheck c = checkPackage(mlx, {"", official});
    out.mlx = dir / "dist" / (m.id + "-" + m.version + ".mlx");
    if (report(c, out.mlx.filename().string())) return false;
    if (!writeFile(out.mlx, mlx.data(), mlx.size())) {
        std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível gravar", "error: could not write"), out.mlx.string().c_str());
        return false;
    }
    out.manifest = c.manifest;
    if (!a.quiet)
        std::printf("%s: %s %s · %s %.1f KB · %s %.1f KB\n", fs::relative(out.mlx, fs::current_path(), ec).string().c_str(), m.name.c_str(),
                    m.version.c_str(), tr("módulo", "module"), double(fs::file_size(out.wasm, ec)) / 1024.0, tr("pacote", "package"),
                    double(mlx.size()) / 1024.0);
    return true;
}

int cmdBuild(const Args& a) {
    if (a.positional.size() > 1) return usage();
    const fs::path dir = a.positional.empty() ? fs::current_path() : fs::path(a.positional[0]);
    Built b;
    if (!a.watch) return buildFolder(a, dir, b) ? 0 : 1;
    // --watch: builds again when a file of the folder changes (the app's development folder picks
    // the new package up).
    const auto stamp = [&] {
        std::error_code ec;
        fs::file_time_type newest{};
        for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); ++it) {
            const std::string rel = fs::relative(it->path(), dir, ec).generic_string();
            if (rel.rfind("dist", 0) == 0 || rel.rfind(".", 0) == 0) {
                if (it->is_directory()) it.disable_recursion_pending();
                continue;
            }
            if (it->is_regular_file()) newest = std::max(newest, it->last_write_time(ec));
        }
        return newest;
    };
    std::printf("%s\n", tr("Observando a pasta (Ctrl+C para parar)...", "Watching the folder (Ctrl+C to stop)..."));
    fs::file_time_type last{};
    for (;;) {
        const fs::file_time_type now = stamp();
        if (now != last) {
            last = now;
            buildFolder(a, dir, b);
            std::fflush(stdout);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int cmdNew(const Args& a) {
    if (a.positional.size() != 1) return usage();
    const std::string id = a.positional[0];
    if (!validExtensionId(id)) {
        std::fprintf(stderr, "%s\n", tr("erro: id inválido; use letras minúsculas, números e '-', em partes separadas por ponto (ex.: io.github.seu-usuario.minha-extensao)",
                                       "error: invalid id; use lowercase letters, digits and '-', in dot-separated parts (e.g. io.github.your-user.my-extension)"));
        return 1;
    }
    if (id.rfind("mobilog.", 0) == 0 && !a.mobilog) {
        std::fprintf(stderr, "%s\n", tr("erro: ids \"mobilog.*\" são reservados", "error: \"mobilog.*\" ids are reserved"));
        return 1;
    }
    const fs::path root = repoRoot();
    const std::string lang = a.lang.empty() ? "cpp" : a.lang;
    const fs::path tpl = root / "templates" / lang;
    std::error_code ec;
    if (root.empty() || !fs::is_directory(tpl, ec)) {
        std::fprintf(stderr, "%s templates/%s\n", tr("erro: modelo não encontrado:", "error: template not found:"), lang.c_str());
        return 1;
    }
    const fs::path dest = fs::is_directory(fs::current_path() / "extensions", ec) ? fs::current_path() / "extensions" / id : fs::current_path() / id;
    if (fs::exists(dest, ec)) {
        std::fprintf(stderr, "%s %s\n", tr("erro: já existe:", "error: already exists:"), dest.string().c_str());
        return 1;
    }
    std::string name = a.name;
    if (name.empty()) {
        name = id.substr(id.rfind('.') + 1);
        for (char& c : name)
            if (c == '-') c = ' ';
        if (!name.empty()) name[0] = char(std::toupper(static_cast<unsigned char>(name[0])));
    }
    const std::time_t t = std::time(nullptr);
    char year[8];
    std::strftime(year, sizeof year, "%Y", std::localtime(&t));
    const std::map<std::string, std::string> vars = {
        {"{{ID}}", id}, {"{{NAME}}", name}, {"{{AUTHOR}}", a.author.empty() ? std::string(tr("Seu nome", "Your name")) : a.author}, {"{{YEAR}}", year}};
    for (auto it = fs::recursive_directory_iterator(tpl, ec); it != fs::recursive_directory_iterator(); ++it) {
        const fs::path rel = fs::relative(it->path(), tpl, ec);
        if (it->is_directory()) {
            fs::create_directories(dest / rel, ec);
            continue;
        }
        Bytes data;
        if (!readFile(it->path(), data)) return 1;
        const std::string ext = it->path().extension().string();
        if (ext != ".png" && ext != ".jpg") {
            std::string text(data.begin(), data.end());
            for (const auto& [k, v] : vars) text = replaceAll(text, k, v);
            data.assign(text.begin(), text.end());
        }
        fs::create_directories((dest / rel).parent_path(), ec);
        if (!writeFile(dest / rel, data.data(), data.size())) return 1;
    }
    std::printf(tr("Criada: %s\nPróximos passos:\n  mobilog_ext build %s     compila e empacota\n  mobilog_ext test %s      roda sem o app\n  mobilog_ext check %s     confere como a loja\n",
                   "Created: %s\nNext steps:\n  mobilog_ext build %s     compiles and packs\n  mobilog_ext test %s      runs it without the app\n  mobilog_ext check %s     checks it as the store does\n"),
                dest.string().c_str(), fs::relative(dest, fs::current_path(), ec).string().c_str(),
                fs::relative(dest, fs::current_path(), ec).string().c_str(), fs::relative(dest, fs::current_path(), ec).string().c_str());
    return 0;
}

}  // namespace mlxtool
