// Gallery lookups by stable ID.
bool isAvailableEngine(int id){
 if(id==PATTERNFLOW_COLLECTION)return true;
 for(int i=0;i<CORE_PATTERN_COUNT;i++)if(CORE_PATTERN_IDS[i]==id)return true;
 return false;
}
bool isAvailableVariant(int variant){
 for(int i=0;i<COLLECTION_PATTERN_COUNT;i++)if(COLLECTION_PATTERN_IDS[i]==variant)return true;
 return false;
}
int galleryIndex(int engine,int variant){
 if(engine==PATTERNFLOW_COLLECTION){
  for(int i=0;i<COLLECTION_PATTERN_COUNT;i++)if(COLLECTION_PATTERN_IDS[i]==variant)return CORE_PATTERN_COUNT+i;
  return -1;
 }
 for(int i=0;i<CORE_PATTERN_COUNT;i++)if(CORE_PATTERN_IDS[i]==engine)return i;
 return -1;
}
