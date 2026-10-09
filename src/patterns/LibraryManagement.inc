#include "LibraryMetadata.h"
#include <new>
LibraryMeta::Store libraryMetadata;
bool libraryMetadataReadable=true;
uint32_t libraryRevision=0;
uint8_t libraryStorageSlot=255;
size_t libraryStoredLength=0;
uint8_t libraryStoredPrefix[4]{};
const char* const libraryKeys[2]={"libmeta_a","libmeta_b"};
uint32_t libraryHash(const uint8_t* p,size_t n){uint32_t h=2166136261U;for(size_t i=0;i<n;i++)h=(h^p[i])*16777619U;return h;}
void libraryPut32(uint8_t* p,uint32_t v){for(int i=0;i<4;i++)p[i]=(v>>(8*i))&255;}
uint32_t libraryGet32(const uint8_t* p){uint32_t v=0;for(int i=0;i<4;i++)v|=uint32_t(p[i])<<(8*i);return v;}
void loadLibraryMetadata(){
 Preferences store;
 if(!store.begin("matrix-cfg",true))return;
 bool hadJournal=false,found=false;
 auto* candidate=new(std::nothrow) LibraryMeta::Store;
 if(!candidate){libraryMetadataReadable=false;store.end();return;}
 for(int slot=0;slot<2;slot++){
  if(!store.isKey(libraryKeys[slot]))continue;hadJournal=true;
  size_t n=store.getBytesLength(libraryKeys[slot]);if(n<14||n>LibraryMeta::FlashBudget+10)continue;
  auto* bytes=new(std::nothrow) uint8_t[n];
  if(bytes&&store.getBytes(libraryKeys[slot],bytes,n)==n&&bytes[0]=='J'&&bytes[1]==2&&
    libraryGet32(bytes+6)==libraryHash(bytes+10,n-10)&&LibraryMeta::decode(bytes+10,n-10,*candidate)){
   uint32_t generation=libraryGet32(bytes+2);
   if(!found||int32_t(generation-libraryRevision)>0){libraryMetadata=*candidate;libraryRevision=generation;libraryStorageSlot=slot;libraryStoredLength=n;found=true;}
  }
  delete[] bytes;
 }
 delete candidate;
 if(found||hadJournal){libraryMetadataReadable=found;store.end();return;}
 if(store.isKey("libmeta")){
  const size_t length=store.getBytesLength("libmeta");
  libraryStoredLength=length;
  if(length&&length<=sizeof(libraryMetadata)){
   auto* diagnostic=new(std::nothrow) uint8_t[length];
   if(diagnostic&&store.getBytes("libmeta",diagnostic,length)==length)memcpy(libraryStoredPrefix,diagnostic,min(length,size_t(4)));
   delete[] diagnostic;
  }
  if(length==2232){
   auto* bytes=new(std::nothrow) uint8_t[length];
   libraryMetadataReadable=bytes&&store.getBytes("libmeta",bytes,length)==length&&LibraryMeta::decodeLegacy(bytes,length,libraryMetadata)&&LibraryMeta::valid(libraryMetadata);
   delete[] bytes;
  }else if(length==sizeof(libraryMetadata)){
   // Read the first deployed fixed-size format without losing user data.
   libraryMetadataReadable=store.getBytes("libmeta",&libraryMetadata,sizeof(libraryMetadata))==sizeof(libraryMetadata) && LibraryMeta::valid(libraryMetadata);
  }else if(length>=4&&length<=LibraryMeta::FlashBudget){
   auto* bytes=new(std::nothrow) uint8_t[length];
   libraryMetadataReadable=bytes&&store.getBytes("libmeta",bytes,length)==length&&LibraryMeta::decode(bytes,length,libraryMetadata);
   delete[] bytes;
  }else libraryMetadataReadable=false;
  if(!libraryMetadataReadable)libraryMetadata=LibraryMeta::Store{}; // Do not overwrite an unknown/corrupt blob.
 }
 store.end();
}
String buildLibraryJson(){
 DynamicJsonDocument doc(12288);
 doc["success"]=true;doc["revision"]=libraryRevision;doc["readable"]=libraryMetadataReadable;
 doc["storageBytes"]=LibraryMeta::encodedSize(libraryMetadata);doc["storageBudget"]=LibraryMeta::FlashBudget;
 doc["storedLength"]=libraryStoredLength;JsonArray prefix=doc.createNestedArray("storedPrefix");for(auto b:libraryStoredPrefix)prefix.add(b);
 doc["storageSlot"]=libraryStorageSlot;
 auto folders=doc.createNestedArray("folders"),families=doc.createNestedArray("families"),tags=doc.createNestedArray("tags");
 for(auto& s:libraryMetadata.folders)folders.add(s);
 for(auto& s:libraryMetadata.families)families.add(s);
 for(auto& s:libraryMetadata.tags)tags.add(s);
 auto entries=doc.createNestedArray("entries");
 for(int id=0;id<LibraryMeta::Records;id++){
  const auto& e=libraryMetadata.entries[id];if(!(e.rating||e.folder||e.family||e.tags||e.reserved))continue;
  auto row=entries.createNestedArray();row.add(id);row.add(e.rating);row.add(e.folder);row.add(e.family);row.add(e.tags);
  row.add(LibraryMeta::archived(e));
 }
 String result;serializeJson(doc,result);return result;
}
bool libraryText(JsonVariantConst value,char* target,size_t capacity){
 if(!value.is<const char*>())return false;
 const char* source=value.as<const char*>();size_t n=strlen(source);
 // Limit UTF-8 bytes, not just displayed characters; reject controls/commas.
 if(n>=capacity)return false;
 for(size_t i=0;i<n;i++)if((uint8_t)source[i]<32||source[i]==','||source[i]==127)return false;
 memcpy(target,source,n+1);return true;
}
bool commitLibraryMetadata(const LibraryMeta::Store& next,String& message){
 const size_t payload=LibraryMeta::encodedSize(next),length=payload+10;
 if(payload>LibraryMeta::FlashBudget){message="Library budget reached: shorten names or remove unused tags";return false;}
 auto* bytes=new(std::nothrow) uint8_t[length];auto* check=new(std::nothrow) uint8_t[length];
 if(!bytes||!check){delete[] bytes;delete[] check;message="Not enough memory to encode library";return false;}
 bytes[0]='J';bytes[1]=2;libraryPut32(bytes+2,libraryRevision+1);
 if(LibraryMeta::encode(next,bytes+10,payload)!=payload){delete[] bytes;delete[] check;message="Could not encode library";return false;}
 libraryPut32(bytes+6,libraryHash(bytes+10,payload));
 // Never replace the only valid copy. Erase only the inactive backup key,
 // then write/read-verify its replacement before switching generations.
 const uint8_t slot=libraryStorageSlot==0?1:0;
#if defined(MATRIX_HUB75_DMA)
 matrix.stop();
#endif
 Preferences store;bool ok=store.begin("matrix-cfg",false);
 if(ok){
  if(store.isKey(libraryKeys[slot]))ok=store.remove(libraryKeys[slot]);
  if(ok)ok=store.putBytes(libraryKeys[slot],bytes,length)==length&&store.getBytesLength(libraryKeys[slot])==length&&store.getBytes(libraryKeys[slot],check,length)==length&&!memcmp(bytes,check,length);
  // The legacy key was created only by this feature; migrate only AFTER a
  // verified journal copy exists. No other preferences or namespace are erased.
  if(ok&&store.isKey("libmeta"))store.remove("libmeta");
  store.end();
 }
#if defined(MATRIX_HUB75_DMA)
 matrix.resume();
#endif
 if(ok){libraryMetadata=next;libraryMetadataReadable=true;libraryStorageSlot=slot;libraryStoredLength=length;libraryRevision++;}
 delete[] bytes;delete[] check;
 message=ok?"Rating, folder, family and tags saved on matrix":"Save could not be verified; previous journal copy retained";
 return ok;
}
bool resetUnreadableLibrary(JsonVariantConst confirmation,String& message){
 if(libraryMetadataReadable||!confirmation.is<const char*>()||strcmp(confirmation.as<const char*>(),"RESET UNREADABLE LIBRARY")){message="Reset is only available for unreadable library metadata";return false;}
 auto* empty=new(std::nothrow) LibraryMeta::Store;
 if(!empty){message="Not enough memory";return false;}
 bool ok=commitLibraryMetadata(*empty,message);delete empty;return ok;
}
bool saveLibraryMetadata(JsonObjectConst value,String& message){
 if(!libraryMetadataReadable){message="Library storage is unreadable; existing data was preserved";return false;}
 int id=value["id"]|(-1),rating=value["rating"]|(-1);
 if(!value["id"].is<int>()||!value["rating"].is<int>()||rating<0||rating>5||id<0||id>=LibraryMeta::Records||
   (id<16?!isAvailableEngine(id):!isAvailableVariant(id-16))){message="Invalid pattern or rating (0–5)";return false;}
 char folder[25],family[49],tags[4][21]{};
 JsonArrayConst tagList=value["tags"].as<JsonArrayConst>();
 if(!libraryText(value["folder"],folder,sizeof(folder))||!libraryText(value["family"],family,sizeof(family))||tagList.isNull()||tagList.size()>4){
  message="Use folder ≤24 bytes, family ≤48 bytes, and up to 4 tags";return false;}
 for(size_t i=0;i<tagList.size();i++)if(!libraryText(tagList[i],tags[i],sizeof(tags[i]))||!tags[i][0]){message="Each tag needs 1–20 UTF-8 bytes";return false;}
 auto* next=new(std::nothrow) LibraryMeta::Store(libraryMetadata);
 if(!next){message="Not enough memory to save library";return false;}
 next->entries[id]={};LibraryMeta::reclaim(*next);
 int f=LibraryMeta::intern(next->folders,folder),g=LibraryMeta::intern(next->families,family);
 bool ok=f>=0&&g>=0;uint32_t mask=0;
 for(size_t i=0;i<tagList.size();i++){int tag=LibraryMeta::intern(next->tags,tags[i]);if(tag<1)ok=false;else mask|=1UL<<(tag-1);}
 if(!ok){delete next;message="Library limit: 12 folders, 16 custom families, 24 shared tags";return false;}
 next->entries[id]={uint8_t(rating),uint8_t(f),uint8_t(g),libraryMetadata.entries[id].reserved,mask};
 if(memcmp(next,&libraryMetadata,sizeof(*next))==0){delete next;message="Library already saved";return true;}
 ok=commitLibraryMetadata(*next,message);delete next;
 return ok;
}
bool isPatternArchived(int engine,int variant){
 int id=engine==PATTERNFLOW_COLLECTION?16+variant:engine;
 return id>=0&&id<LibraryMeta::Records&&LibraryMeta::archived(libraryMetadata.entries[id]);
}
bool isGalleryArchived(int index){
 return index<CORE_PATTERN_COUNT?isPatternArchived(CORE_PATTERN_IDS[index],0):
  isPatternArchived(PATTERNFLOW_COLLECTION,COLLECTION_PATTERN_IDS[index-CORE_PATTERN_COUNT]);
}
bool savePatternArchive(JsonObjectConst value,String& message){
 int id=value["id"]|(-1);
 if(!libraryMetadataReadable){message="Library storage unreadable; preserved";return false;}
 if(!value["id"].is<int>()||!value["archived"].is<bool>()||id<0||id>=LibraryMeta::Records||
  (id<16?!isAvailableEngine(id):!isAvailableVariant(id-16))){message="Invalid archive request";return false;}
 const bool archived=value["archived"].as<bool>();
 if(LibraryMeta::archived(libraryMetadata.entries[id])==archived){message="Already saved";return true;}
 auto* next=new(std::nothrow) LibraryMeta::Store(libraryMetadata);
 if(!next){message="Not enough memory";return false;}
 next->entries[id].reserved=archived?1:0;
 bool ok=commitLibraryMetadata(*next,message);delete next;
 if(ok)message=archived?"Archived — presets and ratings kept":"Restored to library";
 return ok;
}
