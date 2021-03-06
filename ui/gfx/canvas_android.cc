// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include "base/logging.h"
#include "ui/gfx/font.h"

namespace gfx {

// static
void Canvas::SizeStringInt(const string16& text,
                           const gfx::Font& font,
                           int* width, int* height, int flags) {
  NOTIMPLEMENTED();
}

void Canvas::DrawStringInt(const string16& text,
                           const gfx::Font& font,
                           SkColor color,
                           int x, int y, int w, int h,
                           int flags) {
  NOTIMPLEMENTED();
}

}  // namespace gfx
