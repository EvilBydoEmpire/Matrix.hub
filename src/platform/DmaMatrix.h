#pragma once
#if defined(MATRIX_HUB75_DMA)
#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <driver/pcnt.h>
#include <hal/gpio_ll.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#include <soc/lcd_cam_struct.h>
#include "PanelOrientation.h"

// An RGB565 canvas preserves existing renderers, fades and preview capture.
// Only show() converts it to the independent double-buffered DMA bitplanes.
// Kept behind a separate build flag; the normal Protomatter build is unchanged.
class DmaMatrix : public GFXcanvas16 {
  class Panel : public MatrixPanel_I2S_DMA {
   public:
    using MatrixPanel_I2S_DMA::MatrixPanel_I2S_DMA;
    void restartOutput(){dma_bus.dma_transfer_start();}
  };
  Panel* panel_=nullptr;
  bool running_=false,counterReady_=false;
  static void blank(){
    pinMode(14,OUTPUT);digitalWrite(14,HIGH);
  }
 public:
  DmaMatrix():GFXcanvas16(64,64){}
  static uint16_t color565(uint8_t r,uint8_t g,uint8_t b){
    return ((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3);
  }
  bool begin(){
    static_assert(MATRIX_COLOR_DEPTH==4,"DMA candidate validated at four bits only");
    static_assert(S3_LCD_DIV_NUM==8,"Use the validated exact 20 MHz divider");
    blank();if(!getBuffer())return false;
    pcnt_config_t counter={};
    counter.pulse_gpio_num=21;counter.ctrl_gpio_num=PCNT_PIN_NOT_USED;
    counter.channel=PCNT_CHANNEL_0;counter.unit=PCNT_UNIT_0;
    counter.pos_mode=PCNT_COUNT_INC;counter.neg_mode=PCNT_COUNT_DIS;
    counter.lctrl_mode=PCNT_MODE_KEEP;counter.hctrl_mode=PCNT_MODE_KEEP;
    counter.counter_h_lim=32767;counter.counter_l_lim=-1;
    counterReady_=pcnt_unit_config(&counter)==ESP_OK;
    if(counterReady_){pcnt_counter_pause(PCNT_UNIT_0);pcnt_counter_clear(PCNT_UNIT_0);}
    HUB75_I2S_CFG cfg(64,64,1);
    cfg.gpio={42,41,40,38,39,37,45,36,48,35,21,47,14,2};
    cfg.double_buff=true;cfg.setPixelColorDepthBits(4);
    cfg.i2sspeed=HUB75_I2S_CFG::HZ_20M;cfg.min_refresh_rate=1;
    // The rotated panel's top edge corresponds to a physical shift column.
    // Sample on the opposite clock phase to avoid the one-column displacement.
    cfg.latch_blanking=2;cfg.clkphase=false;
    panel_=new Panel(cfg);
    if(!panel_ || !panel_->begin()){blank();return false;}
    // Deliberately dim experimental profile, same OE duty as the validated
    // probe. Independent of the saved per-pattern brightness values.
    panel_->setBrightness8(80);
    // Do not use gpio_set_direction here: IDF 4.4 disconnects LCD output.
    if(counterReady_){gpio_ll_input_enable(&GPIO,GPIO_NUM_21);gpio_pullup_dis(GPIO_NUM_21);
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
    esp_rom_gpio_connect_out_signal(14,LCD_DATA_OUT0_IDX+7,false,false);
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
