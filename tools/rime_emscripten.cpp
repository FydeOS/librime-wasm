//
// Created by fydeos on 23-3-21.
//


#include <emscripten.h>
#include <rime_api.h>
#include <emscripten/wasmfs.h>
#include <unistd.h>

EMSCRIPTEN_KEEPALIVE extern "C" void WasmRimeInitialize();

namespace wasmfs_rime {
  backend_t my_wasmfs_create_fast_indexeddb_backend(const char* root);
}

extern "C" void WasmRimeInitialize() {
  backend_t backend = wasmfs_rime::my_wasmfs_create_fast_indexeddb_backend("rime-data");
  wasmfs_create_directory("/working", 0777, backend);
  chdir("/working");

  RIME_STRUCT(RimeTraits, traits);
  traits.app_name = "rime.wasm";
  RimeSetup(&traits);
  RimeInitialize(&traits);
}