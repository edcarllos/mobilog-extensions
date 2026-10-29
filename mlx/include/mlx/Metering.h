#pragma once
// The metering pass: makes every extension stoppable in the browser (including its workers, where
// nothing else can interrupt a running loop). The native app's wasmtime stops them by itself.
//
// The module is rewritten when it is loaded (the stored package keeps the signed original):
// - a new mutable i32 global holds the "fuel" left;
// - at the start of every loop iteration and of every function that calls others (recursion) the
//   fuel goes down by one; when it runs out, the module calls mobilog.check() (imported if the
//   module did not import it already), which returns the next slice of fuel, or 0 to stop: the
//   module then hits `unreachable` (a trap).
// The app decides in check() (a few thousand times per second at most) whether the call has run
// past its time, was cancelled or the extension was turned off. Custom sections (names, producers)
// are dropped. The input must pass parseWasm.
#include "mlx/Bytes.h"

#include <string>

namespace mobilog::ext {

constexpr int32_t kMeterSlice = 10000;  // loop iterations or calls between two checks

bool meterModule(const Bytes& in, Bytes& out, std::string* error);

}  // namespace mobilog::ext
