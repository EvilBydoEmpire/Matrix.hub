#include "../src/patterns/PatternStore.h"
#include "../src/patterns/PatternCatalog.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
// Keyed NVS blobs; corrupt flips the first byte read back.
struct Store {
  std::map<std::string,std::vector<uint8_t>> blobs;
  bool corrupt=false;
  bool isKey(const char* key){return blobs.count(key)>0;}
  size_t getBytesLength(const char* key){return isKey(key)?blobs[key].size():0;}
  size_t getBytes(const char* key,void* data,size_t size){
    const auto& bytes=blobs[key];if(size<bytes.size())return 0;
    memcpy(data,bytes.data(),bytes.size());if(corrupt)static_cast<uint8_t*>(data)[0]^=1;
    return bytes.size();
  }
  size_t putBytes(const char* key,const void* data,size_t size){
    blobs[key].assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size);return size;
  }
  bool remove(const char* key){return blobs.erase(key)==1;}
};
int main(){
  assert(patternId(14,0)==14&&patternId(16,47)==63&&PATTERN_ID_COUNT==84);
  assert(!strcmp(PatternStore::Key(14).text,"p14")&&!strcmp(PatternStore::Key(PATTERN_ID_COUNT-1).text,"p83"));
  Store store;const uint8_t record[]={1,2,3,4,5};uint8_t read[sizeof(record)]{};
  assert(!PatternStore::read(store,14,read,sizeof(read)));  // Without a record a pattern plays its defaults.
  assert(PatternStore::write(store,14,record,sizeof(record)));
  assert(PatternStore::read(store,14,read,sizeof(read))&&!memcmp(read,record,sizeof(record)));
  assert(!PatternStore::read(store,14,read,sizeof(read)-1));  // A record of another layout is ignored.
  assert(!PatternStore::read(store,63,read,sizeof(read)));    // Records belong to one stable ID.
  store.corrupt=true;assert(!PatternStore::write(store,63,record,sizeof(record)));store.corrupt=false;
  assert(PatternStore::forget(store,14)&&!store.isKey("p14"));
  assert(PatternStore::forget(store,14));  // Forgetting a pattern without a record succeeds.
  puts("PASS stable IDs, record keys, round trip, layout and ID mismatches, verified writes and forgetting");
}
