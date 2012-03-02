// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list/app_list_view_delegate.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/aura/app_list/app_list_model_builder.h"
#include "chrome/browser/ui/views/aura/app_list/chrome_app_list_item.h"

AppListViewDelegate::AppListViewDelegate() {
}

AppListViewDelegate::~AppListViewDelegate() {
}

void AppListViewDelegate::BuildAppListModel(const std::string& query,
                                            ash::AppListModel* model) {
  AppListModelBuilder builder(ProfileManager::GetDefaultProfile(),
                              model);
  builder.Build(query);
}

void AppListViewDelegate::OnAppListItemActivated(
    ash::AppListItemModel* item,
    int event_flags) {
  static_cast<ChromeAppListItem*>(item)->Activate(event_flags);
}
