// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shelf_layout_manager.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/auto_reset.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

// Delay before showing/hiding the launcher after the mouse enters the launcher.
const int kAutoHideDelayMS = 300;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

// static
const int ShelfLayoutManager::kWorkspaceAreaBottomInset = 2;

// static
const int ShelfLayoutManager::kAutoHideHeight = 2;

// Notifies ShelfLayoutManager any time the mouse moves.
class ShelfLayoutManager::AutoHideEventFilter : public aura::EventFilter {
 public:
  explicit AutoHideEventFilter(ShelfLayoutManager* shelf);
  virtual ~AutoHideEventFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(AutoHideEventFilter);
};

ShelfLayoutManager::AutoHideEventFilter::AutoHideEventFilter(
    ShelfLayoutManager* shelf)
    : shelf_(shelf) {
  Shell::GetInstance()->AddRootWindowEventFilter(this);
}

ShelfLayoutManager::AutoHideEventFilter::~AutoHideEventFilter() {
  Shell::GetInstance()->RemoveRootWindowEventFilter(this);
}

bool ShelfLayoutManager::AutoHideEventFilter::PreHandleKeyEvent(
    aura::Window* target,
    aura::KeyEvent* event) {
  return false;  // Always let the event propagate.
}

bool ShelfLayoutManager::AutoHideEventFilter::PreHandleMouseEvent(
    aura::Window* target,
    aura::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_MOVED)
    shelf_->UpdateAutoHideState();
  return false;  // Not handled.
}

ui::TouchStatus ShelfLayoutManager::AutoHideEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;  // Not handled.
}

ui::GestureStatus
ShelfLayoutManager::AutoHideEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;  // Not handled.
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, public:

ShelfLayoutManager::ShelfLayoutManager(views::Widget* status)
    : in_layout_(false),
      shelf_height_(status->GetWindowScreenBounds().height()),
      launcher_(NULL),
      status_(status),
      window_overlaps_shelf_(false) {
}

ShelfLayoutManager::~ShelfLayoutManager() {
}

gfx::Rect ShelfLayoutManager::GetMaximizedWindowBounds(
    aura::Window* window) const {
  // TODO: needs to be multi-mon aware.
  gfx::Rect bounds(gfx::Screen::GetMonitorAreaNearestWindow(window));
  bounds.set_height(bounds.height() - kAutoHideHeight);
  return bounds;
}

gfx::Rect ShelfLayoutManager::GetUnmaximizedWorkAreaBounds(
    aura::Window* window) const {
  // TODO: needs to be multi-mon aware.
  gfx::Rect bounds(gfx::Screen::GetMonitorAreaNearestWindow(window));
  bounds.set_height(bounds.height() - shelf_height_ -
                    kWorkspaceAreaBottomInset);
  return bounds;
}

void ShelfLayoutManager::SetLauncher(Launcher* launcher) {
  if (launcher == launcher_)
    return;

  launcher_ = launcher;
  shelf_height_ =
      std::max(status_->GetWindowScreenBounds().height(),
               launcher_widget()->GetWindowScreenBounds().height());
  LayoutShelf();
}

void ShelfLayoutManager::LayoutShelf() {
  AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  StopAnimating();
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  if (launcher_widget()) {
    GetLayer(launcher_widget())->SetOpacity(target_bounds.opacity);
    launcher_widget()->SetBounds(target_bounds.launcher_bounds);
    launcher_->SetStatusWidth(
        target_bounds.status_bounds.width());
  }
  GetLayer(status_)->SetOpacity(target_bounds.opacity);
  status_->SetBounds(target_bounds.status_bounds);
  Shell::GetInstance()->SetMonitorWorkAreaInsets(
      Shell::GetRootWindow(),
      target_bounds.work_area_insets);
}

void ShelfLayoutManager::SetState(VisibilityState visibility_state) {
  State state;
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);

  if (state_.Equals(state))
    return;  // Nothing changed.

  if (state.visibility_state == AUTO_HIDE) {
    // When state is AUTO_HIDE we need to track when the mouse is over the
    // launcher to unhide the shelf. AutoHideEventFilter does that for us.
    if (!event_filter_.get())
      event_filter_.reset(new AutoHideEventFilter(this));
  } else {
    event_filter_.reset(NULL);
  }

  auto_hide_timer_.Stop();

  // Animating the background when transitioning from auto-hide & hidden to
  // visibile is janking. Update the background immediately in this case.
  internal::BackgroundAnimator::ChangeType change_type =
      (state_.visibility_state == AUTO_HIDE &&
       state_.auto_hide_state == AUTO_HIDE_HIDDEN &&
       state.visibility_state == VISIBLE) ?
      internal::BackgroundAnimator::CHANGE_IMMEDIATE :
      internal::BackgroundAnimator::CHANGE_ANIMATE;
  StopAnimating();
  state_ = state;
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  if (launcher_widget()) {
    ui::ScopedLayerAnimationSettings launcher_animation_setter(
        GetLayer(launcher_widget())->GetAnimator());
    launcher_animation_setter.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(130));
    launcher_animation_setter.SetTweenType(ui::Tween::EASE_OUT);
    GetLayer(launcher_widget())->SetBounds(target_bounds.launcher_bounds);
    GetLayer(launcher_widget())->SetOpacity(target_bounds.opacity);
  }
  ui::ScopedLayerAnimationSettings status_animation_setter(
      GetLayer(status_)->GetAnimator());
  status_animation_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(130));
  status_animation_setter.SetTweenType(ui::Tween::EASE_OUT);
  GetLayer(status_)->SetBounds(target_bounds.status_bounds);
  GetLayer(status_)->SetOpacity(target_bounds.opacity);
  Shell::GetInstance()->SetMonitorWorkAreaInsets(
      Shell::GetRootWindow(),
      target_bounds.work_area_insets);
  UpdateShelfBackground(change_type);
}

void ShelfLayoutManager::UpdateAutoHideState() {
  if (CalculateAutoHideState(state_.visibility_state) !=
      state_.auto_hide_state) {
    // Don't change state immediately. Instead delay for a bit.
    if (!auto_hide_timer_.IsRunning()) {
      auto_hide_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAutoHideDelayMS),
          this, &ShelfLayoutManager::UpdateAutoHideStateNow);
    }
  } else {
    auto_hide_timer_.Stop();
  }
}

void ShelfLayoutManager::SetWindowOverlapsShelf(bool value) {
  window_overlaps_shelf_ = value;
  UpdateShelfBackground(internal::BackgroundAnimator::CHANGE_ANIMATE);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, aura::LayoutManager implementation:

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
}

void ShelfLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void ShelfLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void ShelfLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {
}

void ShelfLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
  if (!in_layout_)
    LayoutShelf();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

void ShelfLayoutManager::StopAnimating() {
  if (launcher_widget())
    GetLayer(launcher_widget())->GetAnimator()->StopAnimating();
  GetLayer(status_)->GetAnimator()->StopAnimating();
}

void ShelfLayoutManager::CalculateTargetBounds(
    const State& state,
    TargetBounds* target_bounds) const {
  const gfx::Rect& available_bounds(
      status_->GetNativeView()->GetRootWindow()->bounds());
  int y = available_bounds.bottom();
  int shelf_height = 0;
  int work_area_delta = 0;
  if (state.visibility_state == VISIBLE ||
      (state.visibility_state == AUTO_HIDE &&
       state.auto_hide_state == AUTO_HIDE_SHOWN)) {
    shelf_height = shelf_height_;
    work_area_delta = kWorkspaceAreaBottomInset;
  } else if (state.visibility_state == AUTO_HIDE &&
             state.auto_hide_state == AUTO_HIDE_HIDDEN) {
    shelf_height = kAutoHideHeight;
  }
  y -= shelf_height;
  gfx::Rect status_bounds(status_->GetWindowScreenBounds());
  // The status widget should extend to the bottom and right edges.
  target_bounds->status_bounds = gfx::Rect(
      available_bounds.right() - status_bounds.width(),
      y + shelf_height_ - status_bounds.height(),
      status_bounds.width(), status_bounds.height());
  if (launcher_widget()) {
    gfx::Rect launcher_bounds(launcher_widget()->GetWindowScreenBounds());
    target_bounds->launcher_bounds = gfx::Rect(
        available_bounds.x(),
        y + (shelf_height_ - launcher_bounds.height()) / 2,
        available_bounds.width(),
        launcher_bounds.height());
  }
  target_bounds->opacity =
      (state.visibility_state == VISIBLE ||
       state.visibility_state == AUTO_HIDE) ? 1.0f : 0.0f;
  target_bounds->work_area_insets =
      gfx::Insets(0, 0, shelf_height + work_area_delta, 0);
}

void ShelfLayoutManager::UpdateShelfBackground(
    BackgroundAnimator::ChangeType type) {
  bool launcher_paints = GetLauncherPaintsBackground();
  if (launcher_)
    launcher_->SetPaintsBackground(launcher_paints, type);
  // SystemTray normally draws a background, but we don't want it to draw a
  // background when the launcher does.
  if (Shell::GetInstance()->tray())
    Shell::GetInstance()->tray()->SetPaintsBackground(!launcher_paints, type);
}

bool ShelfLayoutManager::GetLauncherPaintsBackground() const {
  return window_overlaps_shelf_ || state_.visibility_state == AUTO_HIDE;
}

void ShelfLayoutManager::UpdateAutoHideStateNow() {
  SetState(state_.visibility_state);
}

ShelfLayoutManager::AutoHideState ShelfLayoutManager::CalculateAutoHideState(
    VisibilityState visibility_state) const {
  if (visibility_state != AUTO_HIDE || !launcher_widget())
    return AUTO_HIDE_HIDDEN;

  Shell* shell = Shell::GetInstance();
  if (shell->tray() && shell->tray()->showing_bubble())
    return AUTO_HIDE_SHOWN;  // Always show if a bubble is open from the shelf.

  aura::RootWindow* root = launcher_widget()->GetNativeView()->GetRootWindow();
  bool mouse_over_launcher =
      launcher_widget()->GetWindowScreenBounds().Contains(
          root->last_mouse_location());
  return mouse_over_launcher ? AUTO_HIDE_SHOWN : AUTO_HIDE_HIDDEN;
}

}  // namespace internal
}  // namespace ash
