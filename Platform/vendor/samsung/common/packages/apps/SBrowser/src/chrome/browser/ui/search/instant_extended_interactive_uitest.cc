// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_commit_type.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_overlay.h"
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// Creates a bitmap of the specified color. Caller takes ownership.
gfx::Image CreateBitmap(SkColor color) {
  SkBitmap thumbnail;
  thumbnail.setConfig(SkBitmap::kARGB_8888_Config, 4, 4);
  thumbnail.allocPixels();
  thumbnail.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(thumbnail);  // adds ref.
}

// Task used to make sure history has finished processing a request. Intended
// for use with BlockUntilHistoryProcessesPendingRequests.
class QuittingHistoryDBTask : public history::HistoryDBTask {
 public:
  QuittingHistoryDBTask() {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    MessageLoop::current()->Quit();
  }

 private:
  virtual ~QuittingHistoryDBTask() {}

  DISALLOW_COPY_AND_ASSIGN(QuittingHistoryDBTask);
};

}  // namespace

class InstantExtendedTest : public InProcessBrowserTest,
                            public InstantTestBase {
 public:
  InstantExtendedTest()
      : on_most_visited_change_calls_(0),
        most_visited_items_count_(0),
        first_most_visited_item_id_(0),
        on_native_suggestions_calls_(0),
        on_change_calls_(0),
        submit_count_(0),
        on_esc_key_press_event_calls_(0) {
  }
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    browser()->toolbar_model()->SetSupportsExtractionOfURLLikeSearchTerms(true);
  }

  std::string GetOmniboxText() {
    return UTF16ToUTF8(omnibox()->GetText());
  }

  void SendDownArrow() {
    omnibox()->model()->OnUpOrDownKeyPressed(1);
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  void SendUpArrow() {
    omnibox()->model()->OnUpOrDownKeyPressed(-1);
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  void SendEscape() {
    omnibox()->model()->OnEscapeKeyPressed();
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  bool UpdateSearchState(content::WebContents* contents) WARN_UNUSED_RESULT {
    return GetIntFromJS(contents, "onMostVisitedChangedCalls",
                        &on_most_visited_change_calls_) &&
           GetIntFromJS(contents, "mostVisitedItemsCount",
                        &most_visited_items_count_) &&
           GetIntFromJS(contents, "firstMostVisitedItemId",
                        &first_most_visited_item_id_) &&
           GetIntFromJS(contents, "onNativeSuggestionsCalls",
                        &on_native_suggestions_calls_) &&
           GetIntFromJS(contents, "onChangeCalls",
                        &on_change_calls_) &&
           GetIntFromJS(contents, "submitCount",
                        &submit_count_) &&
           GetStringFromJS(contents, "apiHandle.value",
                           &query_value_) &&
           GetIntFromJS(contents, "onEscKeyPressedCalls",
                        &on_esc_key_press_event_calls_);
  }

  TemplateURL* GetDefaultSearchProviderTemplateURL() {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    if (template_url_service)
      return template_url_service->GetDefaultSearchProvider();
    return NULL;
  }

  bool AddSearchToHistory(string16 term, int visit_count) {
    TemplateURL* template_url = GetDefaultSearchProviderTemplateURL();
    if (!template_url)
      return false;

    HistoryService* history = HistoryServiceFactory::GetForProfile(
        browser()->profile(), Profile::EXPLICIT_ACCESS);
    GURL search(template_url->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(term)));
    history->AddPageWithDetails(
        search, string16(), visit_count, visit_count,
        base::Time::Now(), false, history::SOURCE_BROWSED);
    history->SetKeywordSearchTermsForURL(
        search, template_url->id(), term);
    return true;
  }

  void BlockUntilHistoryProcessesPendingRequests() {
    HistoryService* history = HistoryServiceFactory::GetForProfile(
        browser()->profile(), Profile::EXPLICIT_ACCESS);
    DCHECK(history);
    DCHECK(MessageLoop::current());

    CancelableRequestConsumer consumer;
    history->ScheduleDBTask(new QuittingHistoryDBTask(), &consumer);
    MessageLoop::current()->Run();
  }

  int CountSearchProviderSuggestions() {
    return omnibox()->model()->autocomplete_controller()->search_provider()->
        matches().size();
  }

  int on_most_visited_change_calls_;
  int most_visited_items_count_;
  int first_most_visited_item_id_;
  int on_native_suggestions_calls_;
  int on_change_calls_;
  int submit_count_;
  int on_esc_key_press_event_calls_;
  std::string query_value_;
};

// Test class used to verify chrome-search: scheme and access policy from the
// Instant overlay.  This is a subclass of |ExtensionBrowserTest| because it
// loads a theme that provides a background image.
class InstantPolicyTest : public ExtensionBrowserTest, public InstantTestBase {
 public:
  InstantPolicyTest() {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url);
  }

  void InstallThemeSource() {
    ThemeSource* theme = new ThemeSource(profile());
    content::URLDataSource::Add(profile(), theme);
  }

  void InstallThemeAndVerify(const std::string& theme_dir,
                             const std::string& theme_name) {
    const base::FilePath theme_path = test_data_dir_.AppendASCII(theme_dir);
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
        theme_path, 1, ExtensionBrowserTest::browser()));
    const extensions::Extension* theme =
        ThemeServiceFactory::GetThemeForProfile(
            ExtensionBrowserTest::browser()->profile());
    ASSERT_NE(static_cast<extensions::Extension*>(NULL), theme);
    ASSERT_EQ(theme->name(), theme_name);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantPolicyTest);
};

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ExtendedModeIsOn) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  EXPECT_TRUE(instant()->extended_enabled_);
}

// Test that Instant is preloaded when the omnibox is focused.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxFocusLoadsInstant) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Explicitly unfocus the omnibox.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_FALSE(omnibox()->model()->has_focus());

  // Delete any existing overlay.
  instant()->overlay_.reset();
  EXPECT_FALSE(instant()->GetOverlayContents());

  // Refocus the omnibox. The InstantController should've preloaded Instant.
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(omnibox()->model()->has_focus());

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);

  // Check that the page supports Instant, but it isn't showing.
  EXPECT_TRUE(instant()->overlay_->supports_instant());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Adding a new tab shouldn't delete or recreate the overlay; otherwise,
  // what's the point of preloading?
  AddBlankTabAndShow(browser());
  EXPECT_EQ(overlay, instant()->GetOverlayContents());

  // Unfocusing and refocusing the omnibox should also preserve the overlay.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  FocusOmnibox();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_EQ(overlay, instant()->GetOverlayContents());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, InputShowsOverlay) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Typing in the omnibox should show the overlay.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  EXPECT_EQ(overlay, instant()->GetOverlayContents());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, UsesOverlayIfTabNotReady) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Open a new tab and start typing before InstantTab is properly hooked up.
  // Should use the overlay.
  ui_test_utils::NavigateToURLWithDisposition(browser(),
                                              GURL(chrome::kChromeUINewTabURL),
                                              NEW_FOREGROUND_TAB,
                                              ui_test_utils::BROWSER_TEST_NONE);
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));

  // But Instant tab should still exist.
  ASSERT_NE(static_cast<InstantTab*>(NULL), instant()->instant_tab());
  EXPECT_FALSE(instant()->UseTabForSuggestions());

  // Wait for Instant Tab support if it still hasn't finished loading.
  if (!instant()->instant_tab()->supports_instant()) {
    content::WindowedNotificationObserver instant_tab_observer(
        chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
        content::NotificationService::AllSources());
    instant_tab_observer.Wait();
  }

  // Hide the overlay. Now, we should be using Instant tab for suggestions.
  instant()->HideOverlay();
  EXPECT_TRUE(instant()->UseTabForSuggestions());
}

// Test that middle clicking on a suggestion opens the result in a new tab.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MiddleClickOnSuggestionOpensInNewTab) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Typing in the omnibox should show the overlay.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("http://www.example.com/"));

  // Create an event listener that opens the top suggestion in a new tab.
  EXPECT_TRUE(ExecuteScript(
      "var rid = getApiHandle().nativeSuggestions[0].rid;"
      "document.body.addEventListener('click', function() {"
        "chrome.embeddedSearch.navigateContentWindow(rid, 2);"
      "});"
      ));

  content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());

  // Click to trigger the event listener.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Wait for the new tab to be added.
  observer.Wait();

  // Check that the new tab URL is as expected.
  content::WebContents* new_tab_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ("http://www.example.com/", new_tab_contents->GetURL().spec());

  // Check that there are now two tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       UnfocusingOmniboxDoesNotChangeSuggestions) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Get a committed tab to work with.
  content::WebContents* instant_tab = instant()->GetOverlayContents();
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("committed"));
  browser()->window()->GetLocationBar()->AcceptInput();

  // Put focus back into the omnibox, type, and wait for some gray text.
  EXPECT_TRUE(content::ExecuteScript(instant_tab,
                                     "suggestion = 'santa claus';"));
  SetOmniboxTextAndWaitForSuggestion("santa ");
  EXPECT_EQ(ASCIIToUTF16("claus"), GetGrayText());
  EXPECT_TRUE(content::ExecuteScript(instant_tab,
      "onChangeCalls = onNativeSuggestionsCalls = 0;"));

  // Now unfocus the omnibox.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(UpdateSearchState(instant_tab));
  EXPECT_EQ(0, on_change_calls_);
  EXPECT_EQ(0, on_native_suggestions_calls_);
}

// Test that omnibox text is correctly set when overlay is committed with Enter.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxTextUponEnterCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // The page will autocomplete once we set the omnibox value.
  EXPECT_TRUE(ExecuteScript("suggestion = 'santa claus';"));

  // Set the text, and wait for suggestions to show up.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("santa"));
  EXPECT_EQ(ASCIIToUTF16("santa"), omnibox()->GetText());

  // Test that the current suggestion is correctly set.
  EXPECT_EQ(ASCIIToUTF16(" claus"), GetGrayText());

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // 'Enter' commits the query as it was typed.
  EXPECT_EQ(ASCIIToUTF16("santa"), omnibox()->GetText());

  // Suggestion should be cleared at this point.
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());
}

// Test that omnibox text is correctly set when committed with focus lost.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxTextUponFocusLostCommit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Set autocomplete text (grey text).
  EXPECT_TRUE(ExecuteScript("suggestion = 'johnny depp';"));

  // Set the text, and wait for suggestions to show up.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("johnny"));
  EXPECT_EQ(ASCIIToUTF16("johnny"), omnibox()->GetText());

  // Test that the current suggestion is correctly set.
  EXPECT_EQ(ASCIIToUTF16(" depp"), GetGrayText());

  // Commit the overlay by lost focus (e.g. clicking on the page).
  instant()->CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);

  // Omnibox text and suggestion should not be changed.
  EXPECT_EQ(ASCIIToUTF16("johnny"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(" depp"), GetGrayText());
}

// Test that omnibox text is correctly set when clicking on committed SERP.
// Disabled on Mac because omnibox focus loss is not working correctly.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       OmniboxTextUponFocusedCommittedSERP) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Do a search and commit it.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("hello k"));
  EXPECT_EQ(ASCIIToUTF16("hello k"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // With a committed results page, do a search by unfocusing the omnibox and
  // focusing the contents.
  SetOmniboxText("hello");
  // Calling handleOnChange manually to make sure it is called before the
  // Focus() call below.
  EXPECT_TRUE(content::ExecuteScript(instant()->instant_tab()->contents(),
                                     "suggestion = 'hello kitty';"
                                     "handleOnChange();"));
  instant()->instant_tab()->contents()->GetView()->Focus();

  // Omnibox text and suggestion should not be changed.
  EXPECT_EQ(ASCIIToUTF16("hello"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(" kitty"), GetGrayText());
}

// Checks that a previous Navigation suggestion is not re-used when a search
// suggestion comes in.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       NavigationSuggestionIsDiscardedUponSearchSuggestion) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Tell the page to send a URL suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'http://www.example.com';"
                            "behavior = 1;"));
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("exa"));
  EXPECT_EQ(ASCIIToUTF16("example.com"), omnibox()->GetText());

  // Now send a search suggestion and see that Navigation suggestion is no
  // longer kept.
  EXPECT_TRUE(ExecuteScript("suggestion = 'exams are great';"
                            "behavior = 2;"));
  SetOmniboxText("exam");
  // Wait for JavaScript to run handleOnChange by executing a blank script.
  EXPECT_TRUE(ExecuteScript(std::string()));

  instant()->overlay()->contents()->GetView()->Focus();
  EXPECT_EQ(ASCIIToUTF16("exam"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("s are great"), GetGrayText());

  // TODO(jered): Remove this after fixing OnBlur().
  omnibox()->RevertAll();
}

// This test simulates a search provider using the InstantExtended API to
// navigate through the suggested results and back to the original user query.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NavigateSuggestionsWithArrowKeys) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("hello"));
  EXPECT_EQ("hello", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendDownArrow();
  EXPECT_EQ("result 2", GetOmniboxText());
  SendUpArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendUpArrow();
  EXPECT_EQ("hello", GetOmniboxText());

  // Ensure that the API's value is set correctly.
  std::string result;
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "window.chrome.searchBox.value",
                              &result));
  EXPECT_EQ("hello", result);

  EXPECT_TRUE(HasUserInputInProgress());
  // TODO(beaudoin): Figure out why this fails.
  // EXPECT_FALSE(HasTemporaryText());

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ("hello", GetOmniboxText());
}

// Flaky on Linux Tests bot.  See http://crbug.com/233090.
#if defined(OS_LINUX)
#define MAYBE_NavigateToURLSuggestionHitEnterAndLookForSubmit DISABLED_NavigateToURLSuggestionHitEnterAndLookForSubmit
#else
#define MAYBE_NavigateToURLSuggestionHitEnterAndLookForSubmit NavigateToURLSuggestionHitEnterAndLookForSubmit
#endif

// This test simulates a search provider using the InstantExtended API to
// navigate through the suggested results and back to the original user query.
// If this test starts to flake, it may be that the second call to AcceptInput
// below causes instant()->instant_tab() to no longer be valid due to e.g. a
// navigation. In that case, see https://codereview.chromium.org/12895007/#msg28
// and onwards for possible alternatives.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_NavigateToURLSuggestionHitEnterAndLookForSubmit) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Do a search and commit it.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("hello k"));
  EXPECT_EQ(ASCIIToUTF16("hello k"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  SetOmniboxText("http");
  EXPECT_EQ("http", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendDownArrow();
  EXPECT_EQ("result 2", GetOmniboxText());

  // Set the next suggestion to be of type INSTANT_SUGGESTION_URL.
  EXPECT_TRUE(content::ExecuteScript(instant()->instant_tab()->contents(),
                                     "suggestionType = 1;"));
  SendDownArrow();
  EXPECT_EQ("http://www.google.com", GetOmniboxText());

  EXPECT_TRUE(HasUserInputInProgress());

  EXPECT_TRUE(UpdateSearchState(instant()->instant_tab()->contents()));
  // Note the commit count is initially 1 due to the AcceptInput() call above.
  EXPECT_EQ(1, submit_count_);

  std::string old_query_value(query_value_);

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // Make sure a submit message got sent.
  EXPECT_TRUE(UpdateSearchState(instant()->instant_tab()->contents()));
  EXPECT_EQ(2, submit_count_);
  EXPECT_EQ(old_query_value, query_value_);
}

// This test simulates a search provider using the InstantExtended API to
// navigate through the suggested results and hitting escape to get back to the
// original user query.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NavigateSuggestionsAndHitEscape) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("hello"));
  EXPECT_EQ("hello", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("result 1", GetOmniboxText());
  SendDownArrow();
  EXPECT_EQ("result 2", GetOmniboxText());
  SendEscape();
  EXPECT_EQ("hello", GetOmniboxText());

  // Ensure that the API's value is set correctly.
  std::string result;
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "window.chrome.searchBox.value",
                              &result));
  EXPECT_EQ("hello", result);

  EXPECT_TRUE(HasUserInputInProgress());
  EXPECT_FALSE(HasTemporaryText());

  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ("hello", GetOmniboxText());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PressEscapeWithBlueText) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Set blue text completion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'chimichanga.com';"
                            "behavior = 1;"));

  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("chimi"));

  EXPECT_EQ(ASCIIToUTF16("chimichanga.com"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("changa.com"), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  EXPECT_TRUE(ExecuteScript("onChangeCalls = onNativeSuggestionsCalls = 0;"));

  SendDownArrow();

  EXPECT_EQ(ASCIIToUTF16("result 1"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_SET_SUGGESTION,
      content::NotificationService::AllSources());
  SendEscape();
  observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("chimichanga.com"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("changa.com"), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_EQ(0, on_native_suggestions_calls_);
  EXPECT_EQ(0, on_change_calls_);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PressEscapeWithGrayText) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Set gray text completion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'cowabunga';"
                            "behavior = 2;"));

  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("cowa"));

  EXPECT_EQ(ASCIIToUTF16("cowa"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16("bunga"), GetGrayText());

  EXPECT_TRUE(ExecuteScript("onChangeCalls = onNativeSuggestionsCalls = 0;"));

  SendDownArrow();

  EXPECT_EQ(ASCIIToUTF16("result 1"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_SET_SUGGESTION,
      content::NotificationService::AllSources());
  SendEscape();
  observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("cowa"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetBlueText());
  EXPECT_EQ(ASCIIToUTF16("bunga"), GetGrayText());

  EXPECT_TRUE(UpdateSearchState(instant()->GetOverlayContents()));
  EXPECT_EQ(0, on_native_suggestions_calls_);
  EXPECT_EQ(0, on_change_calls_);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NTPIsPreloaded) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPIsUsedInNewTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(ntp_contents, active_tab);
  EXPECT_TRUE(chrome::IsInstantNTP(active_tab));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPIsUsedInSameTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(ntp_contents, active_tab);
  EXPECT_TRUE(chrome::IsInstantNTP(active_tab));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPForWrongProvider) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);
  GURL ntp_url = ntp_contents->GetURL();

  // Change providers.
  SetInstantURL("chrome://blank");

  // Open new tab. Preloaded NTP contents should have not been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(ntp_url, active_tab->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPRenderViewGone) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  EXPECT_FALSE(instant()->ntp()->IsLocal());

  // NTP not reloaded after being killed.
  instant()->InstantPageRenderViewGone(instant()->ntp()->contents());
  EXPECT_EQ(NULL, instant()->ntp());

  // Open new tab. Should use local NTP.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(instant()->GetLocalInstantURL(), active_tab->GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPDoesntSupportInstant) {
  // Setup Instant.
  GURL instant_url = test_server()->GetURL("files/empty.html?strk=1");
  InstantTestBase::Init(instant_url);
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // NTP contents should have fallen back to the local page.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  EXPECT_TRUE(instant()->ntp()->IsLocal());

  // Open new tab. Should use local NTP.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(instant()->GetLocalInstantURL(), active_tab->GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxHasFocusOnNewTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Explicitly unfocus the omnibox.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_FALSE(omnibox()->model()->has_focus());

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Omnibox should have focus.
  EXPECT_TRUE(omnibox()->model()->has_focus());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxEmptyOnNewTabPage) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Omnibox should be empty.
  EXPECT_TRUE(omnibox()->GetText().empty());
}

// TODO(dhollowa): Fix flakes.  http://crbug.com/179930.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_NoFaviconOnNewTabPage) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // No favicon should be shown.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(active_tab);
  EXPECT_FALSE(favicon_tab_helper->ShouldDisplayFavicon());

  // Favicon should be shown off the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  favicon_tab_helper = FaviconTabHelper::FromWebContents(active_tab);
  EXPECT_TRUE(favicon_tab_helper->ShouldDisplayFavicon());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, InputOnNTPDoesntShowOverlay) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());

  // Navigate to the NTP.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Typing in the omnibox should not show the overlay.
  SetOmniboxText("query");
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());
  EXPECT_TRUE(instant()->model()->mode().is_default());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ProcessIsolation) {
  // Prior to setup, Instant has an overlay with a failed "google.com" load in
  // it, which is rendered in the dedicated Instant renderer process.
  //
  // TODO(sreeram): Fix this up when we stop doing crazy things on init.
  InstantService* instant_service =
        InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);
#if !defined(OS_MACOSX)
  // The failed "google.com" load is deleted, which sometimes leads to the
  // process shutting down on Mac.
  EXPECT_EQ(1, instant_service->GetInstantProcessCount());
#endif

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // The registered Instant render process should still exist.
  EXPECT_EQ(1, instant_service->GetInstantProcessCount());

  // And the Instant overlay and ntp should live inside it.
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      overlay->GetRenderProcessHost()->GetID()));
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_contents->GetRenderProcessHost()->GetID()));

  // Navigating to the NTP should use the Instant render process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));

  // Navigating elsewhere should not use the Instant render process.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));
}

// Test that a search query will not be displayed for navsuggest queries.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       SearchQueryNotDisplayedForNavsuggest) {
  // Use only the local overlay.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableLocalOnlyInstantExtendedAPI);
  ASSERT_TRUE(chrome::IsLocalOnlyInstantExtendedAPIEnabled());

  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // The second argument indicates to use only the local overlay and NTP.
  instant()->SetInstantEnabled(true, true);

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Typing in the omnibox should show the overlay.
  SetOmniboxText("face");

  content::WebContents* overlay = instant()->GetOverlayContents();

  // Add a navsuggest suggestion.
  instant()->SetSuggestions(
      overlay,
      std::vector<InstantSuggestion>(
          1,
          InstantSuggestion(ASCIIToUTF16("http://facemash.com/"),
                            INSTANT_COMPLETE_NOW,
                            INSTANT_SUGGESTION_URL,
                            ASCIIToUTF16("face"))));

  while (!omnibox()->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver autocomplete_observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::NotificationService::AllSources());
    autocomplete_observer.Wait();
  }

  EXPECT_TRUE(ExecuteScript(
      "var sorted = chrome.embeddedSearch.searchBox.nativeSuggestions.sort("
          "function (a,b) {"
            "return b.rankingData.relevance - a.rankingData.relevance;"
          "});"));

  int suggestions_count = -1;
  EXPECT_TRUE(GetIntFromJS(
      overlay, "sorted.length", &suggestions_count));
  ASSERT_GT(suggestions_count, 0);

  std::string type;
  EXPECT_TRUE(
      GetStringFromJS(overlay, "sorted[0].type", &type));
  ASSERT_EQ("navsuggest", type);

  bool is_search;
  EXPECT_TRUE(GetBoolFromJS(
      overlay, "!!sorted[0].is_search", &is_search));
  EXPECT_FALSE(is_search);
}

// Verification of fix for BUG=176365.  Ensure that each Instant WebContents in
// a tab uses a new BrowsingInstance, to avoid conflicts in the
// NavigationController.
// Flaky: http://crbug.com/177516
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_UnrelatedSiteInstance) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Check that the uncommited ntp page and uncommited overlay have unrelated
  // site instances.
  // TODO(sreeram): |ntp_| is going away, so this check can be removed in the
  // future.
  content::WebContents* overlay = instant()->GetOverlayContents();
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_FALSE(overlay->GetSiteInstance()->IsRelatedSiteInstance(
      ntp_contents->GetSiteInstance()));

  // Type a query and hit enter to get a results page.  The overlay becomes the
  // active tab.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("hello"));
  EXPECT_EQ("hello", GetOmniboxText());
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* first_active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(first_active_tab, overlay);
  scoped_refptr<content::SiteInstance> first_site_instance =
      first_active_tab->GetSiteInstance();
  EXPECT_FALSE(first_site_instance->IsRelatedSiteInstance(
      ntp_contents->GetSiteInstance()));

  // Navigating elsewhere gets us off of the commited page.  The next
  // query will give us a new |overlay| which we will then commit.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));

  // Show and commit the new overlay.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("hello again"));
  EXPECT_EQ("hello again", GetOmniboxText());
  browser()->window()->GetLocationBar()->AcceptInput();
  content::WebContents* second_active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(first_active_tab, second_active_tab);
  scoped_refptr<content::SiteInstance> second_site_instance =
      second_active_tab->GetSiteInstance();
  EXPECT_NE(first_site_instance, second_site_instance);
  EXPECT_FALSE(first_site_instance->IsRelatedSiteInstance(
      second_site_instance));
}

// Tests that suggestions are sanity checked.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ValidatesSuggestions) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Do not set gray text that is not a suffix of the query.
  EXPECT_TRUE(ExecuteScript("suggestion = 'potato';"
                            "behavior = 2;"));
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  EXPECT_EQ(ASCIIToUTF16("query"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  omnibox()->RevertAll();

  // Do not set blue text that is not a valid URL completion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'this is not a url!';"
                            "behavior = 1;"));
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("this is"));
  EXPECT_EQ(ASCIIToUTF16("this is"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  omnibox()->RevertAll();

  // Do not set gray text when blue text is already set.
  // First set up some blue text completion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'www.example.com';"
                            "behavior = 1;"));
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("http://www.ex"));
  EXPECT_EQ(ASCIIToUTF16("http://www.example.com"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16("ample.com"), GetBlueText());

  // Now try to set gray text for the same query.
  EXPECT_TRUE(ExecuteScript("suggestion = 'www.example.com rocks';"
                            "behavior = 2;"));
  SetOmniboxText("http://www.ex");
  EXPECT_EQ(ASCIIToUTF16("http://www.example.com"), omnibox()->GetText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  omnibox()->RevertAll();

  // Ignore an out-of-date blue text suggestion. (Simulates a laggy
  // SetSuggestion IPC by directly calling into InstantController.)
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("http://www.example.com/"));
  instant()->SetSuggestions(
      instant()->overlay()->contents(),
      std::vector<InstantSuggestion>(
          1,
          InstantSuggestion(ASCIIToUTF16("www.exa"),
                            INSTANT_COMPLETE_NOW,
                            INSTANT_SUGGESTION_URL,
                            ASCIIToUTF16("www.exa"))));
  EXPECT_EQ(
      "http://www.example.com/",
      omnibox()->model()->result().default_match()->destination_url.spec());

  omnibox()->RevertAll();

  // TODO(samarth): uncomment after fixing crbug.com/191656.
  // Use an out-of-date blue text suggestion, if the text typed by the user is
  // contained in the suggestion.
  // ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("ex"));
  // instant()->SetSuggestions(
  //     instant()->overlay()->contents(),
  //     std::vector<InstantSuggestion>(
  //         1,
  //         InstantSuggestion(ASCIIToUTF16("www.example.com"),
  //                           INSTANT_COMPLETE_NOW,
  //                           INSTANT_SUGGESTION_URL,
  //                           ASCIIToUTF16("e"))));
  // EXPECT_EQ(
  //     "http://www.example.com/",
  //     omnibox()->model()->result().default_match()->destination_url.spec());

  // omnibox()->RevertAll();

  // When asked to suggest blue text in verbatim mode, suggest the exact
  // omnibox text rather than using the supplied suggestion text.
  EXPECT_TRUE(ExecuteScript("suggestion = 'www.example.com/q';"
                            "behavior = 1;"));
  SetOmniboxText("www.example.com/q");
  omnibox()->OnBeforePossibleChange();
  SetOmniboxText("www.example.com/");
  omnibox()->OnAfterPossibleChange();
  EXPECT_EQ(ASCIIToUTF16("www.example.com/"), omnibox()->GetText());
}

// Tests that a previous navigation suggestion is not discarded if it's not
// stale.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       NavigationSuggestionIsNotDiscarded) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Tell the page to send a URL suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'http://www.example.com';"
                            "behavior = 1;"));
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("exa"));
  EXPECT_EQ(ASCIIToUTF16("example.com"), omnibox()->GetText());
  SetOmniboxText("exam");
  EXPECT_EQ(ASCIIToUTF16("example.com"), omnibox()->GetText());

  // TODO(jered): Remove this after fixing OnBlur().
  omnibox()->RevertAll();
}

// TODO(dhollowa): Fix flakes.  http://crbug.com/179930.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_MostVisited) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_SENT_MOST_VISITED_ITEMS,
      content::NotificationService::AllSources());
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Get a handle to the NTP and the current state of the JS.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* overlay = instant()->ntp_->contents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Wait for most visited data to be ready, if necessary.
  if (on_most_visited_change_calls_ == 0) {
    observer.Wait();
    EXPECT_TRUE(UpdateSearchState(overlay));
  }

  EXPECT_EQ(1, on_most_visited_change_calls_);

  // Make sure we have at least two Most Visited Items and save that number.
  // TODO(pedrosimonetti): For now, we're relying on the fact that the Top
  // Sites will have at lease two items in it. The correct approach would be
  // adding those items to the Top Sites manually before starting the test.
  EXPECT_GT(most_visited_items_count_, 1);
  int old_most_visited_items_count = most_visited_items_count_;

  // Delete the fist Most Visited Item.
  int rid = first_most_visited_item_id_;
  std::ostringstream stream;
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ");";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have one less item in there.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count - 1);

  // Undo the deletion of the fist Most Visited Item.
  stream.str(std::string());
  stream << "newTabPageHandle.undoMostVisitedDeletion(" << rid << ");";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have the same number of items as before.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count);

  // Delete the fist Most Visited Item.
  rid = first_most_visited_item_id_;
  stream.str(std::string());
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ");";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Delete the second Most Visited Item.
  rid = first_most_visited_item_id_;
  stream.str(std::string());
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ");";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have two less items in there.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count - 2);

  // Delete the second Most Visited Item.
  stream.str(std::string());
  stream << "newTabPageHandle.undoAllMostVisitedDeletions();";
  EXPECT_TRUE(ExecuteScript(stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(overlay));

  // Make sure we have the same number of items as before.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count);
}

IN_PROC_BROWSER_TEST_F(InstantPolicyTest, ThemeBackgroundAccess) {
  InstallThemeSource();
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::RenderViewHost* rvh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();

  const std::string chrome_url("chrome://theme/IDR_THEME_NTP_BACKGROUND");
  const std::string search_url(
      "chrome-search://theme/IDR_THEME_NTP_BACKGROUND");
  bool loaded = false;
  ASSERT_TRUE(LoadImage(rvh, chrome_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_url;
  ASSERT_TRUE(LoadImage(rvh, search_url, &loaded));
  EXPECT_TRUE(loaded) << search_url;
}

// TODO(dhollowa): Fix flakes.  http://crbug.com/179930.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_FaviconAccess) {
  // Create a favicon.
  history::TopSites* top_sites = browser()->profile()->GetTopSites();
  GURL url("http://www.google.com/foo.html");
  gfx::Image thumbnail(CreateBitmap(SK_ColorWHITE));
  ThumbnailScore high_score(0.0, true, true, base::Time::Now());
  EXPECT_TRUE(top_sites->SetPageThumbnail(url, thumbnail, high_score));

  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::RenderViewHost* rvh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();

  // Get the favicons.
  const std::string chrome_favicon_url(
      "chrome://favicon/largest/http://www.google.com/foo.html");
  const std::string search_favicon_url(
      "chrome-search://favicon/largest/http://www.google.com/foo.html");
  bool loaded = false;
  ASSERT_TRUE(LoadImage(rvh, chrome_favicon_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_favicon_url;
  ASSERT_TRUE(LoadImage(rvh, search_favicon_url, &loaded));
  EXPECT_TRUE(loaded) << search_favicon_url;
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnNTP) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  const content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Instant-provided NTP should not have any bindings enabled.
  EXPECT_EQ(0, tab->GetRenderViewHost()->GetEnabledBindings());
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnPreview) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Typing in the omnibox shows the overlay.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  content::WebContents* preview = instant()->GetOverlayContents();
  ASSERT_NE(static_cast<content::WebContents*>(NULL), preview);

  // Instant preview should not have any bindings enabled.
  EXPECT_EQ(0, preview->GetRenderViewHost()->GetEnabledBindings());
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnResults) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Typing in the omnibox shows the overlay.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  content::WebContents* preview = instant()->GetOverlayContents();
  EXPECT_TRUE(instant()->model()->mode().is_search_suggestions());
  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_TRUE(instant()->model()->mode().is_default());
  const content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(preview, tab);

  // The commited Instant page should not have any bindings enabled.
  EXPECT_EQ(0, tab->GetRenderViewHost()->GetEnabledBindings());
}

// Only implemented in Views and Mac currently: http://crbug.com/164723
#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
#define MAYBE_HomeButtonAffectsMargin HomeButtonAffectsMargin
#else
#define MAYBE_HomeButtonAffectsMargin DISABLED_HomeButtonAffectsMargin
#endif
// Check that toggling the state of the home button changes the start-edge
// margin and width.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, MAYBE_HomeButtonAffectsMargin) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Get the current value of the start-edge margin and width.
  int start_margin;
  int width;
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.startMargin",
      &start_margin));
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.width", &width));

  // Toggle the home button visibility pref.
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  bool show_home = profile_prefs->GetBoolean(prefs::kShowHomeButton);
  profile_prefs->SetBoolean(prefs::kShowHomeButton, !show_home);

  // Make sure the margin and width changed.
  int new_start_margin;
  int new_width;
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.startMargin",
      &new_start_margin));
  EXPECT_TRUE(GetIntFromJS(overlay, "chrome.searchBox.width", &new_width));
  EXPECT_NE(start_margin, new_start_margin);
  EXPECT_NE(width, new_width);
  EXPECT_EQ(new_width - width, start_margin - new_start_margin);
}

// Commit does not happen on Mac: http://crbug.com/178520
#if defined(OS_MACOSX)
#define MAYBE_CommitWhenFocusLostInFullHeight \
        DISABLED_CommitWhenFocusLostInFullHeight
#else
#define MAYBE_CommitWhenFocusLostInFullHeight CommitWhenFocusLostInFullHeight
#endif
// Test that the overlay is committed when the omnibox loses focus when it is
// shown at 100% height.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_CommitWhenFocusLostInFullHeight) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Typing in the omnibox should show the overlay.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  EXPECT_TRUE(instant()->IsOverlayingSearchResults());
  EXPECT_EQ(overlay, instant()->GetOverlayContents());

  // Explicitly unfocus the omnibox without triggering a click. Note that this
  // doesn't actually change the focus state of the omnibox, only what the
  // Instant controller sees it as.
  omnibox()->model()->OnWillKillFocus(NULL);
  omnibox()->model()->OnKillFocus();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);
}

#if defined(OS_MACOSX)
// http://crbug.com/227076
#define MAYBE_CommitWhenShownInFullHeightWithoutFocus \
        DISABLED_CommitWhenShownInFullHeightWithoutFocus
#else
#define MAYBE_CommitWhenShownInFullHeightWithoutFocus \
        CommitWhenShownInFullHeightWithoutFocus
#endif

// Test that the overlay is committed when shown at 100% height without focus
// in the omnibox.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_CommitWhenShownInFullHeightWithoutFocus) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Create an observer to wait for the commit.
  content::WindowedNotificationObserver commit_observer(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllSources());

  // Create an observer to wait for the autocomplete.
  content::WindowedNotificationObserver autocomplete_observer(
      chrome::NOTIFICATION_INSTANT_SENT_AUTOCOMPLETE_RESULTS,
      content::NotificationService::AllSources());

  // Typing in the omnibox should show the overlay. Don't wait for the overlay
  // to show however.
  SetOmniboxText("query");

  autocomplete_observer.Wait();

  // Explicitly unfocus the omnibox without triggering a click. Note that this
  // doesn't actually change the focus state of the omnibox, only what the
  // Instant controller sees it as.
  omnibox()->model()->OnWillKillFocus(NULL);
  omnibox()->model()->OnKillFocus();

  // Wait for the overlay to show.
  commit_observer.Wait();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);
}

// Test that a transient entry is set properly when the overlay is committed
// without a navigation.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, TransientEntrySet) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantOverlaySupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Commit the overlay without triggering a navigation.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllSources());
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);

  // The page hasn't navigated so there should be a transient entry with the
  // same URL but different page ID as the last committed entry.
  const content::NavigationEntry* transient_entry =
      active_tab->GetController().GetTransientEntry();
  const content::NavigationEntry* committed_entry =
      active_tab->GetController().GetLastCommittedEntry();
  EXPECT_EQ(transient_entry->GetURL(), committed_entry->GetURL());
  EXPECT_NE(transient_entry->GetPageID(), committed_entry->GetPageID());
}

// Test that the a transient entry is cleared when the overlay is committed
// with a navigation.
// TODO(samarth) : this test fails, http://crbug.com/181070.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_TransientEntryRemoved) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantOverlaySupport();
  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(overlay);
  EXPECT_TRUE(instant()->model()->mode().is_default());
  EXPECT_FALSE(instant()->IsOverlayingSearchResults());

  // Create an observer to wait for the commit.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllSources());

  // Trigger a navigation on commit.
  EXPECT_TRUE(ExecuteScript(
      "getApiHandle().oncancel = function() {"
      "  location.replace(location.href + '#q=query');"
      "};"
      ));

  // Commit the overlay.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Confirm that the overlay has been committed.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(overlay, active_tab);

  // The page has navigated so there should be no transient entry.
  const content::NavigationEntry* transient_entry =
      active_tab->GetController().GetTransientEntry();
  EXPECT_EQ(NULL, transient_entry);

  // The last committed entry should be the URL the page navigated to.
  const content::NavigationEntry* committed_entry =
      active_tab->GetController().GetLastCommittedEntry();
  EXPECT_TRUE(EndsWith(committed_entry->GetURL().spec(), "#q=query", true));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, RestrictedURLReading) {
  std::string search_query;
  bool is_undefined;
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Verify we can read out something ok.
  const char kOKQuery[] = "santa";
  SetOmniboxText(kOKQuery);
  EXPECT_EQ(ASCIIToUTF16(kOKQuery), omnibox()->GetText());
  // Must always assert the value is defined before trying to read it, as trying
  // to read undefined values causes the test to hang.
  EXPECT_TRUE(GetBoolFromJS(instant()->GetOverlayContents(),
                            "apiHandle.value === undefined",
                            &is_undefined));
  ASSERT_FALSE(is_undefined);
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "apiHandle.value",
                              &search_query));
  EXPECT_EQ(kOKQuery, search_query);

  // Verify we can't read out something that should be restricted, like a https
  // url with a path.
  const char kHTTPSUrlWithPath[] = "https://www.example.com/foobar";
  SetOmniboxText(kHTTPSUrlWithPath);
  EXPECT_EQ(ASCIIToUTF16(kHTTPSUrlWithPath), omnibox()->GetText());
  EXPECT_TRUE(GetBoolFromJS(instant()->GetOverlayContents(),
                            "apiHandle.value === undefined",
                            &is_undefined));
  EXPECT_TRUE(is_undefined);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, RestrictedItemReadback) {
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Get a handle to the NTP and the current state of the JS.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* preview_tab = instant()->ntp()->contents();
  EXPECT_TRUE(preview_tab);

  // Manufacture a few autocomplete results and get them down to the page.
  std::vector<InstantAutocompleteResult> autocomplete_results;
  for (int i = 0; i < 3; ++i) {
    std::string description(base::StringPrintf("Test Description %d", i));
    std::string url(base::StringPrintf("http://www.testurl%d.com", i));

    InstantAutocompleteResult res;
    res.provider = ASCIIToUTF16(AutocompleteProvider::TypeToString(
        AutocompleteProvider::TYPE_BUILTIN));
    res.type = ASCIIToUTF16(AutocompleteMatch::TypeToString(
        AutocompleteMatch::SEARCH_WHAT_YOU_TYPED)),
    res.description = ASCIIToUTF16(description);
    res.destination_url = ASCIIToUTF16(url);
    res.transition = content::PAGE_TRANSITION_TYPED;
    res.relevance = 42 + i;

    autocomplete_results.push_back(res);
  }
  instant()->overlay()->SendAutocompleteResults(autocomplete_results);

  // Apparently, one needs to access nativeSuggestions before
  // apiHandle.setRestrictedValue can work.
  EXPECT_TRUE(ExecuteScript("var foo = apiHandle.nativeSuggestions;"));

  const char kQueryString[] = "Hippos go berzerk!";

  // First set the query text to a non restricted value and ensure it can be
  // read back.
  std::ostringstream stream;
  stream << "apiHandle.setValue('" << kQueryString << "');";
  EXPECT_TRUE(ExecuteScript(stream.str()));

  std::string query_string;
  bool is_undefined;
  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "apiHandle.value",
                              &query_string));
  EXPECT_EQ(kQueryString, query_string);

  // Set the query text to the first restricted autocomplete item.
  int rid = 1;
  stream.str(std::string());
  stream << "apiHandle.setRestrictedValue(" << rid << ");";
  EXPECT_TRUE(ExecuteScript(stream.str()));

  // Expect that we now receive undefined when reading the value back.
  EXPECT_TRUE(GetBoolFromJS(
      instant()->GetOverlayContents(),
      "apiHandle.value === undefined",
      &is_undefined));
  EXPECT_TRUE(is_undefined);

  // Now set the query text to a non restricted value and ensure that the
  // visibility has been reset and the string can again be read back.
  stream.str(std::string());
  stream << "apiHandle.setValue('" << kQueryString << "');";
  EXPECT_TRUE(ExecuteScript(stream.str()));

  EXPECT_TRUE(GetStringFromJS(instant()->GetOverlayContents(),
                              "apiHandle.value",
                              &query_string));
  EXPECT_EQ(kQueryString, query_string);
}

// Test that autocomplete results are sent to the page only when all the
// providers are done.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, AutocompleteProvidersDone) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ(0, on_native_suggestions_calls_);

  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("railroad"));

  EXPECT_EQ(overlay, instant()->GetOverlayContents());
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ(1, on_native_suggestions_calls_);
}

// Test that the local NTP is not preloaded.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, LocalNTPIsNotPreloaded) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  EXPECT_EQ(instant_url(), instant()->ntp_->contents()->GetURL());

  // The second argument says to use only the local overlay and NTP.
  instant()->SetInstantEnabled(false, true);

  EXPECT_EQ(NULL, instant()->ntp());
}

// Verify top bars visibility when searching on |DEFAULT| pages and switching
// between tabs.  Only implemented in Views and Mac currently.
#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
#define MAYBE_TopBarsVisibilityWhenSwitchingTabs \
    TopBarsVisibilityWhenSwitchingTabs
#else
#define MAYBE_TopBarsVisibilityWhenSwitchingTabs \
    DISABLED_TopBarsVisibilityWhenSwitchingTabs
#endif
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_TopBarsVisibilityWhenSwitchingTabs) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Open 2 tabs of |DFEAULT| mode.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("http://www.example.com"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Verify there are two tabs, and their top bars are visible.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  SearchTabHelper* tab0_helper = SearchTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  SearchTabHelper* tab1_helper = SearchTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_TRUE(tab0_helper->model()->top_bars_visible());
  EXPECT_TRUE(tab1_helper->model()->top_bars_visible());

  // Select 1st tab and type in omnibox.  We really want a partial-height
  // overlay, so that it doesn't get committed when switching away from this
  // tab.  However, the instant_extended.html used for tests always shows at
  // full height, though it doesn't commit the overlay when the omnibox text is
  // a URL and not a search.  So we specifically set a URL as the omnibox text.
  // The overlay showing will trigger top bars in 1st tab to be hidden, but keep
  // 2nd tab's visible.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("http://www.example.com/"));
  EXPECT_FALSE(tab0_helper->model()->top_bars_visible());
  EXPECT_TRUE(tab1_helper->model()->top_bars_visible());

  // Select 2nd tab, top bars of 1st tab should become visible, because its
  // overlay has been hidden.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_TRUE(tab0_helper->model()->top_bars_visible());
  EXPECT_TRUE(tab1_helper->model()->top_bars_visible());

  // Select back 1st tab, top bars visibility of both tabs should be the same
  // as before.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(tab0_helper->model()->top_bars_visible());
  EXPECT_TRUE(tab1_helper->model()->top_bars_visible());

  // Type in omnibox to trigger full-height overlay, which will trigger its top
  // bars to be hidden, but keep 2nd tab's visible.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("query"));
  EXPECT_FALSE(tab0_helper->model()->top_bars_visible());
  EXPECT_TRUE(tab1_helper->model()->top_bars_visible());

  // Select 2nd tab, its top bars should show, but top bars for 1st tab should
  // remain hidden, because the committed full-height overlay is still showing
  // suggestions.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_FALSE(tab0_helper->model()->top_bars_visible());
  EXPECT_TRUE(tab1_helper->model()->top_bars_visible());
}

// Test that the Bookmark provider is enabled, and returns results.
// TODO(sreeram): Convert this to a unit test.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_HasBookmarkProvider) {
  // No need to setup Instant.
  set_browser(browser());

  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(bookmark_model);
  ui_test_utils::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_utils::AddIfNotBookmarked(bookmark_model, GURL("http://angeline/"),
                                     ASCIIToUTF16("angeline"));

  SetOmniboxText("angeline");

  bool found_bookmark_match = false;

  const AutocompleteResult& result = omnibox()->model()->result();
  for (AutocompleteResult::const_iterator iter = result.begin();
       !found_bookmark_match && iter != result.end(); ++iter) {
    found_bookmark_match = iter->type == AutocompleteMatch::BOOKMARK_TITLE;
  }

  EXPECT_TRUE(found_bookmark_match);
}

// Test that the omnibox's temporary text is reset when the popup is closed.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, TemporaryTextResetWhenPopupClosed) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Show the overlay and arrow-down to a suggestion (this sets temporary text).
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("juju"));
  SendDownArrow();

  EXPECT_TRUE(HasTemporaryText());
  EXPECT_EQ("result 1", GetOmniboxText());

  // Click outside the omnibox (but not on the overlay), to make the omnibox
  // lose focus. Close the popup explicitly, to workaround test/toolkit issues.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TOOLBAR);
  omnibox()->CloseOmniboxPopup();

  // The temporary text should've been accepted as the user text.
  EXPECT_FALSE(HasTemporaryText());
  EXPECT_EQ("result 1", GetOmniboxText());

  // Now refocus the omnibox and hit Escape. This shouldn't crash.
  FocusOmnibox();
  SendEscape();

  // The omnibox should've reverted to the underlying permanent URL.
  EXPECT_FALSE(HasTemporaryText());
  EXPECT_EQ(std::string(chrome::kAboutBlankURL), GetOmniboxText());
}

// Test that autocomplete results aren't sent when the popup is closed.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       NoAutocompleteResultsWhenPopupClosed) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Show the overlay and arrow-down to a suggestion (this sets temporary text).
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("thangam"));
  SendDownArrow();
  EXPECT_TRUE(HasTemporaryText());

  EXPECT_TRUE(ExecuteScript("onChangeCalls = onNativeSuggestionsCalls = 0;"));

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ(0, on_change_calls_);
  EXPECT_EQ(0, on_native_suggestions_calls_);

  // Click outside the omnibox (but not on the overlay), to make the omnibox
  // lose focus. Close the popup explicitly, to workaround test/toolkit issues.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TOOLBAR);
  omnibox()->CloseOmniboxPopup();
  EXPECT_FALSE(HasTemporaryText());

  EXPECT_EQ(overlay, instant()->GetOverlayContents());
  EXPECT_TRUE(UpdateSearchState(overlay));
  EXPECT_EQ(0, on_change_calls_);
  EXPECT_EQ(0, on_native_suggestions_calls_);
}

// Test that suggestions are not accepted when unexpected.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DeniesUnexpectedSuggestions) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("chip"));
  SendDownArrow();

  EXPECT_EQ("result 1", GetOmniboxText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());

  // Make the page send an unexpected suggestion.
  EXPECT_TRUE(ExecuteScript("suggestion = 'chippies';"
                            "handleOnChange();"));

  // Verify that the suggestion is ignored.
  EXPECT_EQ("result 1", GetOmniboxText());
  EXPECT_EQ(ASCIIToUTF16(""), GetGrayText());
}

// Test that autocomplete results are cleared when the query is cleared.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, EmptyAutocompleteResults) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Type a URL, so that there's at least one autocomplete result (a "URL what
  // you typed" match).
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("http://upsamina/"));

  content::WebContents* overlay = instant()->GetOverlayContents();

  int num_autocomplete_results = 0;
  EXPECT_TRUE(GetIntFromJS(
      overlay,
      "chrome.embeddedSearch.searchBox.nativeSuggestions.length",
      &num_autocomplete_results));
  EXPECT_LT(0, num_autocomplete_results);

  // Erase the query in the omnibox.
  SetOmniboxText("");

  EXPECT_TRUE(GetIntFromJS(
      overlay,
      "chrome.embeddedSearch.searchBox.nativeSuggestions.length",
      &num_autocomplete_results));
  EXPECT_EQ(0, num_autocomplete_results);
}

// Test that hitting Esc to clear the omnibox works. http://crbug.com/231744.
// TODO(kmadhusu): Investigate and re-enable this test.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_EscapeClearsOmnibox) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Navigate to the Instant NTP, and wait for it to be recognized.
  content::WindowedNotificationObserver instant_tab_observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  instant_tab_observer.Wait();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Type a query. Verify that the query is seen by the page.
  SetOmniboxText("mojo");
  std::string query;
  EXPECT_TRUE(GetStringFromJS(contents, "chrome.embeddedSearch.searchBox.value",
                              &query));
  EXPECT_EQ("mojo", query);

  EXPECT_TRUE(content::ExecuteScript(contents,
                                     "onChangeCalls = submitCount = 0;"));

  // Hit Escape, and verify that the page sees that the query is cleared.
  SendEscape();
  EXPECT_TRUE(GetStringFromJS(contents, "chrome.embeddedSearch.searchBox.value",
                              &query));
  EXPECT_EQ("", query);
  EXPECT_EQ("", GetOmniboxText());

  EXPECT_TRUE(UpdateSearchState(contents));
  EXPECT_LT(0, on_change_calls_);
  EXPECT_EQ(0, submit_count_);
  EXPECT_LT(0, on_esc_key_press_event_calls_);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OnDefaultSearchProviderChanged) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  EXPECT_EQ(1, instant_service->GetInstantProcessCount());

  // Navigating to the NTP should use the Instant render process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  content::WebContents* ntp_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome::IsInstantNTP(ntp_contents));
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_contents->GetRenderProcessHost()->GetID()));
  GURL ntp_url = ntp_contents->GetURL();

  AddBlankTabAndShow(browser());
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(chrome::IsInstantNTP(active_tab));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("t");
  data.SetURL("http://defaultturl/q={searchTerms}");
  data.suggestions_url = "http://defaultturl2/q={searchTerms}";
  data.instant_url = "http://does/not/exist";
  data.alternate_urls.push_back(data.instant_url + "#q={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url = new TemplateURL(browser()->profile(), data);
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  service->Add(template_url);  // Takes ownership of |template_url|.

  // Change the default search provider.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &ntp_contents->GetController()));
  service->SetDefaultSearchProvider(template_url);
  observer.Wait();

  // |ntp_contents| should not use the Instant render process.
  EXPECT_FALSE(chrome::IsInstantNTP(ntp_contents));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      ntp_contents->GetRenderProcessHost()->GetID()));
  // Make sure the URL remains the same.
  EXPECT_EQ(ntp_url, ntp_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OverlayRenderViewGone) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  EXPECT_NE(static_cast<content::WebContents*>(NULL),
            instant()->GetOverlayContents());

  // Overlay is not reloaded after being killed.
  EXPECT_FALSE(instant()->overlay()->IsLocal());
  instant()->InstantPageRenderViewGone(instant()->GetOverlayContents());
  EXPECT_EQ(NULL, instant()->GetOverlayContents());

  // The local overlay is used on the next Update().
  SetOmniboxText("query");
  EXPECT_TRUE(instant()->overlay()->IsLocal());

  // Switched back to the remote overlay when omnibox loses and regains focus.
  instant()->HideOverlay();
  browser()->tab_strip_model()->GetActiveWebContents()->GetView()->Focus();
  FocusOmnibox();
  EXPECT_FALSE(instant()->overlay()->IsLocal());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OverlayDoesntSupportInstant) {
  GURL instant_url = test_server()->GetURL("files/empty.html?strk=1");
  InstantTestBase::Init(instant_url);
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus the omnibox. When the support determination response comes back,
  // Instant will destroy the non-Instant page and fall back to the local page.
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  ASSERT_NE(static_cast<InstantOverlay*>(NULL), instant()->overlay());
  EXPECT_TRUE(instant()->overlay()->IsLocal());

  // The local overlay is used on the next Update().
  SetOmniboxText("query");
  EXPECT_TRUE(instant()->overlay()->IsLocal());

  // Switched back to the remote overlay when omnibox loses and regains focus.
  instant()->HideOverlay();
  browser()->tab_strip_model()->GetActiveWebContents()->GetView()->Focus();
  FocusOmnibox();
  EXPECT_FALSE(instant()->overlay()->IsLocal());

  // Overlay falls back to local again after determining support.
  FocusOmniboxAndWaitForInstantOverlaySupport();
  ASSERT_NE(static_cast<InstantOverlay*>(NULL), instant()->overlay());
  EXPECT_TRUE(instant()->overlay()->IsLocal());
}

// Test that if Instant alters the input from URL to search, it's respected.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, InputChangedFromURLToSearch) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(ExecuteScript("suggestions = ['mcqueen.com'];"));

  SetOmniboxTextAndWaitForOverlayToShow("lightning");
  EXPECT_EQ("lightning", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("mcqueen.com", GetOmniboxText());

  // Press Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // Confirm that the Instant overlay was committed.
  EXPECT_EQ(overlay, browser()->tab_strip_model()->GetActiveWebContents());
}

// Test that if Instant alters the input from search to URL, it's respected.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, InputChangedFromSearchToURL) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  content::WebContents* overlay = instant()->GetOverlayContents();
  EXPECT_TRUE(ExecuteScript("suggestionType = 1;"));  // INSTANT_SUGGESTION_URL

  SetOmniboxTextAndWaitForOverlayToShow("mack.com");
  EXPECT_EQ("mack.com", GetOmniboxText());

  SendDownArrow();
  EXPECT_EQ("result 1", GetOmniboxText());

  // Press Enter.
  browser()->window()->GetLocationBar()->AcceptInput();

  // Confirm that the Instant overlay was NOT committed.
  EXPECT_NE(overlay, browser()->tab_strip_model()->GetActiveWebContents());
}

// Test that renderer initiated navigations to an instant URL from a non
// Instant page do not end up in an Instant process if they are bounced to the
// browser.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       RendererInitiatedNavigationNotInInstantProcess) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Don't use https server for the non instant URL so that the browser uses
  // different RenderViews.
  GURL non_instant_url = test_server()->GetURL("files/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      non_instant_url,
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetRenderProcessHost()->GetID()));
  EXPECT_EQ(non_instant_url, contents->GetURL());

  int old_render_view_id = contents->GetRenderViewHost()->GetRoutingID();
  int old_render_process_id = contents->GetRenderProcessHost()->GetID();

  std::string instant_url_with_query = instant_url().spec() + "q=3";
  std::string add_link_script = base::StringPrintf(
      "var a = document.createElement('a');"
      "a.id = 'toClick';"
      "a.href = '%s';"
      "document.body.appendChild(a);",
      instant_url_with_query.c_str());
  EXPECT_TRUE(content::ExecuteScript(contents, add_link_script));

  // Ensure that navigations are bounced to the browser.
  contents->GetMutableRendererPrefs()->browser_handles_all_top_level_requests =
      true;
  contents->GetRenderViewHost()->SyncRendererPrefs();

  content::WindowedNotificationObserver observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('toClick').click();"));
  observer.Wait();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetRenderProcessHost()->GetID()));
  EXPECT_EQ(GURL(instant_url_with_query), contents->GetURL());
  int new_render_view_id = contents->GetRenderViewHost()->GetRoutingID();
  int new_render_process_id = contents->GetRenderProcessHost()->GetID();

  EXPECT_TRUE(old_render_process_id != new_render_process_id ||
              old_render_view_id != new_render_view_id);
}

// Test that renderer initiated navigations to an Instant URL from an
// Instant process end up in an Instant process.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       RendererInitiatedNavigationInInstantProcess) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      instant_url(),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetRenderProcessHost()->GetID()));

  std::string instant_url_with_query = instant_url().spec() + "q=3";
  std::string add_link_script = base::StringPrintf(
      "var a = document.createElement('a');"
      "a.id = 'toClick';"
      "a.href = '%s';"
      "document.body.appendChild(a);",
      instant_url_with_query.c_str());
  EXPECT_TRUE(content::ExecuteScript(contents, add_link_script));

  content::WindowedNotificationObserver observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('toClick').click();"));
  observer.Wait();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetRenderProcessHost()->GetID()));
  EXPECT_EQ(GURL(instant_url_with_query), contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, SearchProviderDoesntRun) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Add "query" to history.
  ASSERT_TRUE(AddSearchToHistory(ASCIIToUTF16("query"), 10000));
  BlockUntilHistoryProcessesPendingRequests();

  SetOmniboxText("quer");

  // Should get only SWYT from SearchProvider.
  EXPECT_EQ(1, CountSearchProviderSuggestions());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, SearchProviderRunsForLocalOnly) {
  // Force local-only Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  instant()->SetInstantEnabled(true, true);
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Add "query" to history.
  ASSERT_TRUE(AddSearchToHistory(ASCIIToUTF16("query"), 10000));
  BlockUntilHistoryProcessesPendingRequests();

  SetOmniboxText("quer");

  // Should get 2 suggestions from SearchProvider:
  //   - SWYT for "quer"
  //   - Search history suggestion for "query"
  EXPECT_EQ(2, CountSearchProviderSuggestions());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, SearchProviderRunsForFallback) {
  // Use an Instant URL that won't support Instant.
  GURL instant_url = test_server()->GetURL("files/empty.html?strk=1");
  InstantTestBase::Init(instant_url);
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();
  // Should fallback to the local overlay.
  ASSERT_NE(static_cast<InstantOverlay*>(NULL), instant()->overlay());
  EXPECT_TRUE(instant()->overlay()->IsLocal());

  // Add "query" to history and wait for Instant support.
  ASSERT_TRUE(AddSearchToHistory(ASCIIToUTF16("query"), 10000));
  BlockUntilHistoryProcessesPendingRequests();

  SetOmniboxText("quer");

  // Should get 2 suggestions from SearchProvider:
  //   - SWYT for "quer"
  //   - Search history suggestion for "query"
  EXPECT_EQ(2, CountSearchProviderSuggestions());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, SearchProviderForLocalNTP) {
  // Force local-only Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();
  instant()->SetInstantEnabled(true, true);

  // Add "google" to history.
  ASSERT_TRUE(AddSearchToHistory(ASCIIToUTF16("google"), 10000));
  BlockUntilHistoryProcessesPendingRequests();

  // Create an observer to wait for the autocomplete.
  content::WindowedNotificationObserver autocomplete_observer(
      chrome::NOTIFICATION_INSTANT_SENT_AUTOCOMPLETE_RESULTS,
      content::NotificationService::AllSources());

  SetOmniboxText("http://www.example.com");

  autocomplete_observer.Wait();
  ASSERT_TRUE(omnibox()->model()->autocomplete_controller()->
              search_provider()->IsNonInstantSearchDone());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, AcceptingURLSearchDoesNotNavigate) {
  // Get a committed Instant tab, which will be in the Instant process and thus
  // support chrome::GetSearchTerms().
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Do a search and commit it.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("foo"));
  EXPECT_EQ(ASCIIToUTF16("foo"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Set URL-like search terms for the instant tab.
  content::WebContents* instant_tab = instant()->instant_tab()->contents();
  content::NavigationEntry* visible_entry =
      instant_tab->GetController().GetVisibleEntry();
  visible_entry->SetExtraData(sessions::kSearchTermsKey,
                              ASCIIToUTF16("http://example.com"));
  SetOmniboxText("http://example.com");
  omnibox()->model()->SetInputInProgress(false);
  omnibox()->CloseOmniboxPopup();

  // Accept the omnibox input.
  EXPECT_FALSE(omnibox()->model()->user_input_in_progress());
  EXPECT_EQ(ToolbarModel::URL_LIKE_SEARCH_TERMS,
            browser()->toolbar_model()->GetSearchTermsType());
  GURL instant_tab_url = instant_tab->GetURL();
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ(instant_tab_url, instant_tab->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, AcceptingJSSearchDoesNotRunJS) {
  // Get a committed Instant tab, which will be in the Instant process and thus
  // support chrome::GetSearchTerms().
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Do a search and commit it.
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("foo"));
  EXPECT_EQ(ASCIIToUTF16("foo"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Set URL-like search terms for the instant tab.
  content::WebContents* instant_tab = instant()->instant_tab()->contents();
  content::NavigationEntry* visible_entry =
      instant_tab->GetController().GetVisibleEntry();
  const char kEvilJS[] = "javascript:document.title='evil';1;";
  visible_entry->SetExtraData(sessions::kSearchTermsKey, ASCIIToUTF16(kEvilJS));
  SetOmniboxText(kEvilJS);
  omnibox()->model()->SetInputInProgress(false);
  omnibox()->CloseOmniboxPopup();

  // Accept the omnibox input.
  EXPECT_FALSE(omnibox()->model()->user_input_in_progress());
  EXPECT_EQ(ToolbarModel::URL_LIKE_SEARCH_TERMS,
            browser()->toolbar_model()->GetSearchTermsType());
  browser()->window()->GetLocationBar()->AcceptInput();
  // Force some Javascript to run in the renderer so the inline javascript:
  // would be forced to run if it's going to.
  EXPECT_TRUE(content::ExecuteScript(instant_tab, "1;"));
  EXPECT_NE(ASCIIToUTF16("evil"), instant_tab->GetTitle());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       ReloadSearchAfterBackReloadsCorrectQuery) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantOverlaySupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Search for [foo].
  ASSERT_TRUE(SetOmniboxTextAndWaitForOverlayToShow("foo"));
  EXPECT_EQ(ASCIIToUTF16("foo"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Search again for [bar].
  content::WebContents* instant_tab = instant()->instant_tab()->contents();
  EXPECT_TRUE(content::ExecuteScript(instant_tab,
                                     "suggestion = 'bart';"));
  SetOmniboxTextAndWaitForSuggestion("bar");
  EXPECT_EQ(ASCIIToUTF16("t"), GetGrayText());

  // Accept the new query and wait for the page to navigate.
  content::WindowedNotificationObserver nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  browser()->window()->GetLocationBar()->AcceptInput();
  nav_observer.Wait();

  // Press back button and reload.
  content::WindowedNotificationObserver back_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  instant_tab->GetController().GoBack();
  back_observer.Wait();
  EXPECT_EQ("foo", GetOmniboxText());
  FocusOmnibox();
  content::WindowedNotificationObserver reload_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  browser()->window()->GetLocationBar()->AcceptInput();
  reload_observer.Wait();

  EXPECT_EQ("foo", GetOmniboxText());
}

class InstantExtendedFirstTabTest : public InProcessBrowserTest,
                                    public InstantTestBase {
 public:
  InstantExtendedFirstTabTest() {}
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableInstantExtendedAPI);
    command_line->AppendSwitch(switches::kDisableLocalFirstLoadNTP);
  }
};

IN_PROC_BROWSER_TEST_F(
    InstantExtendedFirstTabTest, RedirectToLocalOnLoadFailure) {
  // Create a new window to test the first NTP load.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  const BrowserList* native_browser_list = BrowserList::GetInstance(
      chrome::HOST_DESKTOP_TYPE_NATIVE);
  ASSERT_EQ(2u, native_browser_list->size());
  set_browser(native_browser_list->get(1));

  FocusOmniboxAndWaitForInstantOverlayAndNTPSupport();

  // Also make sure our instant_tab_ is loaded.
  if (!instant()->instant_tab_) {
    content::WindowedNotificationObserver instant_tab_observer(
        chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
        content::NotificationService::AllSources());
    instant_tab_observer.Wait();
  }

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  EXPECT_TRUE(instant()->ntp()->IsLocal());

  // Overlay contents should be preloaded.
  ASSERT_NE(static_cast<InstantOverlay*>(NULL), instant()->overlay());
  EXPECT_TRUE(instant()->overlay()->IsLocal());

  // Instant tab contents should be preloaded.
  ASSERT_NE(static_cast<InstantTab*>(NULL), instant()->instant_tab());
  EXPECT_TRUE(instant()->instant_tab()->IsLocal());
}
