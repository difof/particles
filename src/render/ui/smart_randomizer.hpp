#pragma once

#include <raylib.h>
#include <vector>

class SmartRandomizer {
  public:
    SmartRandomizer() = default;
    ~SmartRandomizer() = default;

    // Generate intelligent rules based on group properties
    std::vector<float> generate_rules(const std::vector<Color> &colors,
                                      const std::vector<int> &sizes, int G);

  private:
    // Convert RGB color to HSL and extract temperature (0.0 = cool, 1.0 = warm)
    float color_temperature(const Color &color) const;

    // Convert RGB to HSL
    struct HSL {
        float h, s, l;
    };
    HSL rgb_to_hsl(const Color &color) const;

    // Extract warmth from hue angle
    float hue_to_warmth(float hue) const;
};
