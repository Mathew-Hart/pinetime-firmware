#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>

namespace Pinetime::Applications::Screens {
  class EarthGlobe {
  public:
    static constexpr uint16_t Diameter = 206;
    static constexpr uint32_t TextureBytes = 42948;
    static constexpr uint16_t MaxScanlineBytes = Diameter * LV_IMG_PX_SIZE_ALPHA_BYTE;

    explicit EarthGlobe(lv_obj_t* parent);
    ~EarthGlobe();
    EarthGlobe(const EarthGlobe&) = delete;
    EarthGlobe& operator=(const EarthGlobe&) = delete;

    lv_obj_t* GetObject() const {
      return image;
    }

    // Unit view-frame direction: +X screen-right/east, +Y screen-up/north,
    // +Z toward the viewer. Texture center is longitude-100°, latitude20°.
    // Unknown (or invalid) directions show the neutral, unshaded texture.
    void SetSun(float sunX, float sunY, float sunZ, bool known);

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
    lv_color_t Pixel(uint16_t x, uint16_t y) const;

    lv_obj_t* image;
    lv_img_decoder_t* decoder;
    lv_img_dsc_t source {};
    float sunX = 0;
    float sunY = 0;
    float sunZ = 1;
    bool sunKnown = false;
  };
}
