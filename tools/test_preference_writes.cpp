#include "../src/platform/PreferenceWrites.h"
#include <cassert>
#include <cstdint>
#include <vector>
#include <cstdio>
struct Store {
  enum Failure { None, ShortWrite, WrongLength, ShortRead, Corrupt } failure=None;
  std::vector<uint8_t> bytes;
  bool opens=true,clears=true,ended=false;
  bool begin(const char*,bool){return opens;}
  bool clear(){if(clears)bytes.clear();return clears;}
  void end(){ended=true;}
  size_t putBytes(const char*,const void* data,size_t size){
    bytes.assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size);
    return failure==ShortWrite?size-1:size;
  }
  size_t getBytesLength(const char*){return bytes.size()+(failure==WrongLength);}
  size_t getBytes(const char*,void* data,size_t size){
    memcpy(data,bytes.data(),size);
    if(failure==Corrupt)static_cast<uint8_t*>(data)[0]^=1;
    return failure==ShortRead?size-1:size;
  }
};
int main(){
  Store store;const uint8_t data[]={1,4,9,16};
  assert(writeVerifiedBlob(store,"p0",data,sizeof(data)));
  for(auto failure:{Store::ShortWrite,Store::WrongLength,Store::ShortRead,Store::Corrupt}){
    store.failure=failure;assert(!writeVerifiedBlob(store,"p0",data,sizeof(data)));
  }
  store.opens=false;assert(!clearPreferencesNamespace(store,"wifi-list"));assert(!store.ended);
  store.opens=true;store.clears=false;assert(!clearPreferencesNamespace(store,"wifi-list"));assert(store.ended);
  store.clears=true;assert(clearPreferencesNamespace(store,"wifi-list"));assert(store.bytes.empty());
  puts("PASS verified namespace erase and NVS writes reject short writes, wrong length, short reads and corrupted readback");
}
