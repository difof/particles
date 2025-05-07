#pragma once

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "../irenderer.hpp"

/**
 * @brief UI component for inspecting particle regions and tracking individual
 * particles.
 *
 * Provides functionality for selecting rectangular regions on screen,
 * displaying particle statistics within those regions, and tracking individual
 * particles with real-time position and velocity information.
 */
class InspectorUI : public IRenderer {
  public:
    InspectorUI();
    ~InspectorUI() override;
    InspectorUI(const InspectorUI &) = delete;
    InspectorUI(InspectorUI &&) = delete;
    InspectorUI &operator=(const InspectorUI &) = delete;
    InspectorUI &operator=(InspectorUI &&) = delete;

    /**
     * @brief Resizes the render texture to match current screen dimensions.
     */
    void resize();

    /**
     * @brief Gets the render texture for external use.
     * @return Reference to the render texture.
     */
    RenderTexture2D &texture();

    /**
     * @brief Gets the render texture for external use (const version).
     * @return Const reference to the render texture.
     */
    const RenderTexture2D &texture() const;

    /**
     * @brief Renders the inspector UI components.
     * @param ctx The rendering context containing simulation state and
     * configuration.
     */
    void render(Context &ctx) override;

    /**
     * @brief Updates selection state based on mouse input.
     * @param ctx The rendering context.
     */
    void update_selection_from_mouse(Context &ctx);

    /**
     * @brief Renders the inspector UI window with particle statistics and
     * region preview.
     * @param ctx The rendering context.
     * @param color_rt The color render texture for region preview.
     */
    void render_ui(Context &ctx, const RenderTexture2D &color_rt);

  private:
    /**
     * @brief Updates the selection rectangle to follow the tracked particle.
     * @param ctx The rendering context.
     */
    void follow_tracked(Context &ctx);

    /**
     * @brief Draws the selection overlay on the render texture.
     */
    void draw_selection_overlay();

    /**
     * @brief Renders the tracking controls section of the UI.
     * @param ctx The rendering context.
     * @param logical The normalized selection rectangle.
     */
    void render_tracking_controls(Context &ctx, const Rectangle &logical);

    /**
     * @brief Renders the particle statistics section of the UI.
     * @param ctx The rendering context.
     * @param logical The normalized selection rectangle.
     * @param camera_offset The camera offset for coordinate transformation.
     */
    void render_particle_statistics(Context &ctx, const Rectangle &logical,
                                    const Vector2 &camera_offset);

    /**
     * @brief Renders the region preview image and handles particle picking.
     * @param ctx The rendering context.
     * @param logical The normalized selection rectangle.
     * @param camera_offset The camera offset for coordinate transformation.
     * @param color_rt The color render texture for preview.
     */
    void render_region_preview(Context &ctx, const Rectangle &logical,
                               const Vector2 &camera_offset,
                               const RenderTexture2D &color_rt);

    /**
     * @brief Handles particle picking from the preview image.
     * @param ctx The rendering context.
     * @param logical The normalized selection rectangle.
     * @param camera_offset The camera offset for coordinate transformation.
     * @param min The minimum coordinates of the preview image.
     * @param size The size of the preview image.
     */
    void handle_particle_picking(Context &ctx, const Rectangle &logical,
                                 const Vector2 &camera_offset,
                                 const ImVec2 &min, const ImVec2 &size);

  private:
    /**
     * @brief Structure holding the state of region selection and particle
     * tracking.
     */
    struct RegionSelection {
        /** @brief Whether the inspector window is visible. */
        bool show_window = false;
        /** @brief Whether a selection has been made. */
        bool has = false;
        /** @brief Whether currently dragging to create selection. */
        bool dragging = false;
        /** @brief The selection rectangle in screen coordinates. */
        Rectangle rect{0, 0, 0, 0};
        /** @brief Whether particle tracking is enabled. */
        bool track_enabled = false;
        /** @brief ID of the currently tracked particle. */
        int tracked_id = -1;
        /** @brief Group ID of the currently tracked particle. */
        int tracked_group = -1;
        /** @brief Base width for tracking rectangle. */
        float base_w = 0.f;
        /** @brief Base height for tracking rectangle. */
        float base_h = 0.f;
        /** @brief Whether to pick particle from preview image. */
        bool want_pick_from_preview = false;
        /** @brief Last known velocity of tracked particle. */
        Vector2 last_vel{0.f, 0.f};
        /** @brief Whether last velocity is valid. */
        bool has_last_vel = false;
    } m_selection;

    /** @brief Render texture for drawing selection overlays. */
    RenderTexture2D m_render_texture{};
};