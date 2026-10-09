#include <cassert>
#include <cstdint>
#include "../src/patterns/PatternflowPack.h"
#include "../src/generated/PatternCatalog.h"
int main(){
 assert(GALLERY_PATTERN_COUNT==59);
 for(int i=0;i<COLLECTION_PATTERN_COUNT;i++)assert(COLLECTION_PATTERN_IDS[i]!=56&&COLLECTION_PATTERN_IDS[i]!=57);
 for(int i=0;i<GALLERY_PATTERN_COUNT;i++){
  const int old=bookmarkStorageIndex(i);assert(old==(i<52?i:i+2));
  uint64_t saved=uint64_t(1)<<old;
  for(int j=0;j<GALLERY_PATTERN_COUNT;j++)assert(bool((saved>>bookmarkStorageIndex(j))&1)==(i==j));
 }
}
