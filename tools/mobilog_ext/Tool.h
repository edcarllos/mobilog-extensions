#pragma once
// mobilog_ext: shared pieces of the commands (files, arguments, messages in Portuguese or English).
#include "mlx/Package.h"

#include <filesystem>
#include <string>
#include <vector>

namespace mlxtool {

using mobilog::ext::Bytes;
namespace fs = std::filesystem;

// Messages follow the system language: Portuguese when LANG/LC_ALL/LC_MESSAGES start with "pt",
// English otherwise (MOBILOG_EXT_LANG=pt|en forces one). Problems found by the format library
// (mlx/) are in Portuguese, as the app shows them.
bool portuguese();
inline const char* tr(const char* pt, const char* en) { return portuguese() ? pt : en; }

struct Args {
    std::vector<std::string> positional;
    std::string out, key, name, author, lang, store, wasiSdk, updatedFromGit, script;
    std::vector<std::string> trust;
    bool mobilog = false;   // pack: one of the Mobilog's own packages (ids "mobilog.*")
    bool official = false;  // check: the folder is listed in extensions/OFFICIAL
    bool requireBump = false, watch = false, verbose = false, quiet = false;
    int frames = 120;       // test: frames simulated
};
bool parseArgs(int argc, char** argv, int from, Args& a);

bool readFile(const fs::path& p, Bytes& out);
bool readText(const fs::path& p, std::string& out);
bool writeFile(const fs::path& p, const void* data, size_t size, bool secret = false);
bool loadSecret(const std::string& path, mobilog::ext::SigningKey& key);
void printProblems(const std::vector<std::string>& problems);
int report(const mobilog::ext::PackageCheck& c, const std::string& file);

// The repository (or a copy of it): the folder with sdk/include/mobilog_ext.h, found upwards from
// `from`, then from the program's own folder. Empty when not found.
fs::path repoRoot(const fs::path& from = fs::current_path());
// wasi-sdk: --wasi-sdk, WASI_SDK_PREFIX, <repo>/external/wasi-sdk, /opt/wasi-sdk. Empty when none.
fs::path wasiSdk(const Args& a, const fs::path& root);

// Package a folder of files as they are (pack) / the files a source folder's manifest names (build).
bool collectFolder(const fs::path& dir, std::vector<mobilog::ext::ZipFile>& files);

int usage();
int cmdKeygen(const Args& a);
int cmdPack(const Args& a);
int cmdSign(const Args& a);
int cmdVerify(const Args& a, bool details);
int cmdMeter(const Args& a);
int cmdNew(const Args& a);
int cmdBuild(const Args& a);
int cmdCheck(const Args& a);
int cmdCatalog(const Args& a);
int cmdTest(const Args& a);

// build: the module and the package of one source folder (used by check and test too).
struct Built {
    fs::path wasm, mlx;
    mobilog::ext::Manifest manifest;
};
bool buildFolder(const Args& a, const fs::path& dir, Built& out);

}  // namespace mlxtool
