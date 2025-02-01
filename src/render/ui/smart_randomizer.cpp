#include "smart_randomizer.hpp"
#include <algorithm>
#include <cmath>
#include <random>

std::vector<float>
SmartRandomizer::generate_rules(const std::vector<Color> &colors,
                                const std::vector<int> &sizes, int G) {
    std::vector<float> rules(G * G, 0.0f);

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> random_dist(-0.8f, 0.8f);

    for (int i = 0; i < G; ++i) {
        for (int j = 0; j < G; ++j) {
            if (i == j) {
                // Self-interaction: set to zero or very small value
                rules[i * G + j] = 0.0f;
                continue;
            }

            // Calculate color temperature for both groups
            float temp_i = color_temperature(colors[i]);
            float temp_j = color_temperature(colors[j]);

            // Temperature affinity: similar temps attract (negative values)
            float temp_diff = std::abs(temp_i - temp_j);
            float temp_factor =
                1.0f - temp_diff; // 0.0 (opposite) to 1.0 (same)

            // Size-based clustering: smaller groups orbit larger groups
            int size_i = sizes[i];
            int size_j = sizes[j];
            float size_ratio = static_cast<float>(std::min(size_i, size_j)) /
                               static_cast<float>(std::max(size_i, size_j));
            float size_factor = 0.3f + 0.7f * size_ratio; // 0.3 to 1.0

            // Combine factors: NEGATIVE = attraction, POSITIVE = repulsion
            // Invert the logic: similar colors should get negative values
            // (attraction)
            float base_strength =
                -((temp_factor * 0.6f + size_factor * 0.4f) * 2.0f -
                  1.0f); // 1.0 to -1.0 (inverted)
            float random_var = random_dist(rng);

            // Clamp to reasonable range
            rules[i * G + j] =
                std::clamp(base_strength + random_var, -2.0f, 2.0f);
        }
    }

    return rules;
}

float SmartRandomizer::color_temperature(const Color &color) const {
    HSL hsl = rgb_to_hsl(color);
    return hue_to_warmth(hsl.h);
}

SmartRandomizer::HSL SmartRandomizer::rgb_to_hsl(const Color &color) const {
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;

    float max_val = std::max({r, g, b});
    float min_val = std::min({r, g, b});
    float delta = max_val - min_val;

    float l = (max_val + min_val) / 2.0f;

    if (delta == 0.0f) {
        return {0.0f, 0.0f, l}; // Grayscale
    }

    float s = (l < 0.5f) ? (delta / (max_val + min_val))
                         : (delta / (2.0f - max_val - min_val));

    float h;
    if (max_val == r) {
        h = ((g - b) / delta) + (g < b ? 6.0f : 0.0f);
    } else if (max_val == g) {
        h = (b - r) / delta + 2.0f;
    } else {
        h = (r - g) / delta + 4.0f;
    }
    h /= 6.0f; // Convert to 0-1 range

    return {h, s, l};
}

float SmartRandomizer::hue_to_warmth(float hue) const {
    // Convert hue (0-1) to degrees (0-360)
    float degrees = hue * 360.0f;

    // Map hue to warmth based on color temperature
    if (degrees >= 0.0f && degrees < 60.0f) {
        // Red-orange: warm
        return 0.8f + 0.2f * (degrees / 60.0f);
    } else if (degrees >= 60.0f && degrees < 180.0f) {
        // Yellow-green: neutral-cool
        return 0.7f - 0.4f * ((degrees - 60.0f) / 120.0f);
    } else if (degrees >= 180.0f && degrees < 270.0f) {
        // Cyan-blue: cool
        return 0.3f - 0.3f * ((degrees - 180.0f) / 90.0f);
    } else {
        // Magenta-red: warm
        return 0.5f + 0.3f * ((degrees - 270.0f) / 90.0f);
    }
}
