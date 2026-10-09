#include "../src/patterns/LibraryMetadata.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace LibraryMeta;Store s;assert(valid(s));
 int folder=intern(s.folders,"Acid"),family=intern(s.families,"Warped Checkerboard"),tag=intern(s.tags,"psychedelic");
 s.entries[1]={5,uint8_t(folder),uint8_t(family),0,uint32_t(1UL<<(tag-1))};
 s.entries[16+47]=s.entries[1];reclaim(s);assert(!strcmp(s.folders[0],"Acid"));
 uint8_t bytes[2400];Store decoded;
 size_t length=encode(s,bytes,sizeof(bytes));assert(length==encodedSize(s)&&length<80);
 assert(decode(bytes,length,decoded)&&!memcmp(&s,&decoded,sizeof(s)));
 for(size_t n=0;n<length;n++)assert(!decode(bytes,n,decoded));
 assert(encode(s,bytes,length-1)==0);
 bytes[4]=255;assert(!decode(bytes,length,decoded));
 s.entries[1]={};reclaim(s);assert(s.folders[0][0]);
 s.entries[63]={1,0,0,0,0};Store copy=s;assert(!memcmp(&copy,&s,sizeof(s))&&valid(copy));
 s.entries[16+47]={};reclaim(s);assert(!s.folders[0][0]&&!s.tags[0][0]);
 assert(intern(s.folders,"")==0);assert(intern(s.tags,"123456789012345678901")==-1);
 for(int i=0;i<Tags;i++){char name[20];snprintf(name,sizeof(name),"tag%d",i);assert(intern(s.tags,name)==i+1);}
 assert(intern(s.tags,"overflow")==-1);assert(intern(s.tags,"tag2")==3);
 s.entries[1].rating=6;assert(!valid(s));s.entries[1].rating=0;s.entries[1].tags=1UL<<24;assert(!valid(s));
 Store empty;assert(encodedSize(empty)==4);assert(encode(empty,bytes,sizeof(bytes))==4);assert(decode(bytes,4,decoded));
 Store archive;archive.entries[78]={5,0,0,1,0};
 assert(encodedSize(archive)==12&&encode(archive,bytes,sizeof(bytes))==12);
 assert(decode(bytes,12,decoded)&&archived(decoded.entries[78])&&decoded.entries[78].rating==5);
 bytes[2]=2;assert(!decode(bytes,12,decoded)); // LM2 has no archive flag.
 bytes[5]=5;assert(decode(bytes,12,decoded)&&!archived(decoded.entries[78])); // old LM2 retained
 archive.entries[78].reserved=2;assert(!valid(archive));
 bool hidden[4]={false,true,false,true};
 assert(nextActive(0,4,[&](int i){return hidden[i];})==2);
 assert(nextActive(2,4,[&](int i){return hidden[i];})==0);
 hidden[0]=true;assert(nextActive(2,4,[&](int i){return hidden[i];})==2);
 hidden[2]=true;assert(nextActive(2,4,[&](int i){return hidden[i];})==-1);
 assert(nextActive(0,0,[&](int i){return hidden[i];})==-1);
 for(auto& e:empty.entries)e.rating=5;
 const size_t fullSize=4+8*Records;
 assert(encodedSize(empty)==fullSize&&encode(empty,bytes,sizeof(bytes))==fullSize&&decode(bytes,fullSize,decoded));
 uint32_t random=0x12345678;
 for(int trial=0;trial<2000;trial++){
  size_t n=4+trial%2300;
  for(size_t i=0;i<n;i++){random^=random<<13;random^=random>>17;random^=random<<5;bytes[i]=random&255;}
  bytes[0]='L';bytes[1]='M';bytes[2]=2;
  if(decode(bytes,n,decoded))assert(valid(decoded));
 }
 puts("PASS stable IDs, dictionary reuse/reclamation, limits, rating/mask validation, binary format");
}
