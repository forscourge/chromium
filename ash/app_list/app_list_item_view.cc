// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_view.h"

#include "ash/app_list/app_list_item_model.h"
#include "ash/app_list/drop_shadow_label.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

const int kIconTitleSpacing = 5;

const SkColor kTitleColor = SK_ColorWHITE;
const SkColor kHoverColor = SkColorSetARGB(0x33, 0xFF, 0xFF, 0xFF); // 0.2 white

gfx::Font GetTitleFont() {
  static gfx::Font* font = NULL;
  if (!font) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont).DeriveFont(
        1, gfx::Font::BOLD));
  }
  return *font;
}

// An image view that is not interactive.
class StaticImageView : public views::ImageView {
 public:
  StaticImageView() : ImageView() {
  }

 private:
  // views::View overrides:
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(StaticImageView);
};

}  // namespace

// static
const char AppListItemView::kViewClassName[] = "ash/app_list/AppListItemView";

AppListItemView::AppListItemView(AppListItemModel* model,
                                 views::ButtonListener* listener)
    : CustomButton(listener),
      model_(model),
      icon_(new StaticImageView),
      title_(new DropShadowLabel) {
  title_->SetFont(GetTitleFont());
  title_->SetBackgroundColor(0);
  title_->SetEnabledColor(kTitleColor);
  title_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  AddChildView(icon_);
  AddChildView(title_);

  ItemIconChanged();
  ItemTitleChanged();
  model_->AddObserver(this);
}

AppListItemView::~AppListItemView() {
  model_->RemoveObserver(this);
}

void AppListItemView::ItemIconChanged() {
  icon_->SetImage(model_->icon());
}

void AppListItemView::ItemTitleChanged() {
  title_->SetText(UTF8ToUTF16(model_->title()));
}

std::string AppListItemView::GetClassName() const {
  return kViewClassName;
}

gfx::Size AppListItemView::GetPreferredSize() {
  gfx::Size icon_size = icon_->GetPreferredSize();
  gfx::Size title_size = title_->GetPreferredSize();

  return gfx::Size(icon_size.width() + kIconTitleSpacing + title_size.width(),
                   std::max(icon_size.height(), title_size.height()));
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  int preferred_icon_size = rect.height() - 2 * kPadding;
  gfx::Size icon_size(preferred_icon_size, preferred_icon_size);
  icon_->SetImageSize(icon_size);
  icon_->SetBounds(rect.x() + kPadding, rect.y(),
                   icon_size.width(), rect.height());

  title_->SetBounds(
      icon_->bounds().right() + kIconTitleSpacing,
      rect.y(),
      rect.right() - kPadding - icon_->bounds().right() - kIconTitleSpacing,
      rect.height());
}

void AppListItemView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  if (hover_animation_->is_animating()) {
    int alpha = SkColorGetA(kHoverColor) * hover_animation_->GetCurrentValue();
    canvas->FillRect(rect, SkColorSetA(kHoverColor, alpha));
  } else if (state() == BS_HOT) {
    canvas->FillRect(rect, kHoverColor);
  }
}

}  // namespace ash
