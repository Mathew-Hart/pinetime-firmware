#pragma once

#include <cstddef>
#include <optional>

namespace Pinetime::Applications {
  template <typename Faces, typename Id, typename Available>
  std::optional<Id> NextAvailableWatchFace(const Faces& faces, Id current, Available available) {
    if (faces.empty()) {
      return std::nullopt;
    }

    // An unknown saved identifier starts at the first available face.
    size_t start = faces.size() - 1;
    for (size_t index = 0; index < faces.size(); ++index) {
      if (faces[index].watchFace == current) {
        start = index;
        break;
      }
    }

    for (size_t offset = 1; offset <= faces.size(); ++offset) {
      const auto& face = faces[(start + offset) % faces.size()];
      if (face.watchFace != current && available(face)) {
        return face.watchFace;
      }
    }
    return std::nullopt;
  }
}
