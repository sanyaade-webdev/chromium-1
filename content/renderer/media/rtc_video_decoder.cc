// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/demuxer.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/session/phone/videoframe.h"

using media::CopyUPlane;
using media::CopyVPlane;
using media::CopyYPlane;
using media::DemuxerStream;
using media::PipelineStatusCB;
using media::kNoTimestamp;
using media::PIPELINE_OK;
using media::PipelineStatusCB;
using media::StatisticsCB;
using media::VideoDecoder;

RTCVideoDecoder::RTCVideoDecoder(MessageLoop* message_loop,
                                 const std::string& url)
    : message_loop_(message_loop),
      visible_size_(176, 144),
      url_(url),
      state_(kUnInitialized),
      got_first_frame_(false) {
}

RTCVideoDecoder::~RTCVideoDecoder() {}

void RTCVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                 const PipelineStatusCB& status_cb,
                                 const StatisticsCB& statistics_cb) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::Initialize, this,
                   make_scoped_refptr(demuxer_stream),
                   status_cb, statistics_cb));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  state_ = kNormal;
  status_cb.Run(PIPELINE_OK);

  // TODO(acolwell): Implement stats.
}

void RTCVideoDecoder::Play(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RTCVideoDecoder::Play, this, callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  callback.Run();
}

void RTCVideoDecoder::Pause(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RTCVideoDecoder::Pause,
                                       this, callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  state_ = kPaused;

  callback.Run();
}

void RTCVideoDecoder::Flush(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RTCVideoDecoder::Flush,
                                       this, callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  ReadCB read_cb;
  {
    base::AutoLock auto_lock(lock_);
    if (!read_cb_.is_null()) {
      std::swap(read_cb, read_cb_);
    }
  }

  if (!read_cb.is_null()) {
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(visible_size_.width(),
                                            visible_size_.height());
    read_cb.Run(video_frame);
  }

  VideoDecoder::Flush(callback);
}

void RTCVideoDecoder::Stop(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RTCVideoDecoder::Stop,
                                       this, callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  state_ = kStopped;

  VideoDecoder::Stop(callback);
}

void RTCVideoDecoder::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  if (MessageLoop::current() != message_loop_) {
     message_loop_->PostTask(FROM_HERE,
                             base::Bind(&RTCVideoDecoder::Seek, this,
                                        time, cb));
     return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  state_ = kNormal;
  cb.Run(PIPELINE_OK);
}

void RTCVideoDecoder::Read(const ReadCB& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::Read, this, callback));
    return;
  }
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  base::AutoLock auto_lock(lock_);
  CHECK(read_cb_.is_null());
  read_cb_ = callback;
}

const gfx::Size& RTCVideoDecoder::natural_size() {
  // TODO(vrk): Return natural size when aspect ratio support is implemented.
  return visible_size_;
}

bool RTCVideoDecoder::SetSize(int width, int height, int reserved) {
  visible_size_.SetSize(width, height);

  // TODO(vrk): Provide natural size when aspect ratio support is implemented.

  // TODO(xhwang) host() can be NULL after r128289.  Remove this check when
  // it is no longer needed.
  if (host())
    host()->SetNaturalVideoSize(visible_size_);
  return true;
}

// TODO(wjia): this function can be split into two parts so that the |lock_|
// can be removed.
// First creates media::VideoFrame, then post a task onto |message_loop_|
// to deliver that frame.
bool RTCVideoDecoder::RenderFrame(const cricket::VideoFrame* frame) {
  // Called from libjingle thread.
  DCHECK(frame);

  base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
      frame->GetTimeStamp() / talk_base::kNumNanosecsPerMillisec);

  ReadCB read_cb;
  {
    base::AutoLock auto_lock(lock_);
    if (read_cb_.is_null() || state_ != kNormal) {
      // TODO(ronghuawu): revisit TS adjustment when crbug.com/111672 is
      // resolved.
      if (got_first_frame_) {
        start_time_ += timestamp - last_frame_timestamp_;
      }
      last_frame_timestamp_ = timestamp;
      return true;
    }
    std::swap(read_cb, read_cb_);
  }

  // Rebase timestamp with zero as starting point.
  if (!got_first_frame_) {
    start_time_ = timestamp;
    got_first_frame_ = true;
  }

  // Always allocate a new frame.
  //
  // TODO(scherkus): migrate this to proper buffer recycling.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                     visible_size_.width(),
                                     visible_size_.height(),
                                     timestamp - start_time_,
                                     base::TimeDelta::FromMilliseconds(0));
  last_frame_timestamp_ = timestamp;

  // Aspect ratio unsupported; DCHECK when there are non-square pixels.
  DCHECK_EQ(frame->GetPixelWidth(), 1u);
  DCHECK_EQ(frame->GetPixelHeight(), 1u);

  int y_rows = frame->GetHeight();
  int uv_rows = frame->GetHeight() / 2;  // YV12 format.
  CopyYPlane(frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame);
  CopyUPlane(frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame);
  CopyVPlane(frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame);

  read_cb.Run(video_frame);
  return true;
}
