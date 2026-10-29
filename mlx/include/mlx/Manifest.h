#pragma once
// An extension's manifest.json: identity, version, API level, permissions, what it contributes to
// the app and the limits it asks for. Everything a package says about itself is checked here, and
// the texts it shows are sanitized (no control or right-to-left characters, bounded length).
//
// {
//   "id": "br.ufsb.pontos-criticos",          reverse notation, lowercase, 3-64 chars
//   "name": "Pontos críticos", "name@en": "Hot spots",
//   "version": "1.2.0",                         semver (x.y.z, optional -pre)
//   "apiVersion": 1, "minAppVersion": "0.12.0",
//   "description": "...", "author": "...", "license": "MIT", "homepage": "https://...",
//   "entry": "extension.wasm",                  or "nativa" (built-in extensions only)
//   "icon": "icon.svg", "help": "help.md",
//   "permissions": ["mapa.ler", "simulacao.ler", "mapa.desenhar"],
//   "contributes": [{"kind": "painel", "id": "principal", "title": "Pontos críticos", "icon": "chart"}],
//   "limits": {"memoryMB": 64}
// }
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mobilog::ext {

constexpr int kApiVersion = 1;            // the highest API level this app implements
constexpr uint32_t kDefaultMemoryMB = 64;
constexpr uint32_t kMaxMemoryMB = 512;

// Permissions (a bit set). Order = the order they are listed to the user.
enum Permission : uint32_t {
    kPermMapRead = 1u << 0,
    kPermSimRead = 1u << 1,
    kPermMapDraw = 1u << 2,
    kPermMapTools = 1u << 3,
    kPermScenarioPropose = 1u << 4,
    kPermScenarioWrite = 1u << 5,
    kPermVehicles = 1u << 6,
    kPermSimExperiment = 1u << 7,
    kPermFilesExport = 1u << 8,
    kPermFilesImport = 1u << 9,
};
struct PermissionInfo {
    Permission bit;
    const char* key;     // in manifest.json
    const char* label;   // what the install dialog says ("Ler o mapa")
    const char* detail;  // one sentence of what it allows
    bool sensitive;      // changes the user's work or the simulation: highlighted
};
const std::vector<PermissionInfo>& permissionTable();
const PermissionInfo* permissionInfo(Permission p);
const PermissionInfo* permissionByKey(std::string_view key);
std::vector<Permission> permissionList(uint32_t mask);  // in table order

// What an extension adds to the app. Kinds (manifest "kind"): see contributionKindKey.
enum class ContributionKind { Panel, Tool, Layer, Chart, Kpi, Card, Solver, Problem, Analysis };
const char* contributionKindKey(ContributionKind k);
struct Contribution {
    ContributionKind kind = ContributionKind::Panel;
    std::string id;     // unique within the extension: [a-z0-9-], 1-32 chars
    std::string title;  // sanitized, <= 40 chars
    std::string icon;   // an icon name of the app's set, optional
};

struct SemVer {
    int major = 0, minor = 0, patch = 0;
    std::string pre;  // "beta.1"; a pre-release sorts before the release
};
bool parseSemVer(std::string_view s, SemVer& out);
int compareSemVer(const SemVer& a, const SemVer& b);  // <0, 0, >0
// Convenience on strings; unparsable versions sort first.
int compareVersions(std::string_view a, std::string_view b);

struct Manifest {
    std::string id, name, version, description, author, license, homepage;
    std::map<std::string, std::string> names;  // "en" -> "Hot spots" (name@<lang>)
    int apiVersion = kApiVersion;
    std::string minAppVersion;
    std::string entry = "extension.wasm";
    std::string icon, help;
    uint32_t permissions = 0;
    std::vector<Contribution> contributes;
    uint32_t memoryMB = kDefaultMemoryMB;

    bool native() const { return entry == "nativa"; }
    const Contribution* contribution(ContributionKind k, std::string_view id) const;
};

struct ManifestOptions {
    std::string appVersion;  // checks minAppVersion when set
    bool builtin = false;    // built-in extensions may use "nativa" and the reserved name/id
};
// Parses and validates. Problems are collected in Portuguese ("permissions: 'x' não existe");
// returns true when there are none.
bool parseManifest(std::string_view json, const ManifestOptions& opt, Manifest& out, std::vector<std::string>& problems);
std::string manifestJson(const Manifest& m);

bool validExtensionId(std::string_view id);

}  // namespace mobilog::ext
