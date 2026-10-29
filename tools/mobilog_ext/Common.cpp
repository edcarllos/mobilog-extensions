#include "Tool.h"

#include "mlx/Signature.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace mlxtool {

using namespace mobilog::ext;

bool portuguese() {
    static const bool pt = [] {
        if (const char* f = std::getenv("MOBILOG_EXT_LANG")) return std::string(f).rfind("pt", 0) == 0;
        for (const char* v : {"LC_ALL", "LC_MESSAGES", "LANG"})
            if (const char* s = std::getenv(v); s && *s) return std::string(s).rfind("pt", 0) == 0;
        return false;
    }();
    return pt;
}

bool parseArgs(int argc, char** argv, int from, Args& a) {
    for (int i = from; i < argc; ++i) {
        const std::string s = argv[i];
        const auto value = [&](std::string& dst) {
            if (i + 1 >= argc) return false;
            dst = argv[++i];
            return true;
        };
        if (s == "-o" || s == "--out") {
            if (!value(a.out)) return false;
        } else if (s == "--key") {
            if (!value(a.key)) return false;
        } else if (s == "--name") {
            if (!value(a.name)) return false;
        } else if (s == "--author") {
            if (!value(a.author)) return false;
        } else if (s == "--lang") {
            if (!value(a.lang)) return false;
        } else if (s == "--store") {
            if (!value(a.store)) return false;
        } else if (s == "--wasi-sdk") {
            if (!value(a.wasiSdk)) return false;
        } else if (s == "--updated-from-git") {
            if (!value(a.updatedFromGit)) return false;
        } else if (s == "--script") {
            if (!value(a.script)) return false;
        } else if (s == "--frames") {
            std::string n;
            if (!value(n)) return false;
            a.frames = std::max(1, std::atoi(n.c_str()));
        } else if (s == "--mobilog") {
            a.mobilog = true;
        } else if (s == "--official") {
            a.official = true;
        } else if (s == "--require-bump") {
            a.requireBump = true;
        } else if (s == "--watch") {
            a.watch = true;
        } else if (s == "-v" || s == "--verbose") {
            a.verbose = true;
        } else if (s == "-q" || s == "--quiet") {
            a.quiet = true;
        } else if (s == "--c" || s == "--cpp") {
            a.lang = s.substr(2);
        } else if (s == "--trust") {
            std::string t;
            if (!value(t)) return false;
            a.trust.push_back(t);
        } else if (!s.empty() && s[0] == '-') {
            std::fprintf(stderr, "%s: %s\n", tr("opção desconhecida", "unknown option"), s.c_str());
            return false;
        } else {
            a.positional.push_back(s);
        }
    }
    return true;
}

bool readFile(const fs::path& p, Bytes& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

bool readText(const fs::path& p, std::string& out) {
    Bytes b;
    if (!readFile(p, b)) return false;
    out.assign(b.begin(), b.end());
    return true;
}

bool writeFile(const fs::path& p, const void* data, size_t size, bool secret) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(static_cast<const char*>(data), std::streamsize(size));
    f.close();
    if (secret) {
        std::error_code ec;
        fs::permissions(p, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
    }
    return bool(f);
}

bool loadSecret(const std::string& path, SigningKey& key) {
    std::string text;
    if (!readText(path, text) || !parseSecretKey(text, key)) {
        std::fprintf(stderr, "%s: %s\n", tr("erro: chave secreta ilegível", "error: unreadable secret key"), path.c_str());
        return false;
    }
    return true;
}

void printProblems(const std::vector<std::string>& problems) {
    for (const auto& p : problems) std::fprintf(stderr, "  - %s\n", p.c_str());
}

int report(const PackageCheck& c, const std::string& file) {
    if (!c.ok) {
        std::fprintf(stderr, "%s: %s:\n", file.c_str(), tr("pacote inválido", "invalid package"));
        printProblems(c.problems);
        return 1;
    }
    return 0;
}

namespace {
fs::path selfDir() {
#if defined(__linux__)
    std::error_code ec;
    const fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::path() : p.parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t n = sizeof buf;
    return _NSGetExecutablePath(buf, &n) == 0 ? fs::path(buf).parent_path() : fs::path();
#elif defined(_WIN32)
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n ? fs::path(std::wstring(buf, n)).parent_path() : fs::path();
#else
    return {};
#endif
}

fs::path findUp(fs::path p) {
    std::error_code ec;
    p = fs::absolute(p, ec);
    for (; !p.empty(); p = p.parent_path()) {
        if (fs::is_regular_file(p / "sdk" / "include" / "mobilog_ext.h", ec)) return p;
        if (p == p.root_path()) break;
    }
    return {};
}
}  // namespace

fs::path repoRoot(const fs::path& from) {
    if (fs::path r = findUp(from); !r.empty()) return r;
    return findUp(selfDir());
}

fs::path wasiSdk(const Args& a, const fs::path& root) {
    std::error_code ec;
    const auto ok = [&](const fs::path& p) { return !p.empty() && fs::exists(p / "bin" / "clang", ec); };
    std::vector<fs::path> c;
    if (!a.wasiSdk.empty()) c.push_back(a.wasiSdk);
    if (const char* e = std::getenv("WASI_SDK_PREFIX")) c.push_back(e);
    if (!root.empty()) c.push_back(root / "external" / "wasi-sdk");
    c.push_back("/opt/wasi-sdk");
    for (const fs::path& p : c)
        if (ok(p)) return p;
    return {};
}

bool collectFolder(const fs::path& dir, std::vector<ZipFile>& files) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); it != fs::recursive_directory_iterator(); ++it) {
        const fs::path rel = fs::relative(it->path(), dir);
        const std::string name = rel.generic_string();
        if (name.empty() || name[0] == '.' || name.find("/.") != std::string::npos) {  // hidden files and folders
            if (it->is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (it->is_symlink()) {
            std::fprintf(stderr, "%s: %s\n", tr("erro: links não são permitidos no pacote", "error: links are not allowed in a package"),
                         name.c_str());
            return false;
        }
        if (!it->is_regular_file() || name == kSignatureFile) continue;
        std::string err;
        if (!validMemberName(name, &err)) {
            std::fprintf(stderr, "%s: %s\n", tr("erro", "error"), err.c_str());
            return false;
        }
        ZipFile f{name, {}};
        if (!readFile(it->path(), f.data)) {
            std::fprintf(stderr, "%s %s\n", tr("erro: não foi possível ler", "error: could not read"), name.c_str());
            return false;
        }
        files.push_back(std::move(f));
    }
    std::sort(files.begin(), files.end(), [](const ZipFile& x, const ZipFile& y) { return x.name < y.name; });
    return true;
}

}  // namespace mlxtool
