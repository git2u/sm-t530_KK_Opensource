// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_BROWSERTEST_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_BROWSERTEST_H_

#include "base/command_line.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"

namespace {
  const char kNonSigninURL[] = "www.google.com";
}

class SigninBrowserTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    https_server_.reset(new net::SpawnedTestServer(
        net::SpawnedTestServer::TYPE_HTTPS,
        net::SpawnedTestServer::kLocalhost,
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))));
    ASSERT_TRUE(https_server_->Start());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + https_server_->host_port_pair().ToString() +
            ",EXCLUDE localhost");
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  virtual void SetUp() OVERRIDE {
    factory_.reset(new net::URLFetcherImplFactory());
    fake_factory_.reset(new net::FakeURLFetcherFactory(factory_.get()));
    fake_factory_->SetFakeResponse(
        GaiaUrls::GetInstance()->service_login_url(),
        std::string(),
        true);
    fake_factory_->SetFakeResponse(kNonSigninURL, std::string(), true);
    // Yield control back to the InProcessBrowserTest framework.
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    if (fake_factory_.get()) {
      fake_factory_->ClearFakeResponses();
      fake_factory_.reset();
    }

    // Cancel any outstanding URL fetches and destroy the URLFetcherImplFactory
    // we created.
    net::URLFetcher::CancelAll();
    factory_.reset();
    InProcessBrowserTest::TearDown();
  }

 private:
  // Fake URLFetcher factory used to mock out GAIA signin.
  scoped_ptr<net::FakeURLFetcherFactory> fake_factory_;

  // The URLFetcherImplFactory instance used to instantiate |fake_factory_|.
  scoped_ptr<net::URLFetcherImplFactory> factory_;

  scoped_ptr<net::SpawnedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(SigninBrowserTest, ProcessIsolation) {
  // If the one-click-signin feature is not enabled (e.g Chrome OS), we
  // never grant signin privileges to any renderer processes.
  const bool kOneClickSigninEnabled = SyncPromoUI::UseWebBasedSigninFlow();

  SigninManager* signin = SigninManagerFactory::GetForProfile(
      browser()->profile());
  EXPECT_FALSE(signin->HasSigninProcess());

  ui_test_utils::NavigateToURL(browser(), SyncPromoUI::GetSyncPromoURL(
      GURL(), SyncPromoUI::SOURCE_NTP_LINK, true));
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  // Navigating away should change the process.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_FALSE(signin->HasSigninProcess());

  ui_test_utils::NavigateToURL(browser(), SyncPromoUI::GetSyncPromoURL(
      GURL(), SyncPromoUI::SOURCE_NTP_LINK, true));
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int active_tab_process_id =
      active_tab->GetRenderProcessHost()->GetID();
  EXPECT_EQ(kOneClickSigninEnabled,
            signin->IsSigninProcess(active_tab_process_id));
  EXPECT_EQ(0, active_tab->GetRenderViewHost()->GetEnabledBindings());

  // Entry points to signin request "SINGLETON_TAB" mode, so a new request
  // shouldn't change anything.
  chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
      browser(),
      GURL(SyncPromoUI::GetSyncPromoURL(GURL(),
                                        SyncPromoUI::SOURCE_NTP_LINK,
                                        false))));
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser(), params);
  EXPECT_EQ(active_tab, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(kOneClickSigninEnabled,
            signin->IsSigninProcess(active_tab_process_id));

  // Navigating away should change the process.
  ui_test_utils::NavigateToURL(browser(), GURL(kNonSigninURL));
  EXPECT_FALSE(signin->IsSigninProcess(
      active_tab->GetRenderProcessHost()->GetID()));
}

IN_PROC_BROWSER_TEST_F(SigninBrowserTest, NotTrustedAfterRedirect) {
  const bool kOneClickSigninEnabled = SyncPromoUI::UseWebBasedSigninFlow();

  SigninManager* signin = SigninManagerFactory::GetForProfile(
      browser()->profile());
  EXPECT_FALSE(signin->HasSigninProcess());

  GURL url = SyncPromoUI::GetSyncPromoURL(
      GURL(), SyncPromoUI::SOURCE_NTP_LINK, true);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  // Navigating in a different tab should not affect the sign-in process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kNonSigninURL), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  // Navigating away should clear the sign-in process.
  GURL redirect_url("https://accounts.google.com/server-redirect?"
      "https://foo.com?service=chromiumsync");
  ui_test_utils::NavigateToURL(browser(), redirect_url);
  EXPECT_FALSE(signin->HasSigninProcess());
}

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_BROWSERTEST_H_
