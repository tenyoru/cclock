#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace geom {

inline bool useDarkText(int r, int g, int b) {
  const auto linear = [](double channel) {
    channel /= 255.0;
    return channel <= 0.04045 ? channel / 12.92
                              : std::pow((channel + 0.055) / 1.055, 2.4);
  };
  const double luminance =
      0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b);
  return luminance > 0.179;
}

inline std::string pad2(int n) {
  std::string r = std::to_string(n);
  return r.size() < 2 ? "0" + r : r;
}

// Below the threshold, minutes run past 59, because a 90-minute session
// should read as minutes remaining, not 1:30:00.
inline constexpr int kFineSeconds = 90 * 60;

inline std::string formatTime(int remaining, char sep, bool padMinutes = true) {
  const int s = remaining < 0 ? -remaining : remaining;
  std::string body;
  if (s <= kFineSeconds) {
    body = (padMinutes ? pad2(s / 60) : std::to_string(s / 60)) + sep +
           pad2(s % 60);
  } else {
    // Rounded up, so a freshly started 2h timer is not instantly 1:59.
    const int mins = (s + 59) / 60;
    body = std::to_string(mins / 60) + sep + pad2(mins % 60);
  }
  return remaining < 0 ? "+" + body : body;
}

inline std::string nearestEdge(double px, double py, int sw, int sh,
                               std::string_view current) {
  const double d[] = {py, double(sh) - py, px, double(sw) - px};
  static constexpr const char *names[] = {"top", "bottom", "left", "right"};
  int cur = 0;
  for (int i = 0; i < 4; i++)
    if (current == names[i])
      cur = i;
  int near = cur;
  for (int i = 0; i < 4; i++)
    if (d[i] < d[near])
      near = i;
  if (near != cur && d[near] < d[cur] - 8)
    return names[near];
  return names[cur];
}

struct Rim {
  std::string edge;
  int x;
  int y;
};

inline Rim placeOnRim(double px, double py, int bw, int bh, int sw, int sh,
                      std::string_view current) {
  const std::string edge = nearestEdge(px, py, sw, sh, current);
  Rim r{edge, 0, 0};
  if (edge == "top" || edge == "bottom") {
    r.x = std::clamp(int(std::lround(px - bw / 2.0)), 0,
                     std::max(0, sw - bw));
    r.y = edge == "top" ? 0 : std::max(0, sh - bh);
  } else {
    r.x = edge == "left" ? 0 : std::max(0, sw - bw);
    r.y = std::clamp(int(std::lround(py - bh / 2.0)), 0,
                     std::max(0, sh - bh));
  }
  return r;
}

} // namespace geom
