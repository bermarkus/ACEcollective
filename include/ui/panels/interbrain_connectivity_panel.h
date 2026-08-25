#pragma once

#include "ui/panel_base.h"
#include <vector>
#include <utility>

namespace ace {
namespace ui {

/**
 * @brief Main workspace view: visualizes live cross-channel connectivity.
 * Two view modes, switched via SettingsPanel's View dropdown
 * (ApplicationInterface::get/set_view_type()):
 *
 * - PairDetail: one pair at a time -- a bipartite "two columns" layout
 *   (participant A's channels down the left, B's down the right, connecting
 *   lines colored/weighted by coherence strength), plus an at-a-glance
 *   overview grid of every pair's mean connectivity when more than 2
 *   participants are in the session. Adapted from ACEapp's ConnectivityPanel
 *   (ui/panels/connectivity_panel.h there), whose single circular electrode
 *   layout doesn't generalize to two independent electrode sets.
 * - BlockMatrix: every participant's channels concatenated onto one shared
 *   axis, rendered as a single heatmap -- shows all channels, all pairs, at
 *   once. See render_block_matrix().
 * - Circular: every participant's channels arranged around a shared ring
 *   (one arc segment per participant, sized by their channel count), with
 *   chords between channels above the threshold, colored/weighted by
 *   coherence. Hovering a node isolates its own connections and reports its
 *   strongest link. See render_circular().
 * - SmallMultiples: one mini heatmap thumbnail per participant pair, tiled
 *   in a flow layout -- same information as BlockMatrix's off-diagonal
 *   blocks, but organized by pair instead of one shared axis, so a specific
 *   relationship is easier to pick out. Click a tile to jump to PairDetail
 *   on that pair. See render_small_multiples().
 * - ForceDirected: every channel is a node (colored by participant); a
 *   lightweight spring simulation pulls coherent channel pairs together and
 *   pushes all nodes apart, so clusters of high inter-brain coherence
 *   visually emerge. Runs continuously each frame. See render_force_directed().
 *
 * All modes share the dark canvas (theme::kBgCanvas), coherence color ramp,
 * and per-participant identity colors so they read as the same visual family.
 */
class InterBrainConnectivityPanel : public PanelBase {
public:
    explicit InterBrainConnectivityPanel(ApplicationInterface* app);
    void render_content() override;

private:
    void render_pair_overview_grid(const std::vector<std::pair<size_t, size_t>>& pairs);
    void render_pair_view(size_t index_a, size_t index_b);
    void render_block_matrix();
    void render_circular();
    void render_small_multiples();
    void render_force_directed();
    void step_force_simulation(const std::vector<size_t>& offsets, size_t total_channels,
                                const ImVec2& canvas_center, const ImVec2& canvas_size);

    int selected_pair_flat_index_ = 0;

    // Force-directed layout state, persists across frames so the simulation
    // settles instead of re-randomizing every render. Reseeded whenever the
    // channel count changes (participant added/removed).
    std::vector<ImVec2> force_positions_;
    std::vector<ImVec2> force_velocities_;
    bool force_paused_ = false;
};

} // namespace ui
} // namespace ace
