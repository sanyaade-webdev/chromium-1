// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/test_web_contents.h"

#include <utility>

#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/tab_contents/navigation_entry_impl.h"
#include "content/common/view_messages.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/mock_render_process_host.h"
#include "webkit/forms/password_form.h"
#include "webkit/glue/webkit_glue.h"

namespace content {

TestWebContents::TestWebContents(BrowserContext* browser_context,
                                 SiteInstance* instance)
    : TabContents(browser_context, instance, MSG_ROUTING_NONE, NULL, NULL),
      transition_cross_site(false),
      delegate_view_override_(NULL),
      expect_set_history_length_and_prune_(false),
      expect_set_history_length_and_prune_site_instance_(NULL),
      expect_set_history_length_and_prune_history_length_(0),
      expect_set_history_length_and_prune_min_page_id_(-1) {
}

TestWebContents::~TestWebContents() {
}

RenderViewHost* TestWebContents::GetPendingRenderViewHost() const {
  return render_manager_.pending_render_view_host_;
}

TestRenderViewHost* TestWebContents::pending_test_rvh() const {
  return static_cast<TestRenderViewHost*>(GetPendingRenderViewHost());
}

void TestWebContents::TestDidNavigate(RenderViewHost* render_view_host,
                                      int page_id,
                                      const GURL& url,
                                      PageTransition transition) {
  TestDidNavigateWithReferrer(render_view_host,
                              page_id,
                              url,
                              Referrer(),
                              transition);
}

void TestWebContents::TestDidNavigateWithReferrer(
    RenderViewHost* render_view_host,
    int page_id,
    const GURL& url,
    const Referrer& referrer,
    PageTransition transition) {
  ViewHostMsg_FrameNavigate_Params params;

  params.page_id = page_id;
  params.url = url;
  params.referrer = referrer;
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = false;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.password_form = webkit::forms::PasswordForm();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.was_within_same_page = false;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

  DidNavigate(render_view_host, params);
}

WebPreferences TestWebContents::TestGetWebkitPrefs() {
  return GetWebkitPrefs();
}

bool TestWebContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  // This will go to a TestRenderViewHost.
  static_cast<RenderViewHostImpl*>(
      render_view_host)->CreateRenderView(string16(), -1);
  return true;
}

WebContents* TestWebContents::Clone() {
  TabContents* tc = new TestWebContents(
      GetBrowserContext(),
      SiteInstance::Create(GetBrowserContext()));
  tc->GetControllerImpl().CopyStateFrom(controller_);
  return tc;
}

void TestWebContents::NavigateAndCommit(const GURL& url) {
  GetController().LoadURL(
      url, Referrer(), PAGE_TRANSITION_LINK, std::string());
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
      &loaded_url, GetBrowserContext(), &reverse_on_redirect);

  // LoadURL created a navigation entry, now simulate the RenderView sending
  // a notification that it actually navigated.
  CommitPendingNavigation();
}

void TestWebContents::CommitPendingNavigation() {
  // If we are doing a cross-site navigation, this simulates the current RVH
  // notifying that it has unloaded so the pending RVH is resumed and can
  // navigate.
  ProceedWithCrossSiteNavigation();
  RenderViewHost* old_rvh = render_manager_.current_host();
  TestRenderViewHost* rvh =
      static_cast<TestRenderViewHost*>(GetPendingRenderViewHost());
  if (!rvh)
    rvh = static_cast<TestRenderViewHost*>(old_rvh);

  const NavigationEntry* entry = GetController().GetPendingEntry();
  DCHECK(entry);
  int page_id = entry->GetPageID();
  if (page_id == -1) {
    // It's a new navigation, assign a never-seen page id to it.
    page_id = GetMaxPageIDForSiteInstance(rvh->GetSiteInstance()) + 1;
  }
  rvh->SendNavigate(page_id, entry->GetURL());

  // Simulate the SwapOut_ACK that fires if you commit a cross-site navigation
  // without making any network requests.
  if (old_rvh != rvh)
    static_cast<RenderViewHostImpl*>(old_rvh)->OnSwapOutACK();
}

int TestWebContents::GetNumberOfFocusCalls() {
  NOTREACHED();
  return 0;
}

void TestWebContents::ProceedWithCrossSiteNavigation() {
  if (!GetPendingRenderViewHost())
    return;
  TestRenderViewHost* rvh = static_cast<TestRenderViewHost*>(
      render_manager_.current_host());
  rvh->SendShouldCloseACK(true);
}

RenderViewHostDelegate::View* TestWebContents::GetViewDelegate() {
  if (delegate_view_override_)
    return delegate_view_override_;
  return TabContents::GetViewDelegate();
}

void TestWebContents::ExpectSetHistoryLengthAndPrune(
    const SiteInstance* site_instance,
    int history_length,
    int32 min_page_id) {
  expect_set_history_length_and_prune_ = true;
  expect_set_history_length_and_prune_site_instance_ =
      static_cast<const SiteInstanceImpl*>(site_instance);
  expect_set_history_length_and_prune_history_length_ = history_length;
  expect_set_history_length_and_prune_min_page_id_ = min_page_id;
}

void TestWebContents::SetHistoryLengthAndPrune(
    const SiteInstance* site_instance, int history_length,
    int32 min_page_id) {
  EXPECT_TRUE(expect_set_history_length_and_prune_);
  expect_set_history_length_and_prune_ = false;
  EXPECT_EQ(expect_set_history_length_and_prune_site_instance_, site_instance);
  EXPECT_EQ(expect_set_history_length_and_prune_history_length_,
            history_length);
  EXPECT_EQ(expect_set_history_length_and_prune_min_page_id_, min_page_id);
}

}  // namespace content
