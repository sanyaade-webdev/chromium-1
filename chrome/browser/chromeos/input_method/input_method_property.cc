// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_property.h"

#include <sstream>

#include "base/logging.h"

namespace chromeos {
namespace input_method {

InputMethodProperty::InputMethodProperty(const std::string& in_key,
                                         const std::string& in_label,
                                         bool in_is_selection_item,
                                         bool in_is_selection_item_checked,
                                         int in_selection_item_id)
    : key(in_key),
      label(in_label),
      is_selection_item(in_is_selection_item),
      is_selection_item_checked(in_is_selection_item_checked),
      selection_item_id(in_selection_item_id) {
  DCHECK(!key.empty());
}

InputMethodProperty::InputMethodProperty()
    : is_selection_item(false),
      is_selection_item_checked(false),
      selection_item_id(kInvalidSelectionItemId) {
}

InputMethodProperty::~InputMethodProperty() {
}

std::string InputMethodProperty::ToString() const {
  std::stringstream stream;
  stream << "key=" << key
         << ", label=" << label
         << ", is_selection_item=" << is_selection_item
         << ", is_selection_item_checked=" << is_selection_item_checked
         << ", selection_item_id=" << selection_item_id;
  return stream.str();
}

}  // namespace input_method
}  // namespace chromeos
