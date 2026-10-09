// Curated gallery/palette persistence. Keep legacy engine IDs stable; store only
// deviations from compiled gradient defaults, not dozens of redundant tables.
constexpr int PALETTE_COUNT=Soukan::Total;
const uint32_t LIBRARY_PALETTES[PALETTE_COUNT][4]={
 {0xFF3155,0xFFC247,0x22D3C5,0x765DFF},{0x3326A8,0x765DFF,0xBE5CFF,0xFF5DA2},
 {0xFF9F1C,0xFFE66D,0x2EC4B6,0x247BA0},{0x7A1631,0xFF3155,0xFFC247,0x6D28D9},
 {0x052E5E,0x0077B6,0x00B4D8,0x90E0EF},{0xFF00D4,0xFF3155,0x00F5D4,0x7C5CFF},
 {0xFF3155,0xFFC247,0x22D3C5,0x765DFF},{0xFF3155,0xFFC247,0x22D3C5,0x765DFF},
 {0x160A20,0xA92332,0xFF7926,0xFFD68A}, // Ember
 {0x031B3D,0x2252AB,0x5CDDF0,0xF1FCFF}, // Ice
 {0x063C49,0x0BBF9C,0xAEEA74,0xFF8062}, // Reef
 {Soukan::Colors[0][0],Soukan::Colors[0][1],Soukan::Colors[0][2],Soukan::Colors[0][3]},
 {Soukan::Colors[1][0],Soukan::Colors[1][1],Soukan::Colors[1][2],Soukan::Colors[1][3]},
 {Soukan::Colors[2][0],Soukan::Colors[2][1],Soukan::Colors[2][2],Soukan::Colors[2][3]},
 {Soukan::Colors[3][0],Soukan::Colors[3][1],Soukan::Colors[3][2],Soukan::Colors[3][3]},
 {Soukan::Colors[4][0],Soukan::Colors[4][1],Soukan::Colors[4][2],Soukan::Colors[4][3]},
 {Soukan::Colors[5][0],Soukan::Colors[5][1],Soukan::Colors[5][2],Soukan::Colors[5][3]},
 {Soukan::Colors[6][0],Soukan::Colors[6][1],Soukan::Colors[6][2],Soukan::Colors[6][3]},
 {Soukan::Colors[7][0],Soukan::Colors[7][1],Soukan::Colors[7][2],Soukan::Colors[7][3]},
 {Soukan::Colors[8][0],Soukan::Colors[8][1],Soukan::Colors[8][2],Soukan::Colors[8][3]},
 {Soukan::Colors[9][0],Soukan::Colors[9][1],Soukan::Colors[9][2],Soukan::Colors[9][3]}
};
// A compiled color-only default overlay avoids rewriting all 59 preset blobs.
// Old user colors remain recoverable in their untouched stored patches. Once
// the user saves/selects a default after this upgrade, their choice wins forever.
uint8_t soukanUserDefaults[12]{};
extern uint8_t variantDefaults[PATTERNFLOW_PATTERN_COUNT];
void applySoukanDefault(int engine,int variant,int slot){
 int id=engine==16?16+variant:engine;
 int defaultSlot=engine==16?variantDefaults[variant]:defaultPatches[engine];
 if(slot!=defaultSlot||(soukanUserDefaults[id/8]&(1U<<(id%8))))return;
 colorParams[engine]={0,360,uint8_t(Soukan::paletteFor(engine,variant))};
 colorTuning[engine]={0,100};
}
bool saveSoukanOverride(int engine,int variant){
 int id=engine==16?16+variant:engine;uint8_t next[sizeof(soukanUserDefaults)];memcpy(next,soukanUserDefaults,sizeof(next));
 next[id/8]|=1U<<(id%8);if(!memcmp(next,soukanUserDefaults,sizeof(next)))return true;
 Preferences store;bool ok=store.begin("matrix-cfg",false);uint8_t check[sizeof(next)]{};
 if(ok){ok=store.putBytes("soukan_ud",next,sizeof(next))==sizeof(next)&&store.getBytes("soukan_ud",check,sizeof(check))==sizeof(check)&&!memcmp(next,check,sizeof(next));store.end();}
 if(ok)memcpy(soukanUserDefaults,next,sizeof(next));return ok;
}
struct VariantPatch {
 bool valid=false;
 PatternflowCollectionParams params{};
 ColorParams color{};
 ColorTuningParams tuning{};
 PatternStyleParams style{};
 uint32_t stops[PALETTE_COUNT][4]{};
};
// Empty patch slots cost one pointer, not a full palette/parameter bank. This
// leaves sufficient internal DRAM on ordinary WROOM modules as the library grows.
VariantPatch* variantPatches[PATTERNFLOW_PATTERN_COUNT][NUM_PATCH_SLOTS]{};
uint8_t variantDefaults[PATTERNFLOW_PATTERN_COUNT]{};
uint16_t importedRgbPalette[4096];
uint32_t importedRgbSignature=0;
void ensureImportedRgbPalette(int brightness){
 const ColorParams& control=colorParams[16];const ColorTuningParams& tuning=colorTuning[16];
 uint32_t signature=2166136261U;
 auto hash=[&](uint32_t value){signature=(signature^value)*16777619U;};
 hash(brightness);hash(control.hue);hash(control.spread);hash(control.palette);hash(tuning.hueShift);hash(tuning.saturation);hash(pfcParams.variant);
 bool original=control.palette==7;
 for(int c=0;c<4;c++){hash(paletteStops[16][control.palette][c]);if(paletteStops[16][7][c]!=PATTERNFLOW_METADATA[pfcParams.variant].originalColors[c])original=false;}
 if(signature==importedRgbSignature)return;
 uint32_t toneStops[4];Soukan::orderedByLight(paletteStops[16][control.palette],toneStops);
 for(int i=0;i<4096;i++){
   float r=((i>>8)&15)/15.f,g=((i>>4)&15)/15.f,b=(i&15)/15.f;
   float maximum=max(r,max(g,b)),minimum=min(r,min(g,b)),delta=maximum-minimum,hue=0;
   if(delta>.0001f){if(maximum==r)hue=(g-b)/delta;else if(maximum==g)hue=2+(b-r)/delta;else hue=4+(r-g)/delta;hue/=6;if(hue<0)hue+=1;}
   if(Soukan::active(control.palette)&&(pfcParams.variant==56||pfcParams.variant==57||delta<.001f)){
     // Tonal subjects use luminance, not undefined grey hue; do not multiply
     // their brightness twice. Sort only this transfer ramp, not palette data.
     float light=constrain((r*.2126f+g*.7152f+b*.0722f-.5f)*(control.spread/360.f)+.5f+control.hue/360.f,0.f,1.f);
     if(maximum<.001f){importedRgbPalette[i]=0;continue;}
     uint32_t c=Soukan::photoTone(toneStops,light);
     float cr=((c>>16)&255)/255.f,cg=((c>>8)&255)/255.f,cb=(c&255)/255.f;
     float hi=max(cr,max(cg,cb)),lo=min(cr,min(cg,cb)),d=hi-lo,h=0;
     if(d>.0001f){if(hi==cr)h=(cg-cb)/d;else if(hi==cg)h=2+(cb-cr)/d;else h=4+(cr-cg)/d;h/=6.f;}
     // Non-photo greys are often a dark canvas, not a tone-mapped subject.
     // Never turn a 6% grey backdrop into a full-strength palette pigment.
     float value=(pfcParams.variant==56||pfcParams.variant==57)?hi:min(hi,maximum);
     importedRgbPalette[i]=makeColor(h+tuning.hueShift/360.f,hi>.001f?constrain(d/hi*tuning.saturation/100.f,0.f,1.f):0.f,value,brightness);
   }else if(original){
     float shifted=hue*(control.spread/360.f)+(control.hue+tuning.hueShift)/360.f;
     float saturation=maximum>.001f?delta/maximum:0;
     importedRgbPalette[i]=makeColor(shifted,constrain(saturation*tuning.saturation/100.f,0.f,1.f),maximum,brightness);
   }else importedRgbPalette[i]=maximum<.001f?0:makePaletteColor(delta<.001f?maximum*.749f:hue,255,(uint8_t)(maximum*255),brightness,control);
 }
 importedRgbSignature=signature;
}
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
uint32_t compiledStop(int engine,int variant,int palette,int stop){
 if(palette==7){
   if(engine==PATTERNFLOW_COLLECTION)return PATTERNFLOW_METADATA[variant].originalColors[stop];
   if(engine==PIXEL_AVALANCHE)return 0xFFFFFF;
   if(engine==SNAKE)return 0x32E34E;
   if(engine==BAYER_FLOW||engine==FLOW_SPHERE)return LIBRARY_PALETTES[5][stop];
   if(engine==FLOYD_FLOW)return LIBRARY_PALETTES[3][stop];
 }
 return LIBRARY_PALETTES[palette][stop];
}
// Legacy 7-bank blobs are expanded without changing existing user colors.
size_t readPalettePatches(Preferences& prefs){
 size_t length=prefs.getBytesLength("palettes_p");
 const size_t legacySize=NUM_ANIMATIONS*NUM_PATCH_SLOTS*7*4*sizeof(uint32_t);
 if(length==legacySize){
   uint32_t* old=(uint32_t*)malloc(length);if(!old)return 0;
   size_t got=prefs.getBytes("palettes_p",old,length);
   if(got==length)for(int a=0;a<NUM_ANIMATIONS;a++)for(int s=0;s<NUM_PATCH_SLOTS;s++)
     memcpy(paletteStopPatches[a][s],old+(a*NUM_PATCH_SLOTS+s)*28,28*sizeof(uint32_t));
   free(old);return got==length?sizeof(paletteStopPatches):0;
 }
 if(length<4||length>12000)return 0;
 uint8_t* bytes=(uint8_t*)malloc(length);if(!bytes)return 0;
 size_t got=prefs.getBytes("palettes_p",bytes,length);bool ok=got==length&&bytes[0]=='P'&&bytes[1]=='L'&&bytes[2]==1;
 if(ok)for(size_t o=4;o+7<=length;o+=7){int a=bytes[o],s=bytes[o+1],p=bytes[o+2]>>2,c=bytes[o+2]&3;uint32_t v;memcpy(&v,bytes+o+3,4);
   if(a>=NUM_ANIMATIONS||s>=NUM_PATCH_SLOTS||p>=PALETTE_COUNT){ok=false;break;}paletteStopPatches[a][s][p][c]=v;}
 free(bytes);return ok?sizeof(paletteStopPatches):0;
}
bool writePalettePatches(Preferences& prefs){
 // A palette stop record is seven bytes, and default colors cost no storage.
 size_t n=4;for(int a=0;a<NUM_ANIMATIONS;a++)for(int s=0;s<NUM_PATCH_SLOTS;s++)for(int p=0;p<PALETTE_COUNT;p++)for(int c=0;c<4;c++)
  if(paletteStopPatches[a][s][p][c]!=compiledStop(a,0,p,c))n+=7;
 uint8_t* bytes=(uint8_t*)malloc(n);if(!bytes)return false;
 bytes[0]='P';bytes[1]='L';bytes[2]=1;bytes[3]=0;size_t o=4;
 for(int a=0;a<NUM_ANIMATIONS;a++)for(int s=0;s<NUM_PATCH_SLOTS;s++)for(int p=0;p<PALETTE_COUNT;p++)for(int c=0;c<4;c++){
  uint32_t v=paletteStopPatches[a][s][p][c];if(v==compiledStop(a,0,p,c))continue;
  bytes[o++]=a;bytes[o++]=s;bytes[o++]=(p<<2)|c;memcpy(bytes+o,&v,4);o+=4;
 }
 bool ok=prefs.putBytes("palettes_p",bytes,n)==n;free(bytes);return ok;
}
struct VariantStored {
 uint8_t variant,slot,defaultSlot,stopCount;
 PatternflowCollectionParams params;
 ColorParams color;
 ColorTuningParams tuning;
 PatternStyleParams style;
};
bool storeVariantPatches(){
 // Sparse bank records keep all per-pattern palette edits without 24 KB blobs.
 size_t n=4+PATTERNFLOW_PATTERN_COUNT;
 for(int v=0;v<PATTERNFLOW_PATTERN_COUNT;v++)for(int s=0;s<NUM_PATCH_SLOTS;s++)if(variantPatches[v][s]){
   n+=sizeof(VariantStored);
   for(int p=0;p<PALETTE_COUNT;p++)for(int c=0;c<4;c++)if(variantPatches[v][s]->stops[p][c]!=compiledStop(16,v,p,c))n+=5;
 }
 uint8_t* bytes=(uint8_t*)calloc(1,n);if(!bytes)return false;bytes[0]='V';bytes[1]='P';bytes[2]=3;bytes[3]=PATTERNFLOW_PATTERN_COUNT;size_t o=4;
 memcpy(bytes+o,variantDefaults,PATTERNFLOW_PATTERN_COUNT);o+=PATTERNFLOW_PATTERN_COUNT;
 for(int v=0;v<PATTERNFLOW_PATTERN_COUNT;v++)for(int s=0;s<NUM_PATCH_SLOTS;s++){
   if(!variantPatches[v][s])continue;const VariantPatch& patch=*variantPatches[v][s];
   VariantStored stored{};stored.variant=v;stored.slot=s;stored.defaultSlot=variantDefaults[v];stored.params=patch.params;stored.color=patch.color;stored.tuning=patch.tuning;stored.style=patch.style;
   for(int p=0;p<PALETTE_COUNT;p++)for(int c=0;c<4;c++)if(patch.stops[p][c]!=compiledStop(16,v,p,c))stored.stopCount++;
   memcpy(bytes+o,&stored,sizeof(stored));o+=sizeof(stored);
   for(int p=0;p<PALETTE_COUNT;p++)for(int c=0;c<4;c++){uint32_t col=patch.stops[p][c];if(col==compiledStop(16,v,p,c))continue;bytes[o++]=(p<<2)|c;memcpy(bytes+o,&col,4);o+=4;}
 }
 Preferences prefs;bool ok=prefs.begin("matrix-cfg",false)&&prefs.putBytes("pf_defs",bytes,n)==n;prefs.end();free(bytes);return ok;
}
void readVariantPatches(){
 Preferences prefs;if(!prefs.begin("matrix-cfg",true))return;size_t n=prefs.getBytesLength("pf_defs");if(n<4||n>16000){prefs.end();return;}
 uint8_t* bytes=(uint8_t*)malloc(n);if(!bytes){prefs.end();return;}size_t got=prefs.getBytes("pf_defs",bytes,n);prefs.end();
 if(got==n&&bytes[0]=='V'&&bytes[1]=='P'&&(bytes[2]>=1&&bytes[2]<=3)){size_t o=4;
   // VP2 has exactly 35 defaults, regardless of the new catalog size. VP3
   // records its slot count explicitly so future additions cannot shift records.
   if(bytes[2]>=2){const int count=bytes[2]==2?35:bytes[3];if(n<o+count){free(bytes);return;}
     for(int v=0;v<count;v++){uint8_t slot=bytes[o++];if(v<PATTERNFLOW_PATTERN_COUNT)variantDefaults[v]=min(slot,uint8_t(NUM_PATCH_SLOTS-1));}}
   while(o+sizeof(VariantStored)<=n){VariantStored stored;memcpy(&stored,bytes+o,sizeof(stored));o+=sizeof(stored);
     int v=stored.variant,s=stored.slot;if(v>=PATTERNFLOW_PATTERN_COUNT||s>=NUM_PATCH_SLOTS||stored.defaultSlot>=NUM_PATCH_SLOTS||o+stored.stopCount*5>n)break;
     if(!isAvailableVariant(v)){o+=stored.stopCount*5;continue;}
     if(!variantPatches[v][s])variantPatches[v][s]=new(std::nothrow) VariantPatch;
     if(!variantPatches[v][s]){o+=stored.stopCount*5;continue;}
     VariantPatch& patch=*variantPatches[v][s];patch.params=stored.params;patch.params.variant=v;patch.color=stored.color;patch.tuning=stored.tuning;patch.style=stored.style;
     for(int p=0;p<PALETTE_COUNT;p++)for(int c=0;c<4;c++)patch.stops[p][c]=compiledStop(16,v,p,c);
     bool valid=stored.color.palette<PALETTE_COUNT;
     for(int j=0;j<stored.stopCount;j++){int p=bytes[o]>>2,c=bytes[o++]&3;uint32_t col;memcpy(&col,bytes+o,4);o+=4;if(p>=PALETTE_COUNT){valid=false;continue;}patch.stops[p][c]=col;}
     patch.valid=valid;variantDefaults[v]=stored.defaultSlot;
   }
 }free(bytes);
}
void loadVariantPatch(int v,int slot){
 if(!isAvailableVariant(v)||slot<0||slot>=NUM_PATCH_SLOTS)return;
 const VariantPatch* patch=variantPatches[v][slot];
 if(patch&&patch->valid){pfcParams=patch->params;colorParams[16]=patch->color;colorTuning[16]=patch->tuning;styleParams[16]=patch->style;memcpy(paletteStops[16],patch->stops,sizeof(patch->stops));}
 else{
  pfcParams={v==47?6:7,v,{0,0,0,0},0,.5f,1};memcpy(pfcParams.control,PATTERNFLOW_METADATA[v].defaults,4);
  colorParams[16]={0,360,7};colorTuning[16]={0,100};styleParams[16]={1,0,0,0,0};
  for(int p=0;p<PALETTE_COUNT;p++)for(int c=0;c<4;c++)paletteStops[16][p][c]=compiledStop(16,v,p,c);
 }
 applySoukanDefault(16,v,slot);
 pfcParams.variant=v;defaultPatches[16]=variantDefaults[v];patternflowPack.reset(v);huePaletteAnimation=-1;importedRgbSignature=0;
}
bool saveVariantPatch(int slot){
 int v=pfcParams.variant;const bool existed=variantPatches[v][slot]!=nullptr;VariantPatch previous;
 if(existed)previous=*variantPatches[v][slot];else variantPatches[v][slot]=new(std::nothrow) VariantPatch;
 if(!variantPatches[v][slot])return false;auto& patch=*variantPatches[v][slot];
 patch.valid=true;patch.params=pfcParams;patch.color=colorParams[16];patch.tuning=colorTuning[16];patch.style=styleParams[16];memcpy(patch.stops,paletteStops[16],sizeof(patch.stops));
 bool ok=storeVariantPatches();if(!ok){if(existed)patch=previous;else{delete variantPatches[v][slot];variantPatches[v][slot]=nullptr;}}return ok;
}
void configureCuratedLibrary(){
 Preferences prefs;if(!prefs.begin("matrix-cfg",false))return;
 const size_t maskSize=prefs.getBytesLength("soukan_ud");
 if(maskSize==10||maskSize==sizeof(soukanUserDefaults))prefs.getBytes("soukan_ud",soukanUserDefaults,maskSize);
 bool first=prefs.getUInt("librev",0)<1;
 if(first){
  const uint64_t oldQuality=prefs.getULong64("qbits",0);uint64_t bits=0;
  for(int i=0;i<17;i++){patternflowQuality[i]=(oldQuality>>LEGACY_VARIANT_IDS[i])&1;bits|=uint64_t(patternflowQuality[i]!=0)<<i;}
  memset(patternBookmarks,0,sizeof(patternBookmarks));
  // Legacy engine slots remain as inert tombstones for patch migration; the
  // archived source preserves discarded algorithms without exposing them.
  for(int i=0;i<CORE_PATTERN_COUNT;i++){int a=CORE_PATTERN_IDS[i];for(int s=0;s<NUM_PATCH_SLOTS;s++){
    colorPatches[a][s].palette=7;colorPatches[a][s].hue=0;colorPatches[a][s].spread=360;
    for(int c=0;c<4;c++)paletteStopPatches[a][s][7][c]=compiledStop(a,0,7,c);
  }}
  // Compact the legacy gradients before allocating any new persistent data.
  bool ok=writePalettePatches(prefs);
  ok=ok&&prefs.putBytes("color_p",colorPatches,sizeof(colorPatches))==sizeof(colorPatches);
  ok=ok&&prefs.putULong64("qbits",bits)==sizeof(bits);
  ok=ok&&prefs.putULong64("bmarks",0)==sizeof(uint64_t);
  if(ok)prefs.putUInt("librev",1);
  else Serial.println("Library migration not fully saved; it will retry on restart.");
 }
 if(prefs.getUInt("librev",0)==1){
  // All eight starred entries in the revision-1 snapshot were removed. Do
  // this once only: future user bookmarks must survive every subsequent boot.
  memset(patternBookmarks,0,sizeof(patternBookmarks));
  if(prefs.putULong64("bmarks",0)==sizeof(uint64_t))prefs.putUInt("librev",2);
 }
 if(prefs.getUInt("librev",0)==2){
  // Revision 3 removes every bookmark in the archived device snapshot. Only
  // clear on this upgrade; stars created in the new catalog survive reboot.
  memset(patternBookmarks,0,sizeof(patternBookmarks));
  if(prefs.putULong64("bmarks",0)==sizeof(uint64_t))prefs.putUInt("librev",3);
 }
 prefs.end();readVariantPatches();loadPatch(STARFIELD,defaultPatches[STARFIELD]);
}
