// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/test_autofill_external_delegate.h"

TestAutofillExternalDelegate::TestAutofillExternalDelegate(
    TabContentsWrapper* wrapper, AutofillManager* autofill_manager) :
    AutofillExternalDelegate(wrapper, autofill_manager) {}

TestAutofillExternalDelegate::~TestAutofillExternalDelegate() {}

void TestAutofillExternalDelegate::ApplyAutofillSuggestions(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids,
    int separator_index) {}

void TestAutofillExternalDelegate::OnQueryPlatformSpecific(
    int query_id,
    const webkit::forms::FormData& form,
    const webkit::forms::FormField& field,
    const gfx::Rect& bounds) {}

void TestAutofillExternalDelegate::HideAutofillPopupInternal() {}
