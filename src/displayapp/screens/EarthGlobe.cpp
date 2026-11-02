#include "displayapp/screens/EarthGlobe.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "displayapp/screens/EarthTexture.h"

using namespace Pinetime::Applications::Screens;

EarthGlobe::EarthGlobe(lv_obj_t* parent) {
  static_assert(EarthTexture::Diameter == Diameter);
  static_assert(TextureBytes <= 44000);
  static_assert(MaxScanlineBytes <= 618);
  source.header.cf = LV_IMG_CF_USER_ENCODED_0;
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

EarthGlobe::~EarthGlobe() {
  // Cached decoding sessions retain our context, even after an image is drawn.
  lv_img_cache_invalidate_src(&source);
  lv_obj_del(image);
  lv_img_decoder_delete(decoder);
}

void EarthGlobe::SetSun(float x, float y, float z, bool known) {
  const float magnitudeSquared = x * x + y * y + z * z;
  known = known && std::isfinite(magnitudeSquared) && magnitudeSquared > 0.0001f;
  if (known) {
    const float magnitude = std::sqrt(magnitudeSquared);
    x /= magnitude;
    y /= magnitude;
    z /= magnitude;
  } else {
    x = 0;
    y = 0;
    z = 1;
  }
  if (sunKnown == known && sunX == x && sunY == y && sunZ == z) {
    return;
  }
  sunKnown = known;
  sunX = x;
  sunY = y;
  sunZ = z;
  // The decoder also reads current state per scanline; invalidation prevents
  // an old cached session from surviving a future decoder implementation.
  lv_img_cache_invalidate_src(&source);
  lv_obj_invalidate(image);
}

lv_res_t EarthGlobe::Info(lv_img_decoder_t* decoder, const void* src, lv_img_header_t* header) {
  auto* globe = static_cast<EarthGlobe*>(decoder->user_data);
  if (src != &globe->source) {
    return LV_RES_INV;
  }
  *header = globe->source.header;
  header->cf = LV_IMG_CF_TRUE_COLOR;
  return LV_RES_OK;
}

lv_res_t EarthGlobe::Open(lv_img_decoder_t* decoder, lv_img_decoder_dsc_t* descriptor) {
  auto* globe = static_cast<EarthGlobe*>(decoder->user_data);
  if (descriptor->src != &globe->source) {
    return LV_RES_INV;
  }
  descriptor->img_data = nullptr;
  descriptor->user_data = globe;
  return LV_RES_OK;
}

lv_res_t EarthGlobe::ReadLine(lv_img_decoder_t* /*decoder*/,
                             lv_img_decoder_dsc_t* descriptor,
                             lv_coord_t x,
                             lv_coord_t y,
                             lv_coord_t length,
                             uint8_t* buffer) {
  auto* globe = static_cast<EarthGlobe*>(descriptor->user_data);
  if (globe == nullptr || x < 0 || y < 0 || length < 0 || y >= Diameter || x + length > Diameter || (length > 0 && buffer == nullptr)) {
    return LV_RES_INV;
  }
  for (lv_coord_t offset = 0; offset < length; ++offset) {
    const auto color = globe->Pixel(x + offset, y);
    std::memcpy(buffer + offset * sizeof(lv_color_t), &color, sizeof(color));
  }
  return LV_RES_OK;
}

void EarthGlobe::Close(lv_img_decoder_t* /*decoder*/, lv_img_decoder_dsc_t* descriptor) {
  descriptor->user_data = nullptr;
}

lv_color_t EarthGlobe::Pixel(uint16_t x, uint16_t y) const {
  constexpr float radius = Diameter / 2.0f;
  const float nx = (x + 0.5f - radius) / radius;
  const float ny = (radius - y - 0.5f) / radius;
  const float radiusSquared = nx * nx + ny * ny;
  if (radiusSquared > 1.0f) {
    return LV_COLOR_BLACK;
  }
  uint16_t brightness = 255;
  if (sunKnown) {
    const float nz = std::sqrt(1.0f - radiusSquared);
    const float dot = nx * sunX + ny * sunY + nz * sunZ;
    // A soft transition around the horizon approximates civil twilight. The
    // night side keeps18% of the historical terrain/cloud composite visible.
    const float twilight = std::clamp((dot + 0.1f) * 5.0f, 0.0f, 1.0f);
    const float blend = twilight * twilight * (3.0f - 2.0f * twilight);
    const float daylight = 0.6f + 0.4f * std::max(0.0f, dot);
    brightness = static_cast<uint16_t>((0.18f + blend * (daylight - 0.18f)) * 255.0f + 0.5f);
  }
  const auto palette = EarthTexture::Palette[EarthTexture::Pixels[y * Diameter + x]];
  const uint16_t red = (palette >> 11) & 31;
  const uint16_t green = (palette >> 5) & 63;
  const uint16_t blue = palette & 31;
  return lv_color_make((red * brightness + 15) / 31, (green * brightness + 31) / 63, (blue * brightness + 15) / 31);
}
