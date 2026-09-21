#include "geom.hpp"

#include <iostream>

#define CHECK(expression)                                                      \
  if (!(expression)) {                                                        \
    std::cerr << "check failed: " #expression << '\n';                       \
    return 1;                                                                 \
  }

int main() {
  using namespace geom;

  CHECK(useDarkText(255, 255, 255));
  CHECK(useDarkText(255, 214, 10));
  CHECK(!useDarkText(0, 0, 0));
  CHECK(!useDarkText(30, 30, 80));

  CHECK(formatTime(0, ':') == "00:00");
  CHECK(formatTime(65, ':') == "01:05");
  CHECK(formatTime(300, ':', false) == "5:00");
  CHECK(formatTime(3599, ':') == "59:59");
  CHECK(formatTime(3600, ':') == "60:00");
  CHECK(formatTime(5400, ':') == "90:00");
  CHECK(formatTime(5401, ':') == "1:31");
  CHECK(formatTime(7199, ':') == "2:00");
  CHECK(formatTime(9000, ':') == "2:30");
  CHECK(formatTime(5400, '\n') == "90\n00");
  CHECK(formatTime(-5, ':') == "+00:05");
  CHECK(formatTime(-300, ':', false) == "+5:00");
  CHECK(formatTime(-5, '\n') == "+00\n05");
  CHECK(formatFullscreenTime(0) == "00:00");
  CHECK(formatFullscreenTime(109) == "01:49");
  CHECK(formatFullscreenTime(5245) == "01:27:25");
  CHECK(formatFullscreenTime(360000) == "100:00:00");

  const int sw = 1920, sh = 1080, bw = 140, bh = 62;
  {
    auto r = placeOnRim(960, 10, bw, bh, sw, sh, "top");
    CHECK(r.edge == "top" && r.y == 0 && r.x == 890);
  }
  {
    auto r = placeOnRim(960, 1070, bw, bh, sw, sh, "bottom");
    CHECK(r.edge == "bottom" && r.y == 1018 && r.x == 890);
  }
  {
    auto r = placeOnRim(10, 540, bw, bh, sw, sh, "left");
    CHECK(r.edge == "left" && r.x == 0 && r.y == 509);
  }
  {
    auto r = placeOnRim(1910, 540, bw, bh, sw, sh, "right");
    CHECK(r.edge == "right" && r.x == 1780 && r.y == 509);
  }
  {
    auto r = placeOnRim(960, 540, bw, bh, sw, sh, "top");
    CHECK(r.edge == "top" && r.y == 0 && r.x == 890);
  }
  CHECK(placeOnRim(0, 10, bw, bh, sw, sh, "top").x == 0);
  CHECK(placeOnRim(sw, 10, bw, bh, sw, sh, "top").x == sw - bw);
  CHECK(placeOnRim(0, sh, sw + 1, sh + 1, sw, sh, "bottom").y == 0);
  CHECK(placeOnRim(sw, 0, sw + 1, sh + 1, sw, sh, "right").x == 0);

  CHECK(nearestEdge(50, 54, sw, sh, "top") == "top");
  CHECK(nearestEdge(50, 59, sw, sh, "top") == "left");

  std::cout << "ok\n";
  return 0;
}
