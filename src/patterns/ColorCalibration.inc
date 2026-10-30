// White balance, applied to every pattern, and a test chart to judge it.
#include "../platform/ColorCalibration.h"
#if defined(MATRIX_PROFILE_WS2812)
// WS2812B green and blue outshine sRGB's at equal drive, so yellows read lime and
// whites cool; this correction makes the palettes look as designed.
constexpr uint8_t DEFAULT_BALANCE[3]={100,69,94};
#else
constexpr uint8_t DEFAULT_BALANCE[3]={100,100,100};
#endif
uint8_t panelBalance[3]={DEFAULT_BALANCE[0],DEFAULT_BALANCE[1],DEFAULT_BALANCE[2]};
bool testChartShown=false;
#if defined(MATRIX_PROFILE_WS2812)
// Brightness sets the WS2812 chart's level, as it does every pattern's.
constexpr uint8_t TEST_CHART_LEVEL=255;
#else
constexpr uint8_t TEST_CHART_LEVEL=96;
#endif
unsigned long testChartShownAt=0;
constexpr unsigned long TEST_CHART_TIMEOUT_MS=600000;
bool applyColorCalibration(uint8_t red,uint8_t green,uint8_t blue){
#if defined(MATRIX_PROFILE_WS2812)
  ledEncoder.setBalance(red,green,blue);
  return true;
#else
  return displayBackend.setChannelBalance(red,green,blue);
#endif
}
// Stored as version 1, then red, green and blue.
void loadColorCalibration(){
  Preferences store;
  if(store.begin("matrix-cfg",true)){
    uint8_t saved[8];  // Room for the 7-byte record of earlier firmware, which starts the same way.
    if(store.isKey("colorcal") && store.getBytes("colorcal",saved,sizeof(saved))>=4 && saved[0]==1)
      for(int i=0;i<3;i++)panelBalance[i]=min(uint8_t(100),saved[i+1]);
    store.end();
  }
  if(!applyColorCalibration(panelBalance[0],panelBalance[1],panelBalance[2])){
    memcpy(panelBalance,DEFAULT_BALANCE,sizeof(panelBalance));
    applyColorCalibration(panelBalance[0],panelBalance[1],panelBalance[2]);
  }
}
bool saveColorCalibration(){
  const uint8_t saved[4]={1,panelBalance[0],panelBalance[1],panelBalance[2]};
  Preferences store;if(!store.begin("matrix-cfg",false))return false;
  const bool ok=writeVerifiedBlob(store,"colorcal",saved,sizeof(saved));store.end();return ok;
}
// Top: neutral ramp. Middle: solid white, red, green and blue. Bottom: one ramp
// per channel. Bands keep their proportions of a 64-row panel at every height.
void renderTestChart(){
  for(int y=0;y<SCREEN_HEIGHT;y++){
    const int band=y*64/SCREEN_HEIGHT;
    for(int x=0;x<SCREEN_WIDTH;x++){
      const float v=x*TEST_CHART_LEVEL/max(1,SCREEN_WIDTH-1)/255.f,solid=TEST_CHART_LEVEL/255.f;
      const int c=x*4/SCREEN_WIDTH;
      uint16_t color=0;
      if(band<16)color=rgb565(v,v,v);
      else if(band>=18&&band<38)color=rgb565(c<2?solid:0,c==0||c==2?solid:0,c==0||c==3?solid:0);
      else if(band>=40&&band<47)color=rgb565(v,0,0);
      else if(band>=48&&band<55)color=rgb565(0,v,0);
      else if(band>=56)color=rgb565(0,0,v);
      setMatrixPointColor(y,x,color);
    }
  }
}
