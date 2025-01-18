#ifndef __IRENDERER_HPP
#define __IRENDERER_HPP

#include "context.hpp"

class IRenderer {
  public:
    virtual ~IRenderer() = default;
    virtual void render(RenderContext &ctx) = 0;
};

#endif
