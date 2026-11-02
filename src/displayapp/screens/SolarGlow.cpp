#include "displayapp/screens/SolarGlow.h"
#include <cstring>

using namespace Pinetime::Applications::Screens;

SolarGlow::SolarGlow(lv_obj_t* parent) {
  static_assert(MaxScanlineBytes <= 420);
  source.header.cf = LV_IMG_CF_USER_ENCODED_1;
  source.header.w = Diameter;
  source.header.h = Diameter;
  decoder = lv_img_decoder_create();
  decoder->user_data = this;
  lv_img_decoder_set_info_cb(decoder, Info);
  lv_img_decoder_set_open_cb(decoder, Open);
  lv_img_decoder_set_read_line_cb(decoder, ReadLine);
  lv_img_decoder_set_close_cb(decoder, Close);
  image = lv_img_create(parent, nullptr);
  lv_obj_set_click(image, false);
  lv_img_set_src(image, &source);
}

SolarGlow::~SolarGlow() {
  // Cached scanline sessions retain this instance's source and decoder context.
  lv_img_cache_invalidate_src(&source);
  lv_obj_del(image);
  lv_img_decoder_delete(decoder);
}

lv_res_t SolarGlow::Info(lv_img_decoder_t* decoder, const void* src, lv_img_header_t* header) {
  auto* glow = static_cast<SolarGlow*>(decoder->user_data);
  if (src != &glow->source) {
    return LV_RES_INV;
  }
  *header = glow->source.header;
  header->cf = LV_IMG_CF_TRUE_COLOR;
  return LV_RES_OK;
}

lv_res_t SolarGlow::Open(lv_img_decoder_t* decoder, lv_img_decoder_dsc_t* descriptor) {
  auto* glow = static_cast<SolarGlow*>(decoder->user_data);
  if (descriptor->src != &glow->source) {
    return LV_RES_INV;
  }
  descriptor->img_data = nullptr;
  descriptor->user_data = glow;
  return LV_RES_OK;
}

lv_res_t SolarGlow::ReadLine(lv_img_decoder_t* /*decoder*/,
                            lv_img_decoder_dsc_t* descriptor,
                            lv_coord_t x,
                            lv_coord_t y,
                            lv_coord_t length,
                            uint8_t* buffer) {
  if (descriptor->user_data == nullptr || x < 0 || y < 0 || length < 0 || y >= Diameter || x + length > Diameter ||
      (length > 0 && buffer == nullptr)) {
    return LV_RES_INV;
  }
  for (lv_coord_t offset = 0; offset < length; ++offset) {
    const auto color = Pixel(x + offset, y);
    std::memcpy(buffer + offset * sizeof(lv_color_t), &color, sizeof(color));
  }
  return LV_RES_OK;
}

void SolarGlow::Close(lv_img_decoder_t* /*decoder*/, lv_img_decoder_dsc_t* descriptor) {
  descriptor->user_data = nullptr;
}

lv_color_t SolarGlow::Pixel(uint16_t x, uint16_t y) {
  // Doubled coordinates center the glow between its four middle pixels without
  // floating point, stored pixels, or a full decoded image buffer.
  const int32_t dx = static_cast<int32_t>(x) * 2 + 1 - Diameter;
  const int32_t dy = static_cast<int32_t>(y) * 2 + 1 - Diameter;
  const uint32_t distanceSquared = dx * dx + dy * dy;
  constexpr uint32_t radiusSquared = Diameter * Diameter;
  if (distanceSquared >= radiusSquared) {
    return LV_COLOR_BLACK;
  }
  const uint32_t radial = ((radiusSquared - distanceSquared) * 255 + radiusSquared / 2) / radiusSquared;
  const uint32_t broad = (radial * radial + 127) / 255;
  // A tighter pale-cyan core gives way to muted steel blue, then black space.
  const uint32_t inner = radial > 224 ? ((radial - 224) * 255 + 15) / 31 : 0;
  const uint32_t core = (inner * inner * inner + 32512) / 65025;
  const uint8_t red = (broad * 75 + core * 150 + 127) / 255;
  const uint8_t green = (broad * 140 + core * 105 + 127) / 255;
  const uint8_t blue = (broad * 200 + core * 55 + 127) / 255;
  return lv_color_make(red, green, blue);
}
