#pragma once
#include <stdint.h>
#include <string.h>

// Stable IDs, never gallery positions: 0..15 native, 16..79 imported variants.
// Dictionary entries are shared and reclaimed when no pattern refers to them.
namespace LibraryMeta {
constexpr int Records=96, Folders=12, Families=16, Tags=24;
// reserved bit 0 is reversible archival; preserve the original RAM layout.
struct Entry { uint8_t rating,folder,family,reserved; uint32_t tags; };
inline bool archived(const Entry& e){return (e.reserved&1)!=0;}
// Bounded search also supports an empty active library (returns -1).
template<class Predicate> int nextActive(int current,int count,Predicate hidden){
 for(int step=1;step<=count;step++){int next=(current+step+count)%count;if(!hidden(next))return next;}
 return -1;
}
struct Store {
 uint32_t version=1;
 Entry entries[Records]{};
 char folders[Folders][25]{};
 char families[Families][49]{};
 char tags[Tags][21]{};
};
static_assert(sizeof(Store)==2360,"Check metadata migration when changing layout");
// First deployment stored 80 entries directly. Sparse LM2/LM3 stays compatible.
inline bool decodeLegacy(const uint8_t* bytes,size_t n,Store& s){
 if(n!=2232)return false;
 s=Store{};memcpy(&s.version,bytes,4);
 memcpy(s.entries,bytes+4,80*sizeof(Entry));
 memcpy(s.folders,bytes+4+80*sizeof(Entry),sizeof(s.folders));
 memcpy(s.families,bytes+4+80*sizeof(Entry)+sizeof(s.folders),sizeof(s.families));
 memcpy(s.tags,bytes+4+80*sizeof(Entry)+sizeof(s.folders)+sizeof(s.families),sizeof(s.tags));
 return true;
}
inline bool valid(const Store& s){
 if(s.version!=1)return false;
 for(auto& e:s.entries)if(e.rating>5||e.folder>Folders||e.family>Families||e.reserved>1||e.tags>>Tags)return false;
 for(auto& name:s.folders)if(!memchr(name,0,sizeof(name)))return false;
 for(auto& name:s.families)if(!memchr(name,0,sizeof(name)))return false;
 for(auto& name:s.tags)if(!memchr(name,0,sizeof(name)))return false;
 for(auto& e:s.entries){
  if(e.folder&&!s.folders[e.folder-1][0])return false;
  if(e.family&&!s.families[e.family-1][0])return false;
  for(int i=0;i<Tags;i++)if((e.tags&(1UL<<i))&&!s.tags[i][0])return false;
 }
 return true;
}
inline void reclaim(Store& s){
 bool folders[Folders]{},families[Families]{};uint32_t tags=0;
 for(auto& e:s.entries){if(e.folder)folders[e.folder-1]=true;if(e.family)families[e.family-1]=true;tags|=e.tags;}
 for(int i=0;i<Folders;i++)if(!folders[i])memset(s.folders[i],0,sizeof(s.folders[i]));
 for(int i=0;i<Families;i++)if(!families[i])memset(s.families[i],0,sizeof(s.families[i]));
 for(int i=0;i<Tags;i++)if(!(tags&(1UL<<i)))memset(s.tags[i],0,sizeof(s.tags[i]));
}
template<size_t N,size_t L> int intern(char (&names)[N][L],const char* name){
 if(!*name)return 0;
 if(strlen(name)>=L)return -1;
 for(size_t i=0;i<N;i++)if(!strcmp(names[i],name))return i+1;
 for(size_t i=0;i<N;i++)if(!names[i][0]){strcpy(names[i],name);return i+1;}
 return -1;
}
// NVS keeps the old blob until a new version is committed. Writing the entire
// fixed RAM structure needs twice its flash space and can exhaust a 20 KiB NVS
// partition. LM3 serializes only occupied rows/names; empty library = 4 bytes.
// LM2 remains readable; LM3 uses rating bit 7 for the archived flag.
constexpr size_t FlashBudget=1400;
inline size_t encodedSize(const Store& s){
 size_t n=4;
 for(auto& e:s.entries)if(e.rating||e.folder||e.family||e.tags||e.reserved)n+=8;
 for(auto& name:s.folders)if(name[0])n+=3+strlen(name);
 for(auto& name:s.families)if(name[0])n+=3+strlen(name);
 for(auto& name:s.tags)if(name[0])n+=3+strlen(name);
 return n;
}
inline size_t encode(const Store& s,uint8_t* bytes,size_t capacity){
 size_t n=encodedSize(s);if(!valid(s)||capacity<n)return 0;
 size_t p=0;bytes[p++]='L';bytes[p++]='M';bytes[p++]=3;
 for(int i=0;i<Records;i++){const auto& e=s.entries[i];if(!(e.rating||e.folder||e.family||e.tags||e.reserved))continue;
  bytes[p++]=0;bytes[p++]=i;bytes[p++]=e.rating|(e.reserved<<7);bytes[p++]=e.folder;bytes[p++]=e.family;
  for(int shift=0;shift<24;shift+=8)bytes[p++]=(e.tags>>shift)&255;
 }
 auto name=[&](int type,int id,const char* text){if(!*text)return;size_t length=strlen(text);
  bytes[p++]=type;bytes[p++]=id;bytes[p++]=length;memcpy(bytes+p,text,length);p+=length;};
 for(int i=0;i<Folders;i++)name(1,i,s.folders[i]);
 for(int i=0;i<Families;i++)name(2,i,s.families[i]);
 for(int i=0;i<Tags;i++)name(3,i,s.tags[i]);
 bytes[p++]=255;return p;
}
inline bool decode(const uint8_t* bytes,size_t n,Store& s){
 if(n<4||bytes[0]!='L'||bytes[1]!='M'||(bytes[2]!=2&&bytes[2]!=3))return false;
 // Reset fields without a large stack-local Store temporary.
 s.version=1;memset(s.entries,0,sizeof(s.entries));memset(s.folders,0,sizeof(s.folders));
 memset(s.families,0,sizeof(s.families));memset(s.tags,0,sizeof(s.tags));
 size_t p=3;bool seen[Records]{};
 while(p<n){int type=bytes[p++];if(type==255)return p==n&&valid(s);
  if(type==0){if(n-p<7)return false;int id=bytes[p++];if(id>=Records||seen[id])return false;seen[id]=true;
   auto& e=s.entries[id];e.rating=bytes[p++];e.folder=bytes[p++];e.family=bytes[p++];
   if(bytes[2]==3){e.reserved=e.rating>>7;e.rating&=127;}
   for(int shift=0;shift<24;shift+=8)e.tags|=uint32_t(bytes[p++])<<shift;
  }else{
   if(n-p<2)return false;int id=bytes[p++],length=bytes[p++];char* target=nullptr;size_t capacity=0;
   if(type==1&&id<Folders){target=s.folders[id];capacity=sizeof(s.folders[id]);}
   if(type==2&&id<Families){target=s.families[id];capacity=sizeof(s.families[id]);}
   if(type==3&&id<Tags){target=s.tags[id];capacity=sizeof(s.tags[id]);}
   if(!target||target[0]||!length||size_t(length)>=capacity||n-p<size_t(length)||memchr(bytes+p,0,length))return false;
   memcpy(target,bytes+p,length);target[length]=0;p+=length;
  }
 }
 return false;
}
}
