#include <cassert>
#include "../src/platform/PanelOrientation.h"
int main(){
  // Clockwise: top-left -> top-right, top-right -> bottom-right.
  static_assert(panelOutputIndex(0,0,64,1)==63,"top-left");
  static_assert(panelOutputIndex(63,0,64,1)==4095,"top-right");
  static_assert(panelOutputIndex(0,63,64,1)==0,"bottom-left");
  for(int rot=0;rot<4;rot++){
    bool seen[4096]={};
    for(int y=0;y<64;y++)for(int x=0;x<64;x++){
      int p=panelOutputIndex(x,y,64,rot);assert(p>=0&&p<4096&&!seen[p]);seen[p]=true;
      assert(panelOutputIndex(p%64,p/64,64,(4-rot)%4)==y*64+x);
    }
    for(bool value:seen)assert(value);
  }
}
