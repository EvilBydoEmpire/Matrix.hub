#pragma once
#include <stdint.h>
#include <stdio.h>
#include "../platform/PreferenceWrites.h"

// The matrix-cfg namespace. Settings of another version are erased at boot,
// so this layout needs no conversions.
namespace PatternStore {
constexpr uint32_t Version=17;

// An edited pattern keeps one record under "p" + its stable ID; a pattern
// without one plays its compiled defaults.
struct Key {
 char text[6];
 explicit Key(int id){snprintf(text,sizeof(text),"p%d",id);}
};
template<class Store> bool read(Store& store,int id,void* record,size_t size){
 const Key key(id);
 return store.isKey(key.text)&&store.getBytesLength(key.text)==size&&store.getBytes(key.text,record,size)==size;
}
template<class Store> bool write(Store& store,int id,const void* record,size_t size){
 const Key key(id);
 return writeVerifiedBlob(store,key.text,record,size);
}
template<class Store> bool forget(Store& store,int id){
 const Key key(id);
 return !store.isKey(key.text)||store.remove(key.text);
}

// Records whose write failed, held per pattern until a retry stores them or
// the pattern plays again and takes its edits back.
template<int Count,size_t Capacity> struct PendingRecords {
 static_assert(Capacity<256,"sizes fit a byte");
 uint8_t records[Count][Capacity];
 uint8_t sizes[Count]={};
 void keep(int id,const uint8_t* record,size_t size){memcpy(records[id],record,size);sizes[id]=size;}
 bool take(int id,uint8_t* record,size_t size){
  if(!size||sizes[id]!=size)return false;
  memcpy(record,records[id],size);sizes[id]=0;
  return true;
 }
 bool empty() const{for(uint8_t size:sizes)if(size)return false;return true;}
 // Writes every held record and reports whether all of them are stored.
 template<class Store> bool writeAll(Store& store){
  bool stored=true;
  for(int id=0;id<Count;id++)
   if(sizes[id]){if(write(store,id,records[id],sizes[id]))sizes[id]=0;else stored=false;}
  return stored;
 }
};
}
