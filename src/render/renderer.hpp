#pragma once

#include "types/context.hpp"

class IRenderer {
  public:
    virtual ~IRenderer() = default;
    virtual void render(Context &ctx) = 0;
};
