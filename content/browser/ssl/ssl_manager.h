// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_MANAGER_H_
#define CONTENT_BROWSER_SSL_SSL_MANAGER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/ssl/ssl_policy_backend.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_errors.h"

class LoadFromMemoryCacheDetails;
class NavigationControllerImpl;
class SSLPolicy;

namespace content {
class NavigationEntryImpl;
struct ResourceRedirectDetails;
struct ResourceRequestDetails;
}

namespace net {
class SSLInfo;
}

// The SSLManager SSLManager controls the SSL UI elements in a TabContents.  It
// listens for various events that influence when these elements should or
// should not be displayed and adjusts them accordingly.
//
// There is one SSLManager per tab.
// The security state (secure/insecure) is stored in the navigation entry.
// Along with it are stored any SSL error code and the associated cert.

class SSLManager : public content::NotificationObserver {
 public:
  // Entry point for SSLCertificateErrors.  This function begins the process
  // of resolving a certificate error during an SSL connection.  SSLManager
  // will adjust the security UI and either call |CancelSSLRequest| or
  // |ContinueSSLRequest| of |delegate| with |id| as the first argument.
  //
  // Called on the IO thread.
  static void OnSSLCertificateError(SSLErrorHandler::Delegate* delegate,
                                    const content::GlobalRequestID& id,
                                    ResourceType::Type resource_type,
                                    const GURL& url,
                                    int render_process_id,
                                    int render_view_id,
                                    const net::SSLInfo& ssl_info,
                                    bool fatal);

  // Called when SSL state for a host or tab changes.  Broadcasts the
  // SSL_INTERNAL_STATE_CHANGED notification.
  static void NotifySSLInternalStateChanged(
      NavigationControllerImpl* controller);

  // Construct an SSLManager for the specified tab.
  // If |delegate| is NULL, SSLPolicy::GetDefaultPolicy() is used.
  explicit SSLManager(NavigationControllerImpl* controller);
  virtual ~SSLManager();

  SSLPolicy* policy() { return policy_.get(); }
  SSLPolicyBackend* backend() { return &backend_; }

  // The navigation controller associated with this SSLManager.  The
  // NavigationController is guaranteed to outlive the SSLManager.
  NavigationControllerImpl* controller() { return controller_; }

  // This entry point is called directly (instead of via the notification
  // service) because we need more precise control of the order in which folks
  // are notified of this event.
  void DidCommitProvisionalLoad(const content::NotificationDetails& details);

  // Insecure content entry point.
  void DidRunInsecureContent(const std::string& security_origin);

  // Entry point for navigation.  This function begins the process of updating
  // the security UI when the main frame navigates to a new URL.
  //
  // Called on the UI thread.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Entry points for notifications to which we subscribe. Note that
  // DidCommitProvisionalLoad uses the abstract NotificationDetails type since
  // the type we need is in NavigationController which would create a circular
  // header file dependency.
  void DidLoadFromMemoryCache(LoadFromMemoryCacheDetails* details);
  void DidStartResourceResponse(content::ResourceRequestDetails* details);
  void DidReceiveResourceRedirect(content::ResourceRedirectDetails* details);
  void DidChangeSSLInternalState();

  // Update the NavigationEntry with our current state.
  void UpdateEntry(content::NavigationEntryImpl* entry);

  // The backend for the SSLPolicy to actuate its decisions.
  SSLPolicyBackend backend_;

  // The SSLPolicy instance for this manager.
  scoped_ptr<SSLPolicy> policy_;

  // The NavigationController that owns this SSLManager.  We are responsible
  // for the security UI of this tab.
  NavigationControllerImpl* controller_;

  // Handles registering notifications with the NotificationService.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLManager);
};

#endif  // CONTENT_BROWSER_SSL_SSL_MANAGER_H_
