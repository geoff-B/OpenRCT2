/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Viewport.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Game.h>
#include <openrct2/actions/LargeSceneryRemoveAction.h>
#include <openrct2/actions/SignSetNameAction.h>
#include <openrct2/actions/SignSetStyleAction.h>
#include <openrct2/actions/WallRemoveAction.h>
#include <openrct2/config/Config.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/localisation/StringIds.h>
#include <openrct2/sprites.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/LargeScenery.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/Wall.h>

static constexpr const rct_string_id WINDOW_TITLE = STR_SIGN;
static constexpr const int32_t WW = 113;
static constexpr const int32_t WH = 96;

// clang-format off
enum WINDOW_SIGN_WIDGET_IDX {
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_VIEWPORT,
    WIDX_SIGN_TEXT,
    WIDX_SIGN_DEMOLISH,
    WIDX_MAIN_COLOUR,
    WIDX_TEXT_COLOUR
};

// 0x9AEE00
static rct_widget window_sign_widgets[] = {
    WINDOW_SHIM(WINDOW_TITLE, WW, WH),
    MakeWidget({      3,      17}, {85, 60}, WindowWidgetType::Viewport,  WindowColour::Secondary, STR_VIEWPORT                                 ), // Viewport
    MakeWidget({WW - 25,      19}, {24, 24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, SPR_RENAME,   STR_CHANGE_SIGN_TEXT_TIP       ), // change sign button
    MakeWidget({WW - 25,      67}, {24, 24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, SPR_DEMOLISH, STR_DEMOLISH_SIGN_TIP          ), // demolish button
    MakeWidget({      5, WH - 16}, {12, 12}, WindowWidgetType::ColourBtn, WindowColour::Secondary, 0xFFFFFFFF,   STR_SELECT_MAIN_SIGN_COLOUR_TIP), // Main colour
    MakeWidget({     17, WH - 16}, {12, 12}, WindowWidgetType::ColourBtn, WindowColour::Secondary, 0xFFFFFFFF,   STR_SELECT_TEXT_COLOUR_TIP     ), // Text colour
    { WIDGETS_END },
};

// clang-format on

class SignWindow final : public Window
{
private:
    bool _isSmall = false;
    Banner* _banner = nullptr;
    TileElement* _tileElement = nullptr;

    void ShowTextInput()
    {
        if (_banner != nullptr)
        {
            auto bannerText = _banner->GetText();
            window_text_input_raw_open(this, WIDX_SIGN_TEXT, STR_SIGN_TEXT_TITLE, STR_SIGN_TEXT_PROMPT, bannerText.c_str(), 32);
        }
    }

public:
    void OnOpen() override
    {
        widgets = window_sign_widgets;
        enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_SIGN_TEXT) | (1 << WIDX_SIGN_DEMOLISH) | (1 << WIDX_MAIN_COLOUR)
            | (1 << WIDX_TEXT_COLOUR);

        WindowInitScrollWidgets(this);
    }

    /*
     * Initializes the window and sets it's number and if it's small
     * @return true if successfull
     */
    bool Initialize(rct_windownumber windowNumber, const bool isSmall)
    {
        number = windowNumber;
        _isSmall = isSmall;

        _banner = GetBanner(number);
        if (_banner == nullptr)
            return false;

        auto signViewPosition = _banner->position.ToCoordsXY().ToTileCentre();
        _tileElement = banner_get_tile_element(number);
        if (_tileElement == nullptr)
            return false;

        int32_t viewZ = _tileElement->GetBaseZ();
        frame_no = viewZ;

        if (_isSmall)
        {
            list_information_type = _tileElement->AsWall()->GetPrimaryColour();
            var_492 = _tileElement->AsWall()->GetSecondaryColour();
            SceneryEntry = _tileElement->AsWall()->GetEntryIndex();
        }
        else
        {
            list_information_type = _tileElement->AsLargeScenery()->GetPrimaryColour();
            var_492 = _tileElement->AsLargeScenery()->GetSecondaryColour();
            SceneryEntry = _tileElement->AsLargeScenery()->GetEntryIndex();
        }

        // Create viewport
        rct_widget& viewportWidget = window_sign_widgets[WIDX_VIEWPORT];

        viewport_create(
            this, windowPos + ScreenCoordsXY{ viewportWidget.left + 1, viewportWidget.top + 1 }, viewportWidget.width() - 1,
            viewportWidget.height() - 1, 0, { signViewPosition, viewZ }, 0, SPRITE_INDEX_NULL);

        viewport->flags = gConfigGeneral.always_show_gridlines ? VIEWPORT_FLAG_GRIDLINES : 0;
        Invalidate();

        return true;
    }

    void OnMouseUp(rct_widgetindex widgetIndex) override
    {
        switch (widgetIndex)
        {
            case WIDX_CLOSE:
                Close();
                break;
            case WIDX_SIGN_DEMOLISH:
            {
                auto bannerCoords = _banner->position.ToCoordsXY();

                if (_isSmall)
                {
                    CoordsXYZD wallLocation = { bannerCoords, _tileElement->GetBaseZ(), _tileElement->GetDirection() };
                    auto wallRemoveAction = WallRemoveAction(wallLocation);
                    GameActions::Execute(&wallRemoveAction);
                }
                else
                {
                    auto sceneryRemoveAction = LargeSceneryRemoveAction(
                        { bannerCoords, _tileElement->GetBaseZ(), _tileElement->GetDirection() },
                        _tileElement->AsLargeScenery()->GetSequenceIndex());
                    GameActions::Execute(&sceneryRemoveAction);
                }
                break;
            }
            case WIDX_SIGN_TEXT:
                ShowTextInput();
                break;
        }
    }

    void OnMouseDown(rct_widgetindex widgetIndex) override
    {
        rct_widget* widget = &widgets[widgetIndex];
        switch (widgetIndex)
        {
            case WIDX_MAIN_COLOUR:
                WindowDropdownShowColour(this, widget, TRANSLUCENT(colours[1]), static_cast<uint8_t>(list_information_type));
                break;
            case WIDX_TEXT_COLOUR:
                WindowDropdownShowColour(this, widget, TRANSLUCENT(colours[1]), static_cast<uint8_t>(var_492));
                break;
        }
    }

    void OnDropdown(rct_widgetindex widgetIndex, int32_t dropdownIndex) override
    {
        switch (widgetIndex)
        {
            case WIDX_MAIN_COLOUR:
            {
                if (dropdownIndex == -1)
                    return;
                list_information_type = dropdownIndex;
                auto signSetStyleAction = SignSetStyleAction(number, dropdownIndex, var_492, !_isSmall);
                GameActions::Execute(&signSetStyleAction);
                break;
            }
            case WIDX_TEXT_COLOUR:
            {
                if (dropdownIndex == -1)
                    return;
                var_492 = dropdownIndex;
                auto signSetStyleAction = SignSetStyleAction(number, list_information_type, dropdownIndex, !_isSmall);
                GameActions::Execute(&signSetStyleAction);
                break;
            }
            default:
                return;
        }

        Invalidate();
    }

    void OnTextInput(rct_widgetindex widgetIndex, std::string_view text) override
    {
        if (widgetIndex == WIDX_SIGN_TEXT && !text.empty())
        {
            auto signSetNameAction = SignSetNameAction(number, std::string(text));
            GameActions::Execute(&signSetNameAction);
        }
    }

    void OnPrepareDraw() override
    {
        rct_widget* main_colour_btn = &window_sign_widgets[WIDX_MAIN_COLOUR];
        rct_widget* text_colour_btn = &window_sign_widgets[WIDX_TEXT_COLOUR];

        if (_isSmall)
        {
            rct_scenery_entry* scenery_entry = get_wall_entry(SceneryEntry);

            main_colour_btn->type = WindowWidgetType::Empty;
            text_colour_btn->type = WindowWidgetType::Empty;

            if (scenery_entry->wall.flags & WALL_SCENERY_HAS_PRIMARY_COLOUR)
            {
                main_colour_btn->type = WindowWidgetType::ColourBtn;
            }
            if (scenery_entry->wall.flags & WALL_SCENERY_HAS_SECONDARY_COLOUR)
            {
                text_colour_btn->type = WindowWidgetType::ColourBtn;
            }
        }
        else
        {
            rct_scenery_entry* scenery_entry = get_large_scenery_entry(SceneryEntry);

            main_colour_btn->type = WindowWidgetType::Empty;
            text_colour_btn->type = WindowWidgetType::Empty;

            if (scenery_entry->large_scenery.flags & LARGE_SCENERY_FLAG_HAS_PRIMARY_COLOUR)
            {
                main_colour_btn->type = WindowWidgetType::ColourBtn;
            }
            if (scenery_entry->large_scenery.flags & LARGE_SCENERY_FLAG_HAS_SECONDARY_COLOUR)
            {
                text_colour_btn->type = WindowWidgetType::ColourBtn;
            }
        }

        main_colour_btn->image = SPRITE_ID_PALETTE_COLOUR_1(list_information_type) | IMAGE_TYPE_TRANSPARENT | SPR_PALETTE_BTN;
        text_colour_btn->image = SPRITE_ID_PALETTE_COLOUR_1(var_492) | IMAGE_TYPE_TRANSPARENT | SPR_PALETTE_BTN;
    }

    void OnDraw(rct_drawpixelinfo& dpi) override
    {
        DrawWidgets(dpi);

        if (viewport != nullptr)
        {
            window_draw_viewport(&dpi, this);
        }
    }

    void OnViewportRotate() override
    {
        RemoveViewport();

        auto banner = GetBanner(number);

        auto signViewPos = CoordsXYZ{ banner->position.ToCoordsXY().ToTileCentre(), frame_no };

        // Create viewport
        rct_widget* viewportWidget = &window_sign_widgets[WIDX_VIEWPORT];
        viewport_create(
            this, windowPos + ScreenCoordsXY{ viewportWidget->left + 1, viewportWidget->top + 1 }, viewportWidget->width() - 1,
            viewportWidget->height() - 1, 0, signViewPos, 0, SPRITE_INDEX_NULL);
        if (viewport != nullptr)
            viewport->flags = gConfigGeneral.always_show_gridlines ? VIEWPORT_FLAG_GRIDLINES : 0;
        Invalidate();
    }
};

/**
 *
 *  rct2: 0x006BA305
 */
rct_window* window_sign_open(rct_windownumber number)
{
    auto* w = static_cast<SignWindow*>(window_bring_to_front_by_number(WC_BANNER, number));

    if (w != nullptr)
        return w;

    w = WindowCreate<SignWindow>(WC_BANNER, WW, WH, 0);

    if (w == nullptr)
        return nullptr;

    bool result = w->Initialize(number, false);
    if (result != true)
        return nullptr;

    return w;
}

/**
 *
 *  rct2: 0x6E5F52
 */
rct_window* window_sign_small_open(rct_windownumber number)
{
    auto* w = static_cast<SignWindow*>(window_bring_to_front_by_number(WC_BANNER, number));

    if (w != nullptr)
        return w;

    w = WindowCreate<SignWindow>(WC_BANNER, WW, WH, 0);

    if (w == nullptr)
        return nullptr;

    bool result = w->Initialize(number, true);
    if (result != true)
        return nullptr;

    return w;
}
