/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <ranges>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_carousel.h>

#include <cafe_glyphs.h>

#include "ThemePreviewPopup.h"

#include "../ImageLoader.h"
#include "../IconsFontAwesome4.h"
#include "../UI.h"

using std::cout;
using std::endl;
using namespace std::literals;

namespace ThemePreviewPopup {

    namespace {

        enum class State {
            hidden,
            queued,
            visible,
        };

        State state;

        const std::string popup_id = "ThemePreviewPopup"s;

        std::string unique_id;
        std::vector<std::string> image_list;

        bool hide_ui;

        bool overlay_button(const std::string& label,
                            float x,
                            float y,
                            const ImVec2 size) {
            using ImGui::RAII::StyleColor;
            StyleColor button         {ImGuiCol_Button,        {0.0f, 0.0f, 0.0f, 0.5f}};
            StyleColor button_hovered {ImGuiCol_ButtonHovered, {0.0f, 0.0f, 0.0f, 0.7f}};
            StyleColor button_active  {ImGuiCol_ButtonActive,  {0.0f, 0.0f, 0.0f, 0.9f}};
            ImGui::SetCursorScreenPos({x, y});
            return UI::Button(label, size);
        }

    } // namespace

    void open(const std::string& id, const std::vector<std::string>& images) {
        state = State::queued;
        hide_ui = false;
        unique_id = id;
        image_list = images;
    }

    void open(const std::string& id, const std::vector<std::filesystem::path>& images) {
        state = State::queued;
        hide_ui = false;
        unique_id = id;
        image_list.clear();
        for (auto& img : images)
            image_list.push_back(img);
    }

    void process_ui() {
        using namespace ImGui::RAII;
        if (state == State::hidden) {
            return;
        }

        if (state == State::queued) {
            UI::OpenPopup(popup_id);
            state = State::visible;
        }

        const auto &style = ImGui::GetStyle();
        auto center = ImGui::GetMainViewport()->GetCenter();
        auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        StyleVar popup_no_border{ImGuiStyleVar_WindowBorderSize, 0};
        StyleVar popup_no_padding{ImGuiStyleVar_WindowPadding, {0, 0}};
        Popup popup{popup_id, ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs};

        if (!popup) {
            // ImGui closed the popup
            UI::PlaySFXPopupClose();
            state = State::hidden;
            return;
        }

        // Push a unique ID to the ID stack, to avoid reusing the carousel state from a
        // different theme.
        ID carousel_unique_id{unique_id};

        ImVec2 page_size = viewport->Size;
        ImGuiCarouselSpecs specs{
            .snap_duration = 0.25f,
            .child_flags = ImGuiChildFlags_NavFlattened,
        };

        StyleVar carousel_no_border   {ImGuiStyleVar_ChildBorderSize, 0};
        StyleVar carousel_no_rounding {ImGuiStyleVar_ChildRounding, 0};
        if (Carousel images_carousel{"images_carousel", page_size, specs}) {

            for (auto [idx, location] : image_list | std::views::enumerate) {
                if (idx > 0)
                    ImGui::SameLine();
                auto tex = ImageLoader::get(location);
                ImGui::SetNextItemAllowOverlap();
                ImGui::Image((ImTextureID)tex, page_size);
            }

            // Bug with ImGui, I'm assuming cause they never bothered to implement
            // A Nintendo button layout :P
            if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft))
                hide_ui = !hide_ui;

            if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1))
                ImGui::CarouselJumpPageLeft();

            if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1))
                ImGui::CarouselJumpPageRight();

            if (!hide_ui) {
                const int columns = ImGui::CarouselGetColumns();
                const int page_x = ImGui::CarouselGetPageX();
                const bool is_first_page = page_x <= 0;
                const bool is_last_page = page_x >= columns - 1;

                ImVec2 arrow_button_size{120.0f, ImGui::GetFrameHeight()};
                float middle_y = viewport->Pos.y + (viewport->Size.y - arrow_button_size.y) * 0.5f;
                float padding = 30.0f;

                ImVec2 button_size{160.0f, ImGui::GetFrameHeight()};
                ImVec2 text_box_size{220.0f, ImGui::GetFrameHeight()};

                float spacing = style.ItemSpacing.x;

                float total_width = button_size.x + spacing + text_box_size.x + spacing + button_size.x;

                float start_x = viewport->Pos.x + (viewport->Size.x - total_width) * 0.5f;

                float y = viewport->Pos.y + viewport->Size.y - padding - button_size.y;

                if (overlay_button(CAFE_GLYPH_BTN_B " Close",
                                   start_x,
                                   y,
                                   button_size)) {
                    UI::CloseCurrentPopup();
                    state = State::hidden;
                }

                if (overlay_button(CAFE_GLYPH_BTN_X " Hide",
                                   start_x + button_size.x + spacing +
                                   text_box_size.x + spacing,
                                   y,
                                   button_size))
                    hide_ui = true;

                {
                    Disabled disable_left{is_first_page};
                    if (overlay_button(ICON_FA_CHEVRON_LEFT " " CAFE_GLYPH_GAMEPAD_BTN_L,
                                       viewport->Pos.x + padding,
                                       middle_y,
                                       arrow_button_size))
                        ImGui::CarouselJumpPageLeft();
                }

                {
                    Disabled disable_right{is_last_page};
                    if (overlay_button(CAFE_GLYPH_GAMEPAD_BTN_R " " ICON_FA_CHEVRON_RIGHT,
                                       viewport->Pos.x + viewport->Size.x - padding - arrow_button_size.x,
                                       middle_y,
                                       arrow_button_size))
                        ImGui::CarouselJumpPageRight();
                }
            }
            else {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    hide_ui = false;
            }

        } // images_carousel
    }
}
