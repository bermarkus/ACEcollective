#include "ui/panels/interbrain_connectivity_panel.h"
#include "ui/theme.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace ace {
namespace ui {

namespace {
ImU32 coherence_color(float value)
{
    namespace T = ace::ui::theme;
    value = std::clamp(value, 0.0f, 1.0f);
    ImVec4 c;
    if (value < 0.33f) {
        float t = value / 0.33f;
        c = ImVec4(T::kCoherenceLow.x + t * (T::kCoherenceModerate.x - T::kCoherenceLow.x),
                   T::kCoherenceLow.y + t * (T::kCoherenceModerate.y - T::kCoherenceLow.y),
                   T::kCoherenceLow.z + t * (T::kCoherenceModerate.z - T::kCoherenceLow.z), 1.0f);
    } else if (value < 0.66f) {
        float t = (value - 0.33f) / 0.33f;
        c = ImVec4(T::kCoherenceModerate.x + t * (T::kCoherenceHigh.x - T::kCoherenceModerate.x),
                   T::kCoherenceModerate.y + t * (T::kCoherenceHigh.y - T::kCoherenceModerate.y),
                   T::kCoherenceModerate.z + t * (T::kCoherenceHigh.z - T::kCoherenceModerate.z), 1.0f);
    } else {
        float t = (value - 0.66f) / 0.34f;
        c = ImVec4(T::kCoherenceHigh.x + t * (T::kCoherenceVeryHigh.x - T::kCoherenceHigh.x),
                   T::kCoherenceHigh.y + t * (T::kCoherenceVeryHigh.y - T::kCoherenceHigh.y),
                   T::kCoherenceHigh.z + t * (T::kCoherenceVeryHigh.z - T::kCoherenceHigh.z), 1.0f);
    }
    return T::to_im_col32(c);
}
} // namespace

InterBrainConnectivityPanel::InterBrainConnectivityPanel(ApplicationInterface* app)
    : PanelBase(app, "Connectivity")
{
}

void InterBrainConnectivityPanel::render_content()
{
    namespace T = ace::ui::theme;
    const auto& participants = app_->get_participants();

    if (participants.size() < 2) {
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::TextWrapped("Add at least 2 participants in the Connect tab and start a session "
                            "to see inter-brain coherence here.");
        return;
    }

    std::vector<std::pair<size_t, size_t>> pairs;
    for (size_t i = 0; i < participants.size(); ++i)
        for (size_t j = i + 1; j < participants.size(); ++j)
            pairs.emplace_back(i, j);

    if (!app_->is_session_active()) {
        T::text_status(T::StatusLevel::Warn, "Session not started -- press Start Session in Connect.");
    }

    if (app_->get_view_type() == ace::ui::ConnectivityViewType::BlockMatrix) {
        render_block_matrix();
        return;
    }
    if (app_->get_view_type() == ace::ui::ConnectivityViewType::Circular) {
        render_circular();
        return;
    }
    if (app_->get_view_type() == ace::ui::ConnectivityViewType::SmallMultiples) {
        render_small_multiples();
        return;
    }
    if (app_->get_view_type() == ace::ui::ConnectivityViewType::ForceDirected) {
        render_force_directed();
        return;
    }

    if (pairs.size() > 1) {
        render_pair_overview_grid(pairs);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    selected_pair_flat_index_ = std::clamp(selected_pair_flat_index_, 0, static_cast<int>(pairs.size()) - 1);
    auto [a, b] = pairs[selected_pair_flat_index_];

    if (pairs.size() > 1) {
        std::string preview = participants[a].participant_label + " <-> " + participants[b].participant_label;
        if (ImGui::BeginCombo("Pair", preview.c_str())) {
            for (int k = 0; k < static_cast<int>(pairs.size()); ++k) {
                auto [pi, pj] = pairs[k];
                std::string label = participants[pi].participant_label + " <-> " + participants[pj].participant_label;
                if (ImGui::Selectable(label.c_str(), k == selected_pair_flat_index_)) {
                    selected_pair_flat_index_ = k;
                }
            }
            ImGui::EndCombo();
        }
    }

    render_pair_view(a, b);
}

void InterBrainConnectivityPanel::render_pair_overview_grid(const std::vector<std::pair<size_t, size_t>>& pairs)
{
    namespace T = ace::ui::theme;
    const auto& participants = app_->get_participants();

    T::section_header("Session Overview", "Mean coherence across each pair's full cross-channel matrix.");

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    const float cell = 36.0f;
    const float label_w = 90.0f;

    ImGui::Dummy(ImVec2(label_w + cell * participants.size(), label_w + cell * participants.size()));

    for (size_t col = 0; col < participants.size(); ++col) {
        ImVec2 pos(origin.x + label_w + col * cell, origin.y + label_w - 4.0f);
        draw_list->AddText(ImVec2(pos.x + 2, pos.y - 14), T::to_im_col32(T::kSectionHeader),
                            participants[col].participant_label.c_str());
    }
    for (size_t row = 0; row < participants.size(); ++row) {
        ImVec2 pos(origin.x, origin.y + label_w + row * cell);
        draw_list->AddText(ImVec2(pos.x, pos.y + cell * 0.3f), T::to_im_col32(T::kSectionHeader),
                            participants[row].participant_label.c_str());
    }

    for (size_t k = 0; k < pairs.size(); ++k) {
        auto [i, j] = pairs[k];
        auto matrix = app_->get_pairwise_matrix(participants[i].lsl_stream_name, participants[j].lsl_stream_name);
        float mean_val = app_->get_pairwise_mean(participants[i].lsl_stream_name, participants[j].lsl_stream_name);

        ImVec2 tl(origin.x + label_w + j * cell, origin.y + label_w + i * cell);
        ImVec2 br(tl.x + cell - 2, tl.y + cell - 2);
        ImU32 col = matrix.empty() ? T::to_im_col32(T::kStatusMuted, 0.3f) : coherence_color(mean_val);
        draw_list->AddRectFilled(tl, br, col, 3.0f);

        if (!matrix.empty()) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2f", mean_val);
            draw_list->AddText(ImVec2(tl.x + 4, tl.y + 8), IM_COL32(0, 0, 0, 200), buf);
        }

        // Clickable overlay to jump the detail view to this pair.
        ImGui::SetCursorScreenPos(tl);
        ImGui::PushID(static_cast<int>(k));
        if (ImGui::InvisibleButton("##cell", ImVec2(cell - 2, cell - 2))) {
            selected_pair_flat_index_ = static_cast<int>(k);
        }
        ImGui::PopID();
    }
}

void InterBrainConnectivityPanel::render_pair_view(size_t index_a, size_t index_b)
{
    namespace T = ace::ui::theme;
    const auto& participants = app_->get_participants();
    const auto& pa = participants[index_a];
    const auto& pb = participants[index_b];

    auto matrix = app_->get_pairwise_matrix(pa.lsl_stream_name, pb.lsl_stream_name);

    ImVec2 canvas_size(ImGui::GetContentRegionAvail().x, std::max(400.0f, ImGui::GetContentRegionAvail().y));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, ImVec2(origin.x + canvas_size.x, origin.y + canvas_size.y), T::kBgCanvas, 6.0f);

    if (matrix.empty()) {
        draw_list->AddText(ImVec2(origin.x + 16, origin.y + 16), T::to_im_col32(T::kStatusMuted),
                            "Waiting for enough data from both participants...");
        ImGui::Dummy(canvas_size);
        return;
    }

    size_t n_a = pa.channel_names.size();
    size_t n_b = pb.channel_names.size();
    float margin_y = 30.0f;
    float usable_h = canvas_size.y - 2 * margin_y;
    float col_a_x = origin.x + 90.0f;
    float col_b_x = origin.x + canvas_size.x - 90.0f;

    ImVec4 color_a = T::kParticipantColors[index_a % 6];
    ImVec4 color_b = T::kParticipantColors[index_b % 6];

    std::vector<ImVec2> pos_a(n_a), pos_b(n_b);
    for (size_t i = 0; i < n_a; ++i) {
        pos_a[i] = ImVec2(col_a_x, origin.y + margin_y + (n_a > 1 ? usable_h * i / (n_a - 1) : usable_h * 0.5f));
    }
    for (size_t j = 0; j < n_b; ++j) {
        pos_b[j] = ImVec2(col_b_x, origin.y + margin_y + (n_b > 1 ? usable_h * j / (n_b - 1) : usable_h * 0.5f));
    }

    // Connections first (so channel dots draw on top).
    float threshold = app_->get_connection_threshold();
    for (size_t i = 0; i < n_a && i < matrix.size(); ++i) {
        for (size_t j = 0; j < n_b && j < matrix[i].size(); ++j) {
            float v = matrix[i][j];
            if (v < threshold) continue;
            float thickness = 1.0f + v * 4.0f;
            draw_list->AddLine(pos_a[i], pos_b[j], coherence_color(v), thickness);
        }
    }

    for (size_t i = 0; i < n_a; ++i) {
        draw_list->AddCircleFilled(pos_a[i], 5.0f, T::to_im_col32(color_a));
        draw_list->AddText(ImVec2(pos_a[i].x - 85.0f, pos_a[i].y - 7.0f), T::to_im_col32(color_a),
                            pa.channel_names[i].c_str());
    }
    for (size_t j = 0; j < n_b; ++j) {
        draw_list->AddCircleFilled(pos_b[j], 5.0f, T::to_im_col32(color_b));
        draw_list->AddText(ImVec2(pos_b[j].x + 10.0f, pos_b[j].y - 7.0f), T::to_im_col32(color_b),
                            pb.channel_names[j].c_str());
    }

    draw_list->AddText(ImVec2(col_a_x - 20.0f, origin.y + 8.0f), T::to_im_col32(color_a), pa.participant_label.c_str());
    draw_list->AddText(ImVec2(col_b_x - 20.0f, origin.y + 8.0f), T::to_im_col32(color_b), pb.participant_label.c_str());

    ImGui::Dummy(canvas_size);
}

void InterBrainConnectivityPanel::render_block_matrix()
{
    namespace T = ace::ui::theme;
    const auto& participants = app_->get_participants();

    // Cumulative channel offsets so every participant's channels land on one
    // shared axis: [P1 ch0..chN][P2 ch0..chN]...
    std::vector<size_t> offsets(participants.size());
    size_t total_channels = 0;
    for (size_t i = 0; i < participants.size(); ++i) {
        offsets[i] = total_channels;
        total_channels += participants[i].channel_names.size();
    }
    if (total_channels == 0) {
        ImGui::TextWrapped("Waiting for channel info from participants...");
        return;
    }

    T::section_header("All Channels, All Pairs",
        "Every participant's channels on one shared axis. Off-diagonal blocks show "
        "inter-brain coherence; each participant's own block (diagonal, grayed) isn't computed.");

    const float cell = 10.0f;
    const float label_margin = 110.0f;
    ImVec2 grid_size(total_channels * cell, total_channels * cell);

    ImGui::BeginChild("##BlockMatrixScroll", ImGui::GetContentRegionAvail(), false,
                       ImGuiWindowFlags_HorizontalScrollbar);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    origin.x += label_margin;
    origin.y += label_margin;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(origin, ImVec2(origin.x + grid_size.x, origin.y + grid_size.y), T::kBgCanvas);

    // Participant axis bands + labels (top and left), instead of one label
    // per channel -- unreadable at 6 participants x 32 channels.
    for (size_t p = 0; p < participants.size(); ++p) {
        size_t n = participants[p].channel_names.size();
        if (n == 0) continue;
        ImU32 col32 = T::to_im_col32(T::kParticipantColors[p % 6]);
        float band_start = offsets[p] * cell;
        float band_len = n * cell;

        draw_list->AddRectFilled(ImVec2(origin.x + band_start, origin.y - 14.0f),
                                  ImVec2(origin.x + band_start + band_len, origin.y - 2.0f), col32);
        draw_list->AddText(ImVec2(origin.x + band_start + 2.0f, origin.y - 14.0f), IM_COL32(0, 0, 0, 255),
                            participants[p].participant_label.c_str());

        draw_list->AddRectFilled(ImVec2(origin.x - 14.0f, origin.y + band_start),
                                  ImVec2(origin.x - 2.0f, origin.y + band_start + band_len), col32);
    }

    // Off-diagonal blocks: fill with each pair's cached matrix (recomputed
    // every 0.5s by Application::recompute_all_pairs(), not here). Diagonal
    // blocks (a participant against themselves) are left as bare canvas --
    // that connectivity isn't computed, on purpose (see class doc comment).
    for (size_t pi = 0; pi < participants.size(); ++pi) {
        for (size_t pj = 0; pj < participants.size(); ++pj) {
            if (pi == pj) continue;
            auto matrix = app_->get_pairwise_matrix(participants[pi].lsl_stream_name, participants[pj].lsl_stream_name);
            for (size_t r = 0; r < matrix.size(); ++r) {
                for (size_t c = 0; c < matrix[r].size(); ++c) {
                    // Exactly 0.0f means "filtered out" in ChannelToChannel
                    // mode (see Application::recompute_all_pairs()), not a
                    // genuine near-zero measurement -- real coherence values
                    // essentially never land on exact 0.0f. Leaving these as
                    // bare canvas instead of a "low coherence" color keeps
                    // the matched-channel diagonal visible instead of
                    // burying it in a fully-colored block.
                    if (matrix[r][c] == 0.0f) continue;
                    ImVec2 tl(origin.x + (offsets[pj] + c) * cell, origin.y + (offsets[pi] + r) * cell);
                    draw_list->AddRectFilled(tl, ImVec2(tl.x + cell, tl.y + cell), coherence_color(matrix[r][c]));
                }
            }
        }
    }

    // Hover: map mouse position back to (participant, channel) on each axis
    // and show the exact pair + value -- the heatmap is too dense for
    // permanent per-cell labels, so this is how exact readings happen.
    ImVec2 mouse = ImGui::GetMousePos();
    if (mouse.x >= origin.x && mouse.y >= origin.y &&
        mouse.x < origin.x + grid_size.x && mouse.y < origin.y + grid_size.y) {
        size_t col = static_cast<size_t>((mouse.x - origin.x) / cell);
        size_t row = static_cast<size_t>((mouse.y - origin.y) / cell);

        auto locate = [&](size_t global_index) -> std::pair<size_t, size_t> {
            for (size_t p = 0; p < participants.size(); ++p) {
                size_t n = participants[p].channel_names.size();
                if (global_index < offsets[p] + n) return {p, global_index - offsets[p]};
            }
            return {participants.size() - 1, 0};
        };
        auto row_loc = locate(row);
        auto col_loc = locate(col);

        if (row_loc.first != col_loc.first) {
            auto matrix = app_->get_pairwise_matrix(participants[row_loc.first].lsl_stream_name,
                                                      participants[col_loc.first].lsl_stream_name);
            float v = (row_loc.second < matrix.size() && col_loc.second < matrix[row_loc.second].size())
                          ? matrix[row_loc.second][col_loc.second] : 0.0f;
            ImGui::BeginTooltip();
            ImGui::Text("%s %s <-> %s %s",
                        participants[row_loc.first].participant_label.c_str(),
                        participants[row_loc.first].channel_names[row_loc.second].c_str(),
                        participants[col_loc.first].participant_label.c_str(),
                        participants[col_loc.first].channel_names[col_loc.second].c_str());
            ImGui::Text("Coherence: %.3f", v);
            ImGui::EndTooltip();
        }

        ImVec2 hl_tl(origin.x + col * cell, origin.y + row * cell);
        draw_list->AddRect(hl_tl, ImVec2(hl_tl.x + cell, hl_tl.y + cell), IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.5f);
    }

    ImGui::Dummy(ImVec2(grid_size.x + label_margin, grid_size.y + label_margin));
    ImGui::EndChild();
}

void InterBrainConnectivityPanel::render_circular()
{
    namespace T = ace::ui::theme;
    const float kPi = 3.14159265358979323846f;
    const auto& participants = app_->get_participants();

    std::vector<size_t> offsets(participants.size());
    size_t total_channels = 0;
    for (size_t i = 0; i < participants.size(); ++i) {
        offsets[i] = total_channels;
        total_channels += participants[i].channel_names.size();
    }
    if (total_channels == 0) {
        ImGui::TextWrapped("Waiting for channel info from participants...");
        return;
    }

    T::section_header("All Channels, All Pairs (Circular)",
        "Every participant gets an arc segment of the ring, sized by channel count. "
        "Hover a node to isolate its connections and see its strongest link.");

    ImVec2 canvas_size(ImGui::GetContentRegionAvail().x, std::max(500.0f, ImGui::GetContentRegionAvail().y));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, ImVec2(origin.x + canvas_size.x, origin.y + canvas_size.y), T::kBgCanvas, 6.0f);

    ImVec2 center(origin.x + canvas_size.x * 0.5f, origin.y + canvas_size.y * 0.5f);
    float radius = std::min(canvas_size.x, canvas_size.y) * 0.5f - 70.0f;
    if (radius < 20.0f) radius = 20.0f;

    // Each participant gets an arc segment proportional to their channel
    // count, separated by a small fixed gap -- a standard chord-diagram
    // layout (Circos-style), generalized here to N independent brains
    // instead of N genomic chromosomes.
    const float gap_rad = 8.0f * (kPi / 180.0f);
    float available_angle = 2.0f * kPi - gap_rad * participants.size();

    std::vector<ImVec2> channel_pos(total_channels);
    float angle_cursor = -kPi * 0.5f; // start at the top, like a clock face

    for (size_t p = 0; p < participants.size(); ++p) {
        size_t n = participants[p].channel_names.size();
        if (n == 0) continue;
        float span = available_angle * (static_cast<float>(n) / total_channels);
        float start = angle_cursor;
        float end = start + span;

        ImU32 col32 = T::to_im_col32(T::kParticipantColors[p % 6]);
        draw_list->PathArcTo(center, radius + 10.0f, start, end);
        draw_list->PathStroke(col32, 0, 4.0f);

        float mid = (start + end) * 0.5f;
        ImVec2 label_pos(center.x + (radius + 26.0f) * cosf(mid), center.y + (radius + 26.0f) * sinf(mid));
        ImVec2 text_size = ImGui::CalcTextSize(participants[p].participant_label.c_str());
        draw_list->AddText(ImVec2(label_pos.x - text_size.x * 0.5f, label_pos.y - text_size.y * 0.5f),
                            col32, participants[p].participant_label.c_str());

        for (size_t local = 0; local < n; ++local) {
            float a = (n > 1) ? (start + span * local / (n - 1)) : mid;
            channel_pos[offsets[p] + local] = ImVec2(center.x + radius * cosf(a), center.y + radius * sinf(a));
        }

        angle_cursor = end + gap_rad;
    }

    auto locate = [&](size_t global_index) -> std::pair<size_t, size_t> {
        for (size_t p = 0; p < participants.size(); ++p) {
            size_t n = participants[p].channel_names.size();
            if (global_index < offsets[p] + n) return {p, global_index - offsets[p]};
        }
        return {participants.size() - 1, 0};
    };

    // Hover: nearest node within a small pixel radius.
    ImVec2 mouse = ImGui::GetMousePos();
    int hovered = -1;
    float best_dist2 = 12.0f * 12.0f;
    for (size_t g = 0; g < total_channels; ++g) {
        float dx = mouse.x - channel_pos[g].x;
        float dy = mouse.y - channel_pos[g].y;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_dist2) { best_dist2 = d2; hovered = static_cast<int>(g); }
    }

    // Chords: every cross-participant channel pair above threshold. While a
    // node is hovered, only its own chords are drawn -- otherwise a 6-person
    // session draws thousands of overlapping curves and nothing is legible.
    float threshold = app_->get_connection_threshold();
    for (size_t pi = 0; pi < participants.size(); ++pi) {
        for (size_t pj = pi + 1; pj < participants.size(); ++pj) {
            auto matrix = app_->get_pairwise_matrix(participants[pi].lsl_stream_name, participants[pj].lsl_stream_name);
            for (size_t r = 0; r < matrix.size(); ++r) {
                for (size_t c = 0; c < matrix[r].size(); ++c) {
                    float v = matrix[r][c];
                    if (v < threshold) continue;
                    size_t g1 = offsets[pi] + r;
                    size_t g2 = offsets[pj] + c;
                    bool involved = hovered >= 0 &&
                        (static_cast<size_t>(hovered) == g1 || static_cast<size_t>(hovered) == g2);
                    if (hovered >= 0 && !involved) continue;

                    ImVec2 mid((channel_pos[g1].x + channel_pos[g2].x) * 0.5f,
                                (channel_pos[g1].y + channel_pos[g2].y) * 0.5f);
                    ImVec2 control(center.x + (mid.x - center.x) * 0.3f, center.y + (mid.y - center.y) * 0.3f);
                    float thickness = involved ? (2.0f + v * 5.0f) : (0.75f + v * 2.5f);
                    draw_list->AddBezierQuadratic(channel_pos[g1], control, channel_pos[g2], coherence_color(v), thickness);
                }
            }
        }
    }

    // Channel nodes, drawn on top of the chords.
    for (size_t g = 0; g < total_channels; ++g) {
        auto [p, local] = locate(g);
        (void)local;
        draw_list->AddCircleFilled(channel_pos[g], 3.5f, T::to_im_col32(T::kParticipantColors[p % 6]));
    }

    if (hovered >= 0) {
        size_t g = static_cast<size_t>(hovered);
        draw_list->AddCircleFilled(channel_pos[g], 6.0f, IM_COL32(255, 255, 255, 255));

        auto [hp, hc] = locate(g);
        float best_v = -1.0f;
        std::string best_label;
        for (size_t p = 0; p < participants.size(); ++p) {
            if (p == hp) continue;
            auto matrix = app_->get_pairwise_matrix(participants[hp].lsl_stream_name, participants[p].lsl_stream_name);
            if (hc >= matrix.size()) continue;
            for (size_t c = 0; c < matrix[hc].size(); ++c) {
                if (matrix[hc][c] > best_v) {
                    best_v = matrix[hc][c];
                    best_label = participants[p].participant_label + " " + participants[p].channel_names[c];
                }
            }
        }

        ImGui::BeginTooltip();
        ImGui::Text("%s %s", participants[hp].participant_label.c_str(), participants[hp].channel_names[hc].c_str());
        if (best_v >= 0.0f) {
            ImGui::Text("Strongest link: %s (%.3f)", best_label.c_str(), best_v);
        }
        ImGui::EndTooltip();
    }

    ImGui::Dummy(canvas_size);
}

void InterBrainConnectivityPanel::render_small_multiples()
{
    namespace T = ace::ui::theme;
    const auto& participants = app_->get_participants();

    std::vector<std::pair<size_t, size_t>> pairs;
    for (size_t i = 0; i < participants.size(); ++i)
        for (size_t j = i + 1; j < participants.size(); ++j)
            pairs.emplace_back(i, j);
    if (pairs.empty()) {
        ImGui::TextWrapped("Add at least 2 participants to see pairwise heatmaps.");
        return;
    }

    T::section_header("All Pairs (Small Multiples)",
        "One mini heatmap per participant pair. Click a tile to open it in Pair Detail.");

    const float tile_w = 220.0f;
    const float tile_h = 200.0f;
    const float tile_pad = 12.0f;
    float avail_w = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>((avail_w + tile_pad) / (tile_w + tile_pad)));
    int rows_used = static_cast<int>((pairs.size() + cols - 1) / cols);

    ImGui::BeginChild("##SmallMultiplesScroll", ImGui::GetContentRegionAvail(), false);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (size_t k = 0; k < pairs.size(); ++k) {
        size_t i = pairs[k].first, j = pairs[k].second;
        int col = static_cast<int>(k) % cols;
        int row = static_cast<int>(k) / cols;
        ImVec2 tile_origin(origin.x + col * (tile_w + tile_pad), origin.y + row * (tile_h + tile_pad));

        draw_list->AddRectFilled(tile_origin, ImVec2(tile_origin.x + tile_w, tile_origin.y + tile_h), T::kBgCanvas, 4.0f);

        std::string label = participants[i].participant_label + " <-> " + participants[j].participant_label;
        draw_list->AddText(ImVec2(tile_origin.x + 6, tile_origin.y + 4), T::to_im_col32(T::kSectionHeader), label.c_str());

        auto matrix = app_->get_pairwise_matrix(participants[i].lsl_stream_name, participants[j].lsl_stream_name);
        ImVec2 heat_origin(tile_origin.x + 6, tile_origin.y + 22);
        ImVec2 heat_size(tile_w - 12, tile_h - 44);

        if (!matrix.empty() && !matrix[0].empty()) {
            size_t n_rows = matrix.size();
            size_t n_cols = matrix[0].size();
            float cell_w = heat_size.x / n_cols;
            float cell_h = heat_size.y / n_rows;
            for (size_t r = 0; r < n_rows; ++r) {
                for (size_t c = 0; c < n_cols; ++c) {
                    if (matrix[r][c] == 0.0f) continue; // filtered out in ChannelToChannel mode -- see block matrix's note
                    ImVec2 tl(heat_origin.x + c * cell_w, heat_origin.y + r * cell_h);
                    draw_list->AddRectFilled(tl, ImVec2(tl.x + cell_w, tl.y + cell_h), coherence_color(matrix[r][c]));
                }
            }

            ImVec2 mouse = ImGui::GetMousePos();
            if (mouse.x >= heat_origin.x && mouse.y >= heat_origin.y &&
                mouse.x < heat_origin.x + heat_size.x && mouse.y < heat_origin.y + heat_size.y) {
                size_t c = static_cast<size_t>((mouse.x - heat_origin.x) / cell_w);
                size_t r = static_cast<size_t>((mouse.y - heat_origin.y) / cell_h);
                if (r < n_rows && c < n_cols) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s %s <-> %s %s", participants[i].participant_label.c_str(),
                                participants[i].channel_names[r].c_str(),
                                participants[j].participant_label.c_str(),
                                participants[j].channel_names[c].c_str());
                    ImGui::Text("Coherence: %.3f", matrix[r][c]);
                    ImGui::EndTooltip();
                }
            }
        } else {
            draw_list->AddText(ImVec2(heat_origin.x, heat_origin.y + heat_size.y * 0.5f),
                                T::to_im_col32(T::kStatusMuted), "waiting for data...");
        }

        char mean_buf[32];
        float mean_val = app_->get_pairwise_mean(participants[i].lsl_stream_name, participants[j].lsl_stream_name);
        std::snprintf(mean_buf, sizeof(mean_buf), "mean %.2f", mean_val);
        draw_list->AddText(ImVec2(tile_origin.x + 6, tile_origin.y + tile_h - 18),
                            T::to_im_col32(T::kStatusMuted), mean_buf);

        ImGui::SetCursorScreenPos(tile_origin);
        ImGui::PushID(static_cast<int>(k));
        if (ImGui::InvisibleButton("##tile", ImVec2(tile_w, tile_h))) {
            selected_pair_flat_index_ = static_cast<int>(k);
            app_->set_view_type(ace::ui::ConnectivityViewType::PairDetail);
        }
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(cols * (tile_w + tile_pad), rows_used * (tile_h + tile_pad)));
    ImGui::EndChild();
}

void InterBrainConnectivityPanel::step_force_simulation(const std::vector<size_t>& offsets, size_t total_channels,
                                                          const ImVec2& canvas_center, const ImVec2& canvas_size)
{
    const auto& participants = app_->get_participants();
    const float kPi = 3.14159265358979323846f;

    if (force_positions_.size() != total_channels) {
        // Reseed: scatter nodes in a circle around the center rather than
        // starting from a degenerate all-at-one-point state, which would
        // make the first several frames of repulsion chaotic.
        force_positions_.assign(total_channels, ImVec2(canvas_center.x, canvas_center.y));
        force_velocities_.assign(total_channels, ImVec2(0.0f, 0.0f));
        float seed_radius = std::min(canvas_size.x, canvas_size.y) * 0.3f;
        for (size_t g = 0; g < total_channels; ++g) {
            float a = (total_channels > 0) ? (2.0f * kPi * g / total_channels) : 0.0f;
            force_positions_[g] = ImVec2(canvas_center.x + seed_radius * cosf(a),
                                          canvas_center.y + seed_radius * sinf(a));
        }
    }

    if (force_paused_) return;

    struct Edge { size_t a, b; float value; };
    std::vector<Edge> edges;
    float threshold = app_->get_connection_threshold();
    for (size_t pi = 0; pi < participants.size(); ++pi) {
        for (size_t pj = pi + 1; pj < participants.size(); ++pj) {
            auto matrix = app_->get_pairwise_matrix(participants[pi].lsl_stream_name, participants[pj].lsl_stream_name);
            for (size_t r = 0; r < matrix.size(); ++r) {
                for (size_t c = 0; c < matrix[r].size(); ++c) {
                    if (matrix[r][c] < threshold) continue;
                    edges.push_back({offsets[pi] + r, offsets[pj] + c, matrix[r][c]});
                }
            }
        }
    }

    std::vector<ImVec2> forces(total_channels, ImVec2(0.0f, 0.0f));

    // Repulsion: every node pushes every other node apart (classic
    // Fruchterman-Reingold-style O(N^2) term -- fine at N <= ~200).
    const float repulsion_k = 12000.0f;
    for (size_t a = 0; a < total_channels; ++a) {
        for (size_t b = a + 1; b < total_channels; ++b) {
            float dx = force_positions_[a].x - force_positions_[b].x;
            float dy = force_positions_[a].y - force_positions_[b].y;
            float dist2 = std::max(dx * dx + dy * dy, 1.0f);
            float dist = std::sqrt(dist2);
            float f = repulsion_k / dist2;
            float fx = f * dx / dist, fy = f * dy / dist;
            forces[a].x += fx; forces[a].y += fy;
            forces[b].x -= fx; forces[b].y -= fy;
        }
    }

    // Attraction: coherent cross-brain channel pairs pull together -- the
    // stronger the coherence, the shorter the spring's rest length, so
    // highly-synced pairs end up visibly closer than weakly-synced ones.
    const float spring_k = 0.06f;
    const float rest_length_far = 260.0f;
    const float rest_length_near = 40.0f;
    for (const auto& e : edges) {
        float dx = force_positions_[e.b].x - force_positions_[e.a].x;
        float dy = force_positions_[e.b].y - force_positions_[e.a].y;
        float dist = std::max(std::sqrt(dx * dx + dy * dy), 1.0f);
        float rest_length = rest_length_far - (rest_length_far - rest_length_near) * e.value;
        float f = (dist - rest_length) * spring_k;
        float fx = f * dx / dist, fy = f * dy / dist;
        forces[e.a].x += fx; forces[e.a].y += fy;
        forces[e.b].x -= fx; forces[e.b].y -= fy;
    }

    // Mild pull toward center so the whole layout doesn't drift off-canvas.
    const float center_k = 0.01f;
    for (size_t g = 0; g < total_channels; ++g) {
        forces[g].x += (canvas_center.x - force_positions_[g].x) * center_k;
        forces[g].y += (canvas_center.y - force_positions_[g].y) * center_k;
    }

    const float damping = 0.85f;
    const float max_speed = 40.0f;
    float min_x = canvas_center.x - canvas_size.x * 0.5f + 12.0f;
    float max_x = canvas_center.x + canvas_size.x * 0.5f - 12.0f;
    float min_y = canvas_center.y - canvas_size.y * 0.5f + 12.0f;
    float max_y = canvas_center.y + canvas_size.y * 0.5f - 12.0f;

    for (size_t g = 0; g < total_channels; ++g) {
        force_velocities_[g].x = (force_velocities_[g].x + forces[g].x * 0.02f) * damping;
        force_velocities_[g].y = (force_velocities_[g].y + forces[g].y * 0.02f) * damping;

        float speed = std::sqrt(force_velocities_[g].x * force_velocities_[g].x +
                                 force_velocities_[g].y * force_velocities_[g].y);
        if (speed > max_speed) {
            force_velocities_[g].x *= max_speed / speed;
            force_velocities_[g].y *= max_speed / speed;
        }

        force_positions_[g].x = std::clamp(force_positions_[g].x + force_velocities_[g].x, min_x, max_x);
        force_positions_[g].y = std::clamp(force_positions_[g].y + force_velocities_[g].y, min_y, max_y);
    }
}

void InterBrainConnectivityPanel::render_force_directed()
{
    namespace T = ace::ui::theme;
    const auto& participants = app_->get_participants();

    std::vector<size_t> offsets(participants.size());
    size_t total_channels = 0;
    for (size_t i = 0; i < participants.size(); ++i) {
        offsets[i] = total_channels;
        total_channels += participants[i].channel_names.size();
    }
    if (total_channels == 0) {
        ImGui::TextWrapped("Waiting for channel info from participants...");
        return;
    }

    T::section_header("All Channels, All Pairs (Force-Directed)",
        "Channels pulled together by coherence strength, pushed apart otherwise -- "
        "clusters reveal which channels/participants are syncing most.");

    ImGui::Checkbox("Pause", &force_paused_);
    ImGui::SameLine();
    if (ImGui::Button("Reset Layout")) {
        force_positions_.clear();
        force_velocities_.clear();
    }

    ImVec2 canvas_size(ImGui::GetContentRegionAvail().x, std::max(500.0f, ImGui::GetContentRegionAvail().y));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, ImVec2(origin.x + canvas_size.x, origin.y + canvas_size.y), T::kBgCanvas, 6.0f);
    ImVec2 center(origin.x + canvas_size.x * 0.5f, origin.y + canvas_size.y * 0.5f);

    step_force_simulation(offsets, total_channels, center, canvas_size);

    auto locate = [&](size_t global_index) -> std::pair<size_t, size_t> {
        for (size_t p = 0; p < participants.size(); ++p) {
            size_t n = participants[p].channel_names.size();
            if (global_index < offsets[p] + n) return {p, global_index - offsets[p]};
        }
        return {participants.size() - 1, 0};
    };

    ImVec2 mouse = ImGui::GetMousePos();
    int hovered = -1;
    float best_dist2 = 12.0f * 12.0f;
    for (size_t g = 0; g < total_channels; ++g) {
        float dx = mouse.x - force_positions_[g].x;
        float dy = mouse.y - force_positions_[g].y;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_dist2) { best_dist2 = d2; hovered = static_cast<int>(g); }
    }

    float threshold = app_->get_connection_threshold();
    for (size_t pi = 0; pi < participants.size(); ++pi) {
        for (size_t pj = pi + 1; pj < participants.size(); ++pj) {
            auto matrix = app_->get_pairwise_matrix(participants[pi].lsl_stream_name, participants[pj].lsl_stream_name);
            for (size_t r = 0; r < matrix.size(); ++r) {
                for (size_t c = 0; c < matrix[r].size(); ++c) {
                    float v = matrix[r][c];
                    if (v < threshold) continue;
                    size_t g1 = offsets[pi] + r;
                    size_t g2 = offsets[pj] + c;
                    bool involved = hovered >= 0 &&
                        (static_cast<size_t>(hovered) == g1 || static_cast<size_t>(hovered) == g2);
                    if (hovered >= 0 && !involved) continue;
                    float thickness = involved ? (2.0f + v * 5.0f) : (0.75f + v * 2.5f);
                    draw_list->AddLine(force_positions_[g1], force_positions_[g2], coherence_color(v), thickness);
                }
            }
        }
    }

    for (size_t g = 0; g < total_channels; ++g) {
        auto [p, local] = locate(g);
        (void)local;
        draw_list->AddCircleFilled(force_positions_[g], 4.0f, T::to_im_col32(T::kParticipantColors[p % 6]));
    }

    if (hovered >= 0) {
        size_t g = static_cast<size_t>(hovered);
        draw_list->AddCircleFilled(force_positions_[g], 7.0f, IM_COL32(255, 255, 255, 255));

        auto [hp, hc] = locate(g);
        float best_v = -1.0f;
        std::string best_label;
        for (size_t p = 0; p < participants.size(); ++p) {
            if (p == hp) continue;
            auto matrix = app_->get_pairwise_matrix(participants[hp].lsl_stream_name, participants[p].lsl_stream_name);
            if (hc >= matrix.size()) continue;
            for (size_t c = 0; c < matrix[hc].size(); ++c) {
                if (matrix[hc][c] > best_v) {
                    best_v = matrix[hc][c];
                    best_label = participants[p].participant_label + " " + participants[p].channel_names[c];
                }
            }
        }

        ImGui::BeginTooltip();
        ImGui::Text("%s %s", participants[hp].participant_label.c_str(), participants[hp].channel_names[hc].c_str());
        if (best_v >= 0.0f) {
            ImGui::Text("Strongest link: %s (%.3f)", best_label.c_str(), best_v);
        }
        ImGui::EndTooltip();
    }

    ImGui::Dummy(canvas_size);
}

} // namespace ui
} // namespace ace
