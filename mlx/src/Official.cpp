#include "mlx/Official.h"

#include "mlx/Package.h"

#include <algorithm>
#include <mutex>
#include <iterator>

namespace mobilog::ext {

namespace {
// Mobilog 0.16.0's examples/extensions/dist: bases, busca-local, onibus, pontos-criticos, rotas.
const char* const kLegacy[] = {
    "41cf483941cbb0b195f6d309bbbb3c8af5b2e9016eaab00bf2c0b1ba47b298c6eab17f2c07e2d7e172985f37a56cbffcdb900c6247011a0a506f045db18650dc",
    "b9e56dd756bd48ccac296cb9444705a32d5804a17830dec272b7346f3051d7d6976561a9f9c9e0a1ed813bf0fba25758600d99115fdfcd928a673f3ad457201a",
    "a59955119390fcdb8484a05e8ede3716f5b329243aac7a913d6ceb1da4e6e77cb5ca7c4dcbe40e2de701de8abac5de379ce331289653291ea2a2e53044855b7f",
    "81fe7ccf4a3bde37381ab74753314065da43f74f9cb7e6dedc49bd02cd76f2593b8df71003f3c51343ff15daa1a0a80fbaadea5d02b40171e406bf8889c2b229",
    "e2e47dbd076b1969117dd3bbcee38c3d838bfaa41efd198ad2ed46e5999f757c41b47a2c0bc0a183652e677ccffa7698fa9c15f4bf4eb4429e1697b58af23569",
};
}  // namespace

bool officialExample(const std::string& sha512Hex) {
    return !sha512Hex.empty() && std::find(std::begin(kLegacy), std::end(kLegacy), sha512Hex) != std::end(kLegacy);
}

// keys/*.pub, written into StoreKeys.cpp by mlx/CMakeLists.txt.
extern const char* const kStoreKeyLines[];
extern const size_t kStoreKeyCount;

namespace {
std::mutex& keysMutex() {
    static std::mutex m;
    return m;
}
std::vector<PublicKey>& keyList() {
    static std::vector<PublicKey> keys = [] {
        std::vector<PublicKey> out;
        for (size_t i = 0; i < kStoreKeyCount; ++i) {
            PublicKey k{};
            if (parsePublicKey(kStoreKeyLines[i], k)) out.push_back(k);
        }
        return out;
    }();
    return keys;
}
}  // namespace

const std::vector<PublicKey>& storeKeys() { return keyList(); }

bool isStoreKey(const PublicKey& key) {
    std::lock_guard<std::mutex> lock(keysMutex());
    const auto& keys = keyList();
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

void addStoreKey(const PublicKey& key) {
    std::lock_guard<std::mutex> lock(keysMutex());
    auto& keys = keyList();
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
}

bool officialPackage(const PackageCheck& c) {
    return (c.storeSigned && reservedExtensionId(c.manifest.id)) || officialExample(c.sha512);
}

}  // namespace mobilog::ext
