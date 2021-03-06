// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_popup_view.h"

#include "base/logging.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

AutofillPopupView::AutofillPopupView(
    content::WebContents* web_contents,
    AutofillExternalDelegate* external_delegate)
    : external_delegate_(external_delegate),
      selected_line_(-1) {
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_HIDDEN,
                 content::Source<content::WebContents>(web_contents));
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents->GetController())));
}

AutofillPopupView::~AutofillPopupView() {}

void AutofillPopupView::Hide() {
  HideInternal();

  external_delegate_->ClearPreviewedForm();
}

void AutofillPopupView::Show(const std::vector<string16>& autofill_values,
                             const std::vector<string16>& autofill_labels,
                             const std::vector<string16>& autofill_icons,
                             const std::vector<int>& autofill_unique_ids,
                             int separator_index) {
  autofill_values_ = autofill_values;
  autofill_labels_ = autofill_labels;
  autofill_icons_ = autofill_icons;
  autofill_unique_ids_ = autofill_unique_ids;

  separator_index_ = separator_index;

  ShowInternal();
}

void AutofillPopupView::SetSelectedLine(int selected_line) {
  if (selected_line_ == selected_line)
    return;

  if (selected_line_ != -1)
    InvalidateRow(selected_line_);

  if (selected_line != -1)
    InvalidateRow(selected_line);

  selected_line_ = selected_line;

  if (selected_line_ != -1) {
    external_delegate_->SelectAutofillSuggestionAtIndex(
        autofill_unique_ids_[selected_line_],
        selected_line);
  }
}

void AutofillPopupView::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_HIDDEN
      || type == content::NOTIFICATION_NAV_ENTRY_COMMITTED)
    Hide();
}
