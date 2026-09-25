#include "mycelis_display.h"

#include "assets/lang_config.h"
#include "mycelis_assets.h"

#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t kFrameMs = 40;
constexpr float kOuterDegreesPerSecond = 12.0f;
constexpr float kInnerDegreesPerSecond = -18.0f;
constexpr float kPulsePeriodMs = 900.0f;
constexpr float kPulseAmount = 0.07f;
constexpr int32_t kBaseScale = 256;

static void HideObject(lv_obj_t* object) {
    if (object != nullptr) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}
}  // namespace

MycelisLcdDisplay::MycelisLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                                     esp_lcd_panel_handle_t panel_handle,
                                     int width,
                                     int height,
                                     int offset_x,
                                     int offset_y,
                                     bool mirror_x,
                                     bool mirror_y,
                                     bool swap_xy)
    : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y,
                    mirror_x, mirror_y, swap_xy) {
}

MycelisLcdDisplay::~MycelisLcdDisplay() {
    if (animation_timer_ != nullptr) {
        lv_timer_delete(animation_timer_);
        animation_timer_ = nullptr;
    }
}

void MycelisLcdDisplay::SetupUI() {
    // Let XiaoZhi build its normal UI first so all existing display APIs remain safe.
    SpiLcdDisplay::SetupUI();

    DisplayLockGuard lock(this);
    lv_obj_t* screen = lv_screen_active();

    // Hide the stock XiaoZhi UI. The AI, audio, Wi-Fi, touch, and state machine remain intact.
    HideObject(container_);
    HideObject(top_bar_);
    HideObject(status_bar_);
    HideObject(side_bar_);
    HideObject(bottom_bar_);
    HideObject(preview_image_);
    HideObject(emoji_box_);

    // Dark technical background behind the Mycelis artwork.
    background_ = lv_image_create(screen);
    lv_image_set_src(background_, mycelis_assets::background_png);
    lv_obj_center(background_);
    lv_obj_clear_flag(background_, LV_OBJ_FLAG_CLICKABLE);

    // Two independent orbital layers.
    outer_ = lv_image_create(screen);
    lv_image_set_src(outer_, mycelis_assets::outer_png);
    lv_obj_center(outer_);
    lv_obj_clear_flag(outer_, LV_OBJ_FLAG_CLICKABLE);

    inner_ = lv_image_create(screen);
    lv_image_set_src(inner_, mycelis_assets::inner_png);
    lv_obj_center(inner_);
    lv_obj_clear_flag(inner_, LV_OBJ_FLAG_CLICKABLE);

    // Central mushroom.
    center_ = lv_image_create(screen);
    lv_image_set_src(center_, mycelis_assets::center_png);
    lv_obj_center(center_);
    lv_obj_clear_flag(center_, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_scale(center_, kBaseScale);

    // Start the animation loop at roughly 25 FPS.
    animation_timer_ = lv_timer_create(AnimationTimerCallback, kFrameMs, this);
}

void MycelisLcdDisplay::SetStatus(const char* status) {
    if (status == nullptr) {
        return;
    }

    // The stock status text is intentionally hidden. We only use the state transition
    // to switch Mycelis into its speaking animation.
    talking_.store(std::strcmp(status, Lang::Strings::SPEAKING) == 0,
                   std::memory_order_relaxed);
}

void MycelisLcdDisplay::AnimationTimerCallback(lv_timer_t* timer) {
    auto* display = static_cast<MycelisLcdDisplay*>(lv_timer_get_user_data(timer));
    if (display == nullptr) {
        return;
    }

    display->Animate(kFrameMs);
}

void MycelisLcdDisplay::Animate(uint32_t elapsed_ms) {
    if (outer_ == nullptr || inner_ == nullptr || center_ == nullptr) {
        return;
    }

    elapsed_ms_ += elapsed_ms;

    // Opposite directions, intentionally with slightly different speeds.
    outer_rotation_ += kOuterDegreesPerSecond * static_cast<float>(elapsed_ms) / 1000.0f;
    inner_rotation_ += kInnerDegreesPerSecond * static_cast<float>(elapsed_ms) / 1000.0f;

    while (outer_rotation_ >= 360.0f) outer_rotation_ -= 360.0f;
    while (outer_rotation_ < 0.0f) outer_rotation_ += 360.0f;
    while (inner_rotation_ >= 360.0f) inner_rotation_ -= 360.0f;
    while (inner_rotation_ < 0.0f) inner_rotation_ += 360.0f;

    // LVGL rotation is expressed in 0.1 degree units.
    lv_image_set_rotation(outer_, static_cast<int32_t>(outer_rotation_ * 10.0f));
    lv_image_set_rotation(inner_, static_cast<int32_t>(inner_rotation_ * 10.0f));

    if (talking_.load(std::memory_order_relaxed)) {
        const float phase =
            static_cast<float>(elapsed_ms_ % static_cast<uint32_t>(kPulsePeriodMs)) /
            kPulsePeriodMs;
        const float wave = (std::sin(phase * 6.28318530718f) + 1.0f) * 0.5f;
        const int32_t scale =
            static_cast<int32_t>(kBaseScale * (1.0f + wave * kPulseAmount));
        lv_image_set_scale(center_, scale);
    } else {
        lv_image_set_scale(center_, kBaseScale);
    }
}
