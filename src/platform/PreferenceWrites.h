#pragma once
#include <stdlib.h>
#include <string.h>

// Verify NVS blobs before acknowledging a save. Allocate before writing so a
// low-memory failure cannot begin an operation that we cannot read back.
template<class Store>
bool writeVerifiedBlob(Store& store, const char* key, const void* data, size_t size) {
  void* check = malloc(size);
  if (!check) return false;
  const bool ok = store.putBytes(key, data, size) == size &&
    store.getBytesLength(key) == size && store.getBytes(key, check, size) == size &&
    memcmp(data, check, size) == 0;
  free(check);
  return ok;
}

template<class Store>
bool clearPreferencesNamespace(Store& store, const char* name) {
  if (!store.begin(name, false)) return false;
  const bool ok = store.clear();
  store.end();
  return ok;
}
