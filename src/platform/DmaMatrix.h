#pragma once
#if !defined(MATRIX_PROFILE_WS2812)
#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <driver/pcnt.h>
#include <hal/gpio_ll.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#include <soc/lcd_cam_struct.h>
#include "PanelOrientation.h"

// MatrixPortal S3 driving a 64x64 HUB75 panel through the I2S DMA driver.
// Patterns draw into an RGB565 canvas; show() rotates it into the panel's
// double-buffered four-bit DMA bitplanes.
class DmaMatrix : public GFXcanvas16 {
  class Panel : public MatrixPanel_I2S_DMA {
   public:
    using MatrixPanel_I2S_DMA::MatrixPanel_I2S_DMA;
    void restartOutput(){dma_bus.dma_transfer_start();}
  };
  // Output enable, and row address E, which also clocks the refresh counter.
  static constexpr int OE_PIN=14,ROW_E_PIN=21;
  Panel* panel_=nullptr;
  bool running_=false,counterReady_=false;
  static void blank(){
    pinMode(OE_PIN,OUTPUT);digitalWrite(OE_PIN,HIGH);
  }
 public:
  DmaMatrix():GFXcanvas16(64,64){}
  static uint16_t color565(uint8_t r,uint8_t g,uint8_t b){
    return ((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3);
  }
  bool begin(){
    static_assert(S3_LCD_DIV_NUM==8,"The panel is tuned for the 20 MHz divider");
    blank();if(!getBuffer())return false;
    pcnt_config_t counter={};
    counter.pulse_gpio_num=ROW_E_PIN;counter.ctrl_gpio_num=PCNT_PIN_NOT_USED;
    counter.channel=PCNT_CHANNEL_0;counter.unit=PCNT_UNIT_0;
    counter.pos_mode=PCNT_COUNT_INC;counter.neg_mode=PCNT_COUNT_DIS;
    counter.lctrl_mode=PCNT_MODE_KEEP;counter.hctrl_mode=PCNT_MODE_KEEP;
    counter.counter_h_lim=32767;counter.counter_l_lim=-1;
    counterReady_=pcnt_unit_config(&counter)==ESP_OK;
    if(counterReady_){pcnt_counter_pause(PCNT_UNIT_0);pcnt_counter_clear(PCNT_UNIT_0);}
    HUB75_I2S_CFG cfg(64,64,1);
    cfg.gpio={42,41,40,38,39,37,45,36,48,35,ROW_E_PIN,47,OE_PIN,2}; // R1 G1 B1 R2 G2 B2, A-E, latch, OE, clock
    cfg.double_buff=true;cfg.setPixelColorDepthBits(4);
    cfg.i2sspeed=HUB75_I2S_CFG::HZ_20M;cfg.min_refresh_rate=1;
    // The rotated panel's top edge corresponds to a physical shift column.
    // Sample on the opposite clock phase to avoid the one-column displacement.
    cfg.latch_blanking=2;cfg.clkphase=false;
    panel_=new Panel(cfg);
    if(!panel_ || !panel_->begin()){blank();return false;}
    // Fixed output-enable duty tuned on the reference panel; pattern
    // brightness is applied while rendering.
    panel_->setBrightness8(80);
    // Do not use gpio_set_direction here: IDF 4.4 disconnects LCD output.
    if(counterReady_){gpio_ll_input_enable(&GPIO,(gpio_num_t)ROW_E_PIN);gpio_pullup_dis((gpio_num_t)ROW_E_PIN);
      pcnt_counter_clear(PCNT_UNIT_0);pcnt_counter_resume(PCNT_UNIT_0);}
    running_=true;fillScreen(0);show();return true;
  }
  void show(){
    if(!running_)return;
    const uint16_t* pixels=getBuffer();
    for(int y=0;y<64;y++)for(int x=0;x<64;x++){
      uint16_t v=pixels[y*64+x];uint8_t r=(v>>11)&31,g=(v>>5)&63,b=v&31;
      const int dest=panelOutputIndex(x,y,64,MATRIX_OUTPUT_ROTATION);
      panel_->drawPixelRGB888(dest%64,dest/64,(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2));
    }
    panel_->flipDMABuffer();
  }
  void stop(){
    if(panel_ && running_)panel_->stopDMAoutput();
    running_=false;blank();if(counterReady_)pcnt_counter_pause(PCNT_UNIT_0);
  }
  void resume(){
    if(!panel_ || running_)return;
    esp_rom_gpio_connect_out_signal(OE_PIN,LCD_DATA_OUT0_IDX+7,false,false);
    panel_->restartOutput();running_=true;
    if(counterReady_){pcnt_counter_clear(PCNT_UNIT_0);pcnt_counter_resume(PCNT_UNIT_0);}
    show();
  }
  uint32_t getFrameCount(){
    if(!counterReady_ || !running_)return 0;
    int16_t count=0;pcnt_get_counter_value(PCNT_UNIT_0,&count);
    pcnt_counter_clear(PCNT_UNIT_0);return count>0?count:0;
  }
  float clockMHz() const{return panel_?160.f/LCD_CAM.lcd_clock.lcd_clkm_div_num:0;}
  bool counterReady() const{return counterReady_;}
};
#endif
