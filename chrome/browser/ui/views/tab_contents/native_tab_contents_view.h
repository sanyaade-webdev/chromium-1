// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/gfx/native_widget_types.h"

struct WebDropData;
namespace content {
class RenderWidgetHost;
class RenderWidgetHostView;
}
namespace gfx {
class Point;
}
namespace internal {
class NativeTabContentsViewDelegate;
}
namespace views {
class NativeWidget;
}

class NativeTabContentsView {
 public:
  virtual ~NativeTabContentsView() {}

  static NativeTabContentsView* CreateNativeTabContentsView(
      internal::NativeTabContentsViewDelegate* delegate);

  virtual void InitNativeTabContentsView() = 0;

  virtual content::RenderWidgetHostView* CreateRenderWidgetHostView(
      content::RenderWidgetHost* render_widget_host) = 0;

  virtual gfx::NativeWindow GetTopLevelNativeWindow() const = 0;

  virtual void SetPageTitle(const string16& title) = 0;

  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) = 0;
  virtual void CancelDrag() = 0;
  virtual bool IsDoingDrag() const = 0;
  virtual void SetDragCursor(WebKit::WebDragOperation operation) = 0;

  virtual views::NativeWidget* AsNativeWidget() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_H_
