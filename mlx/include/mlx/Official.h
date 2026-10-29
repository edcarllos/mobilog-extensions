#pragma once
// The Mobilog's own packages (ids "mobilog.*", which only they may use): they get the trust of the
// app's own code. Either signed by the store's key (the store's CI signs what it builds from the
// repository; keys/*.pub, compiled in) or one of the packages that came with Mobilog 0.12–0.16,
// before the store (SHA-512 of the whole file, listed here for good, so those files keep working).
#include "mlx/Bytes.h"
#include "mlx/Signature.h"

#include <string>
#include <vector>

namespace mobilog::ext {

bool officialExample(const std::string& sha512Hex);
inline bool officialExample(const Bytes& mlx) {
    const Sha512 h = sha512(mlx);
    return officialExample(toHex(h.data(), h.size()));
}

// The store's public keys: the repository's keys/*.pub (several while a key is being replaced).
const std::vector<PublicKey>& storeKeys();
bool isStoreKey(const PublicKey& key);
void addStoreKey(const PublicKey& key);  // tests

struct PackageCheck;
// One of the Mobilog's own: signed by the store with a reserved id, or a legacy package.
bool officialPackage(const PackageCheck& check);

}  // namespace mobilog::ext
