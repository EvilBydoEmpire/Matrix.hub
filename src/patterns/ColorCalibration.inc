// Device-level white balance is separate from per-pattern artistic palettes.
#include "../platform/ColorCalibration.h"
uint8_t calibrationMode=0; // 0 playback, 1 chart, 2 raw/fixed photo, 3 depth/photo
uint8_t calibrationPhoto=0;
uint8_t calibrationLevel=96;
uint8_t panelBalance[3]={100,100,100};
bool photoCorrectionEnabled=true;
uint16_t photoGamma100=180;
uint8_t photoTransfer[256];
unsigned long calibrationStarted=0;
void rebuildPhotoTransfer(){
  for(int i=0;i<256;i++)photoTransfer[i]=photoGammaValue(i,photoGamma100);
}
void loadColorCalibration(){
  Preferences store;
  if(store.begin("matrix-cfg",true)){
    uint8_t saved[7];
    if(store.getBytesLength("colorcal")==sizeof(saved) && store.getBytes("colorcal",saved,sizeof(saved))==sizeof(saved) && saved[0]==1){
      for(int i=0;i<3;i++)panelBalance[i]=min(uint8_t(100),saved[i+1]);
      photoGamma100=constrain(int(saved[4])|(int(saved[5])<<8),100,260);
      photoCorrectionEnabled=saved[6]!=0;
    }
    store.end();
  }
  rebuildPhotoTransfer();
#if !defined(MATRIX_PROFILE_WS2812)
  if(!displayBackend.setChannelBalance(panelBalance[0],panelBalance[1],panelBalance[2]))
    panelBalance[0]=panelBalance[1]=panelBalance[2]=100;
#endif
}
bool saveColorCalibration(){
  uint8_t saved[7]={1,panelBalance[0],panelBalance[1],panelBalance[2],uint8_t(photoGamma100),uint8_t(photoGamma100>>8),uint8_t(photoCorrectionEnabled)};
  Preferences store;if(!store.begin("matrix-cfg",false))return false;
  bool ok=store.putBytes("colorcal",saved,sizeof(saved))==sizeof(saved);store.end();return ok;
}
bool photoTransferActive(){
  return photoCorrectionEnabled && currentAnimation==PATTERNFLOW_COLLECTION &&
    (pfcParams.variant==56 || pfcParams.variant==57);
}
// Read the photo already included by PatternflowPack.cpp without a second
// flash copy. The accessor validates IDs and coordinates.
void renderColorCalibration(){
  matrix.fillScreen(0);matrix.setTextWrap(false);matrix.setTextSize(1);
  if(calibrationMode==1){
    // Top: neutral ramp. Middle: solid white/R/G/B. Bottom: channel ramps.
    // Peak is bounded to 32..128 (default 96), not a full-white power test.
    for(int x=0;x<SCREEN_WIDTH;x++){
      uint8_t v=x*calibrationLevel/max(1,SCREEN_WIDTH-1);
      matrix.drawFastVLine(x,0,16,matrix.color565(v,v,v));
      for(int c=0;c<4;c++){
        int start=c*SCREEN_WIDTH/4,end=(c+1)*SCREEN_WIDTH/4;
        if(x>=start&&x<end){
          uint8_t r=(c==0||c==1)?calibrationLevel:0,g=(c==0||c==2)?calibrationLevel:0,b=(c==0||c==3)?calibrationLevel:0;
          matrix.drawFastVLine(x,18,20,matrix.color565(r,g,b));
        }
      }
      matrix.drawFastVLine(x,40,7,matrix.color565(v,0,0));
      matrix.drawFastVLine(x,48,7,matrix.color565(0,v,0));
      matrix.drawFastVLine(x,56,8,matrix.color565(0,0,v));
    }
    return;
  }
  matrix.setTextColor(matrix.color565(64,64,64));
  matrix.setCursor(1,0);matrix.print(calibrationMode==3?"3BIT":"RAW");
  matrix.setCursor(33,0);matrix.print(calibrationMode==3?"4BIT":"FIX");
  for(int side=0;side<2;side++){
    for(int y=0;y<32;y++)for(int x=0;x<32;x++){
      unsigned sum[3]{};
      // Identical full-image framing on each side; area average 2x2 pixels.
      for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
        uint16_t v=personalPhotoTexel(calibrationPhoto,x*2+dx,y*2+dy);
        sum[0]+=(v>>8)&15;sum[1]+=(v>>4)&15;sum[2]+=v&15;
      }
      uint8_t channel[3];
      for(int c=0;c<3;c++){
        uint8_t v=(sum[c]*17+2)/4;
        if((calibrationMode==2 && side==1) || (calibrationMode==3 && photoCorrectionEnabled))v=photoTransfer[v];
        channel[c]=(v*calibrationLevel+127)/255;
      }
      uint16_t color=matrix.color565(channel[0],channel[1],channel[2]);
      if(calibrationMode==3 && side==0)color=simulateThreeBit565(color);
      matrix.drawPixel(x+side*32,y+10,color);
    }
    for(int x=0;x<32;x++){
      uint8_t v=x*255/31;
      if((calibrationMode==2&&side==1)||(calibrationMode==3&&photoCorrectionEnabled))v=photoTransfer[v];
      v=(v*calibrationLevel+127)/255;uint16_t color=matrix.color565(v,v,v);
      if(calibrationMode==3&&side==0)color=simulateThreeBit565(color);
      matrix.drawFastVLine(x+side*32,46,18,color);
    }
  }
}
