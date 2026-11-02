#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>

namespace Pinetime::Applications::Screens {
  class SolarGlow {
  public:
    static constexpr uint16_t Diameter = 140;
    static constexpr uint16_t MaxScanlineBytes = Diameter * LV_IMG_PX_SIZE_ALPHA_BYTE;

    explicit SolarGlow(lv_obj_t* parent);
    ~SolarGlow();
    SolarGlow(const SolarGlow&) = delete;
    SolarGlow& operator=(const SolarGlow&) = delete;

    lv_obj_t* GetObject() const {
      return image;
    }

  private:
    static lv_res_t Info(lv_img_decoder_t* decoder, const void* src, lv_img_header_t* header);
    static lv_res_t Open(lv_img_decoder_t* decoder, lv_img_decoder_dsc_t* descriptor);
    static lv_res_t ReadLine(lv_img_decoder_t* decoder,
                             lv_img_decoder_dsc_t* descriptor,
                             lv_coord_t x,
                             lv_coord_t y,
                             lv_coord_t length,
                             uint8_t* buffer);
    static void Close(lv_img_decoder_t* decoder, lv_img_decoder_dsc_t* descriptor);
    static lv_color_t Pixel(uint16_t x, uint16_t y);

    lv_obj_t* image;
    lv_img_decoder_t* decoder;
    lv_img_dsc_t source {};
  };
}
