// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CURSOR_H_
#define UI_AURA_CURSOR_H_
#pragma once

namespace aura {

// TODO(jamescook): Once we're on C++0x we could change these constants
// to an enum and forward declare it in native_widget_types.h.

// Equivalent to a NULL HCURSOR on Windows.
const int kCursorNull = 0;

// Aura cursors mirror WebKit cursors from WebCursorInfo, but are replicated
// here so we don't introduce a WebKit dependency.
const int kCursorPointer = 1;
const int kCursorCross = 2;
const int kCursorHand = 3;
const int kCursorIBeam = 4;
const int kCursorWait = 5;
const int kCursorHelp = 6;
const int kCursorEastResize = 7;
const int kCursorNorthResize = 8;
const int kCursorNorthEastResize = 9;
const int kCursorNorthWestResize = 10;
const int kCursorSouthResize = 11;
const int kCursorSouthEastResize = 12;
const int kCursorSouthWestResize = 13;
const int kCursorWestResize = 14;
const int kCursorNorthSouthResize = 15;
const int kCursorEastWestResize = 16;
const int kCursorNorthEastSouthWestResize = 17;
const int kCursorNorthWestSouthEastResize = 18;
const int kCursorColumnResize = 19;
const int kCursorRowResize = 20;
const int kCursorMiddlePanning = 21;
const int kCursorEastPanning = 22;
const int kCursorNorthPanning = 23;
const int kCursorNorthEastPanning = 24;
const int kCursorNorthWestPanning = 25;
const int kCursorSouthPanning = 26;
const int kCursorSouthEastPanning = 27;
const int kCursorSouthWestPanning = 28;
const int kCursorWestPanning = 29;
const int kCursorMove = 30;
const int kCursorVerticalText = 31;
const int kCursorCell = 32;
const int kCursorContextMenu = 33;
const int kCursorAlias = 34;
const int kCursorProgress = 35;
const int kCursorNoDrop = 36;
const int kCursorCopy = 37;
const int kCursorNone = 38;
const int kCursorNotAllowed = 39;
const int kCursorZoomIn = 40;
const int kCursorZoomOut = 41;
const int kCursorGrab = 42;
const int kCursorGrabbing = 43;
const int kCursorCustom = 44;

}  // namespace aura

#endif  // UI_AURA_CURSOR_H_
