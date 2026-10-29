#pragma once
// The Mobilog's own packages (ids "mobilog.*", which only they may use): they get the trust of the
// app's own code. Known by their bytes: the packages that came with Mobilog 0.12–0.16, before the
// store (SHA-512 of the whole file, listed here for good, so those files keep working).
#include "mlx/Bytes.h"

#include <string>

namespace mobilog::ext {

bool officialExample(const std::string& sha512Hex);
inline bool officialExample(const Bytes& mlx) {
    const Sha512 h = sha512(mlx);
    return officialExample(toHex(h.data(), h.size()));
}

}  // namespace mobilog::ext
