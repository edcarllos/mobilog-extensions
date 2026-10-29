#include "mlx/Api.h"

namespace mobilog::ext {

const ApiFunctionInfo* apiFunction(uint32_t fn) {
    for (const ApiFunctionInfo& f : kApiFunctions)
        if (f.fn == fn) return &f;
    return nullptr;
}

const ApiFunctionInfo* apiFunction(std::string_view name) {
    for (const ApiFunctionInfo& f : kApiFunctions)
        if (name == f.name) return &f;
    return nullptr;
}

const char* apiFunctionName(uint32_t fn) {
    const ApiFunctionInfo* f = apiFunction(fn);
    return f ? f->name : nullptr;
}

}  // namespace mobilog::ext
