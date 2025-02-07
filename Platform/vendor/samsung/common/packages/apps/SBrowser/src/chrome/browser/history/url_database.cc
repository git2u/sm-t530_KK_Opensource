// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/url_database.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/i18n/case_conversion.h"

#if SBROWSER_OMNIBOX_SMART_SUGGEST
#include "base/logging.h"
#endif

#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "sql/statement.h"
#include "ui/base/l10n/l10n_util.h"

namespace history {

const char URLDatabase::kURLRowFields[] = HISTORY_URL_ROW_FIELDS;
const int URLDatabase::kNumURLRowFields = 9;

URLDatabase::URLEnumeratorBase::URLEnumeratorBase()
    : initialized_(false) {
}

URLDatabase::URLEnumeratorBase::~URLEnumeratorBase() {
}

URLDatabase::URLEnumerator::URLEnumerator() {
}

URLDatabase::IconMappingEnumerator::IconMappingEnumerator() {
}

bool URLDatabase::URLEnumerator::GetNextURL(URLRow* r) {
  if (statement_.Step()) {
    FillURLRow(statement_, r);
    return true;
  }
  return false;
}

bool URLDatabase::IconMappingEnumerator::GetNextIconMapping(IconMapping* r) {
  if (!statement_.Step())
    return false;

  r->page_url = GURL(statement_.ColumnString(0));
  r->icon_id =  statement_.ColumnInt64(1);
  return true;
}

URLDatabase::URLDatabase()
    : has_keyword_search_terms_(false) {
}

URLDatabase::~URLDatabase() {
}

// static
std::string URLDatabase::GURLToDatabaseURL(const GURL& gurl) {
  // TODO(brettw): do something fancy here with encoding, etc.

  // Strip username and password from URL before sending to DB.
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();

  return (gurl.ReplaceComponents(replacements)).spec();
}

// Convenience to fill a history::URLRow. Must be in sync with the fields in
// kURLRowFields.
void URLDatabase::FillURLRow(sql::Statement& s, history::URLRow* i) {
  DCHECK(i);
  i->id_ = s.ColumnInt64(0);
  i->url_ = GURL(s.ColumnString(1));
  i->title_ = s.ColumnString16(2);
  i->visit_count_ = s.ColumnInt(3);
  i->typed_count_ = s.ColumnInt(4);
  i->last_visit_ = base::Time::FromInternalValue(s.ColumnInt64(5));
  i->hidden_ = s.ColumnInt(6) != 0;
}

bool URLDatabase::GetURLRow(URLID url_id, URLRow* info) {
  // TODO(brettw) We need check for empty URLs to handle the case where
  // there are old URLs in the database that are empty that got in before
  // we added any checks. We should eventually be able to remove it
  // when all inputs are using GURL (which prohibit empty input).
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE id=?"));
  statement.BindInt64(0, url_id);

  if (statement.Step()) {
    FillURLRow(statement, info);
    return true;
  }
  return false;
}

bool URLDatabase::GetAllTypedUrls(URLRows* urls) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE typed_count > 0"));

  while (statement.Step()) {
    URLRow info;
    FillURLRow(statement, &info);
    urls->push_back(info);
  }
  return true;
}

URLID URLDatabase::GetRowForURL(const GURL& url, history::URLRow* info) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls WHERE url=?"));
  std::string url_string = GURLToDatabaseURL(url);
  statement.BindString(0, url_string);

  if (!statement.Step())
    return 0;  // no data

  if (info)
    FillURLRow(statement, info);
  return statement.ColumnInt64(0);
}

bool URLDatabase::UpdateURLRow(URLID url_id,
                               const history::URLRow& info) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE urls SET title=?,visit_count=?,typed_count=?,last_visit_time=?,"
        "hidden=?"
      "WHERE id=?"));
  statement.BindString16(0, info.title());
  statement.BindInt(1, info.visit_count());
  statement.BindInt(2, info.typed_count());
  statement.BindInt64(3, info.last_visit().ToInternalValue());
  statement.BindInt(4, info.hidden() ? 1 : 0);
  statement.BindInt64(5, url_id);

  return statement.Run();
}

URLID URLDatabase::AddURLInternal(const history::URLRow& info,
                                  bool is_temporary) {
  // This function is used to insert into two different tables, so we have to
  // do some shuffling. Unfortinately, we can't use the macro
  // HISTORY_URL_ROW_FIELDS because that specifies the table name which is
  // invalid in the insert syntax.
  #define ADDURL_COMMON_SUFFIX \
      " (url, title, visit_count, typed_count, "\
      "last_visit_time, hidden) "\
      "VALUES (?,?,?,?,?,?)"
  const char* statement_name;
  const char* statement_sql;
  if (is_temporary) {
    statement_name = "AddURLTemporary";
    statement_sql = "INSERT INTO temp_urls" ADDURL_COMMON_SUFFIX;
  } else {
    statement_name = "AddURL";
    statement_sql = "INSERT INTO urls" ADDURL_COMMON_SUFFIX;
  }
  #undef ADDURL_COMMON_SUFFIX

  sql::Statement statement(GetDB().GetCachedStatement(
      sql::StatementID(statement_name), statement_sql));
  statement.BindString(0, GURLToDatabaseURL(info.url()));
  statement.BindString16(1, info.title());
  statement.BindInt(2, info.visit_count());
  statement.BindInt(3, info.typed_count());
  statement.BindInt64(4, info.last_visit().ToInternalValue());
  statement.BindInt(5, info.hidden() ? 1 : 0);

  if (!statement.Run()) {
    VLOG(0) << "Failed to add url " << info.url().possibly_invalid_spec()
            << " to table history.urls.";
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool URLDatabase::DeleteURLRow(URLID id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM urls WHERE id = ?"));
  statement.BindInt64(0, id);

  if (!statement.Run())
    return false;

  // And delete any keyword visits.
  if (!has_keyword_search_terms_)
    return true;

  sql::Statement del_keyword_visit(GetDB().GetCachedStatement(SQL_FROM_HERE,
                          "DELETE FROM keyword_search_terms WHERE url_id=?"));
  del_keyword_visit.BindInt64(0, id);

  return del_keyword_visit.Run();
}

bool URLDatabase::CreateTemporaryURLTable() {
  return CreateURLTable(true);
}

bool URLDatabase::CommitTemporaryURLTable() {
  // See the comments in the header file as well as
  // HistoryBackend::DeleteAllHistory() for more information on how this works
  // and why it does what it does.
  //
  // Note that the main database overrides this to additionally create the
  // supplimentary indices that the archived database doesn't need.

  // Swap the url table out and replace it with the temporary one.
  if (!GetDB().Execute("DROP TABLE urls")) {
    NOTREACHED() << GetDB().GetErrorMessage();
    return false;
  }
  if (!GetDB().Execute("ALTER TABLE temp_urls RENAME TO urls")) {
    NOTREACHED() << GetDB().GetErrorMessage();
    return false;
  }

  // Create the index over URLs. This is needed for the main, in-memory, and
  // archived databases, so we always do it. The supplimentary indices used by
  // the main database are not created here. When deleting all history, they
  // are created by HistoryDatabase::RecreateAllButStarAndURLTables().
  CreateMainURLIndex();

  return true;
}

bool URLDatabase::InitURLEnumeratorForEverything(URLEnumerator* enumerator) {
  DCHECK(!enumerator->initialized_);
  std::string sql("SELECT ");
  sql.append(kURLRowFields);
  sql.append(" FROM urls");
  enumerator->statement_.Assign(GetDB().GetUniqueStatement(sql.c_str()));
  enumerator->initialized_ = enumerator->statement_.is_valid();
  return enumerator->statement_.is_valid();
}

bool URLDatabase::InitURLEnumeratorForSignificant(URLEnumerator* enumerator) {
  DCHECK(!enumerator->initialized_);
  std::string sql("SELECT ");
  sql.append(kURLRowFields);
  sql.append(" FROM urls WHERE last_visit_time >= ? OR visit_count >= ? OR "
             "typed_count >= ?");
  enumerator->statement_.Assign(GetDB().GetUniqueStatement(sql.c_str()));
  enumerator->statement_.BindInt64(
      0, AutocompleteAgeThreshold().ToInternalValue());
  enumerator->statement_.BindInt(1, kLowQualityMatchVisitLimit);
  enumerator->statement_.BindInt(2, kLowQualityMatchTypedLimit);
  enumerator->initialized_ = enumerator->statement_.is_valid();
  return enumerator->statement_.is_valid();
}

bool URLDatabase::InitIconMappingEnumeratorForEverything(
    IconMappingEnumerator* enumerator) {
  DCHECK(!enumerator->initialized_);
  enumerator->statement_.Assign(GetDB().GetUniqueStatement(
      "SELECT url, favicon_id FROM urls WHERE favicon_id <> 0"));
  enumerator->initialized_ = enumerator->statement_.is_valid();
  return enumerator->statement_.is_valid();
}

bool URLDatabase::AutocompleteForPrefix(const std::string& prefix,
                                        size_t max_results,
                                        bool typed_only,
                                        URLRows* results) {
  // NOTE: this query originally sorted by starred as the second parameter. But
  // as bookmarks is no longer part of the db we no longer include the order
  // by clause.
  results->clear();
  const char* sql;
  int line;
  if (typed_only) {
    sql = "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls "
        "WHERE url >= ? AND url < ? AND hidden = 0 AND typed_count > 0 "
        "ORDER BY typed_count DESC, visit_count DESC, last_visit_time DESC "
        "LIMIT ?";
    line = __LINE__;
  } else {
    sql = "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls "
        "WHERE url >= ? AND url < ? AND hidden = 0 "
        "ORDER BY typed_count DESC, visit_count DESC, last_visit_time DESC "
        "LIMIT ?";
    line = __LINE__;
  }
  sql::Statement statement(
      GetDB().GetCachedStatement(sql::StatementID(__FILE__, line), sql));

  // We will find all strings between "prefix" and this string, which is prefix
  // followed by the maximum character size. Use 8-bit strings for everything
  // so we can be sure sqlite is comparing everything in 8-bit mode. Otherwise,
  // it will have to convert strings either to UTF-8 or UTF-16 for comparison.
  std::string end_query(prefix);
  end_query.push_back(std::numeric_limits<unsigned char>::max());

  statement.BindString(0, prefix);
  statement.BindString(1, end_query);
  statement.BindInt(2, static_cast<int>(max_results));

  while (statement.Step()) {
    history::URLRow info;
    FillURLRow(statement, &info);
    if (info.url().is_valid())
      results->push_back(info);
  }
  return !results->empty();
}

bool URLDatabase::IsTypedHost(const std::string& host) {
  const char* schemes[] = {
    chrome::kHttpScheme,
    chrome::kHttpsScheme,
    chrome::kFtpScheme
  };
  URLRows dummy;
  for (size_t i = 0; i < arraysize(schemes); ++i) {
    std::string scheme_and_host(schemes[i]);
    scheme_and_host += content::kStandardSchemeSeparator + host;
    if (AutocompleteForPrefix(scheme_and_host + '/', 1, true, &dummy) ||
        AutocompleteForPrefix(scheme_and_host + ':', 1, true, &dummy))
      return true;
  }
  return false;
}

bool URLDatabase::FindShortestURLFromBase(const std::string& base,
                                          const std::string& url,
                                          int min_visits,
                                          int min_typed,
                                          bool allow_base,
                                          history::URLRow* info) {
  // Select URLs that start with |base| and are prefixes of |url|.  All parts
  // of this query except the substr() call can be done using the index.  We
  // could do this query with a couple of LIKE or GLOB statements as well, but
  // those wouldn't use the index, and would run into problems with "wildcard"
  // characters that appear in URLs (% for LIKE, or *, ? for GLOB).
  std::string sql("SELECT ");
  sql.append(kURLRowFields);
  sql.append(" FROM urls WHERE url ");
  sql.append(allow_base ? ">=" : ">");
  sql.append(" ? AND url < :end AND url = substr(:end, 1, length(url)) "
             "AND hidden = 0 AND visit_count >= ? AND typed_count >= ? "
             "ORDER BY url LIMIT 1");
  sql::Statement statement(GetDB().GetUniqueStatement(sql.c_str()));
  statement.BindString(0, base);
  statement.BindString(1, url);   // :end
  statement.BindInt(2, min_visits);
  statement.BindInt(3, min_typed);

  if (!statement.Step())
    return false;

  DCHECK(info);
  FillURLRow(statement, info);
  return true;
}

bool URLDatabase::InitKeywordSearchTermsTable() {
  has_keyword_search_terms_ = true;
  if (!GetDB().DoesTableExist("keyword_search_terms")) {
    if (!GetDB().Execute("CREATE TABLE keyword_search_terms ("
        "keyword_id INTEGER NOT NULL,"      // ID of the TemplateURL.
        "url_id INTEGER NOT NULL,"          // ID of the url.
        "lower_term LONGVARCHAR NOT NULL,"  // The search term, in lower case.
        "term LONGVARCHAR NOT NULL)"))      // The actual search term.
      return false;
  }
  return true;
}

bool URLDatabase::CreateKeywordSearchTermsIndices() {
  // For searching.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS keyword_search_terms_index1 ON "
          "keyword_search_terms (keyword_id, lower_term)")) {
    return false;
  }

  // For deletion.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS keyword_search_terms_index2 ON "
          "keyword_search_terms (url_id)")) {
    return false;
  }

  // For query or deletion by term.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS keyword_search_terms_index3 ON "
          "keyword_search_terms (term)")) {
    return false;
  }
  return true;
}

bool URLDatabase::DropKeywordSearchTermsTable() {
  // This will implicitly delete the indices over the table.
  return GetDB().Execute("DROP TABLE keyword_search_terms");
}

bool URLDatabase::SetKeywordSearchTermsForURL(URLID url_id,
                                              TemplateURLID keyword_id,
                                              const string16& term) {
  DCHECK(url_id && keyword_id && !term.empty());

  sql::Statement exist_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT term FROM keyword_search_terms "
      "WHERE keyword_id = ? AND url_id = ?"));
  exist_statement.BindInt64(0, keyword_id);
  exist_statement.BindInt64(1, url_id);

  if (exist_statement.Step())
    return true;  // Term already exists, no need to add it.

  if (!exist_statement.Succeeded())
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO keyword_search_terms (keyword_id, url_id, lower_term, term) "
      "VALUES (?,?,?,?)"));
  statement.BindInt64(0, keyword_id);
  statement.BindInt64(1, url_id);
  statement.BindString16(2, base::i18n::ToLower(term));
  statement.BindString16(3, term);
  return statement.Run();
}

bool URLDatabase::GetKeywordSearchTermRow(URLID url_id,
                                          KeywordSearchTermRow* row) {
  DCHECK(url_id);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT keyword_id, term FROM keyword_search_terms WHERE url_id=?"));
  statement.BindInt64(0, url_id);

  if (!statement.Step())
    return false;

  if (row) {
    row->url_id = url_id;
    row->keyword_id = statement.ColumnInt64(0);
    row->term = statement.ColumnString16(1);
  }
  return true;
}

bool URLDatabase::GetKeywordSearchTermRows(
    const string16& term,
    std::vector<KeywordSearchTermRow>* rows) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT keyword_id, url_id FROM keyword_search_terms WHERE term=?"));
  statement.BindString16(0, term);

  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    KeywordSearchTermRow row;
    row.url_id = statement.ColumnInt64(1);
    row.keyword_id = statement.ColumnInt64(0);
    row.term = term;
    rows->push_back(row);
  }
  return true;
}

void URLDatabase::DeleteAllSearchTermsForKeyword(
    TemplateURLID keyword_id) {
  DCHECK(keyword_id);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM keyword_search_terms WHERE keyword_id=?"));
  statement.BindInt64(0, keyword_id);

  statement.Run();
}

void URLDatabase::GetMostRecentKeywordSearchTerms(
    TemplateURLID keyword_id,
    const string16& prefix,
    int max_count,
    std::vector<KeywordSearchTermVisit>* matches) {
  // NOTE: the keyword_id can be zero if on first run the user does a query
  // before the TemplateURLService has finished loading. As the chances of this
  // occurring are small, we ignore it.
  if (!keyword_id)
    return;

  DCHECK(!prefix.empty());
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT DISTINCT kv.term, u.visit_count, u.last_visit_time "
      "FROM keyword_search_terms kv "
      "JOIN urls u ON kv.url_id = u.id "
      "WHERE kv.keyword_id = ? AND kv.lower_term >= ? AND kv.lower_term < ? "
      "ORDER BY u.last_visit_time DESC LIMIT ?"));

  // NOTE: Keep this ToLower() call in sync with search_provider.cc.
  string16 lower_prefix = base::i18n::ToLower(prefix);
  // This magic gives us a prefix search.
  string16 next_prefix = lower_prefix;
  next_prefix[next_prefix.size() - 1] =
      next_prefix[next_prefix.size() - 1] + 1;
  statement.BindInt64(0, keyword_id);
  statement.BindString16(1, lower_prefix);
  statement.BindString16(2, next_prefix);
  statement.BindInt(3, max_count);

  KeywordSearchTermVisit visit;
  while (statement.Step()) {
    visit.term = statement.ColumnString16(0);
    visit.visits = statement.ColumnInt(1);
    visit.time = base::Time::FromInternalValue(statement.ColumnInt64(2));
    matches->push_back(visit);
  }
}

bool URLDatabase::DeleteKeywordSearchTerm(const string16& term) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM keyword_search_terms WHERE term=?"));
  statement.BindString16(0, term);

  return statement.Run();
}

#if SBROWSER_OMNIBOX_SMART_SUGGEST
//TODO(anish.p): need to create index if searching is to be done efficiently
bool URLDatabase::InitSearchTermsResultsTable() {
  has_search_terms_results_ = true;
  if (!GetDB().DoesTableExist("search_terms_results")) {
    if (!GetDB().Execute("CREATE TABLE search_terms_results ("
        "url_id INTEGER NOT NULL,"          // ID of the url.
        "lower_term LONGVARCHAR NOT NULL,"  // The search term, in lower case.
        "term LONGVARCHAR NOT NULL)")) {     // The actual search term.
    	LOG(ERROR)<<"Failed to create search_terms_results table ";
      return false;
    }
  }
  LOG(INFO)<<"Created search_terms_results table ";
  return true;
}
bool URLDatabase::DropSearchTermsResultsTable() {
  // This will implicitly delete the indices over the table.
  return GetDB().Execute("DROP TABLE search_terms_results");
}

bool URLDatabase::SetSearchTermsForURL(URLID url_id,
                                       const string16& term) {
	LOG(INFO)<<"SetSearchTermsForURL: id: "<<url_id<<" terms: "<<term;
	DCHECK(url_id && !term.empty());
	sql::Statement exist_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
	      "SELECT term FROM search_terms_results "
	      "WHERE url_id = ?"));
	  exist_statement.BindInt64(0, url_id);

	  if (exist_statement.Step()){
		  LOG(ERROR)<<"Term already exists, we can replace or append.";
		  return true;  //TODO(anish.p) Term already exists, we can replace or append.
	  }
	  if (!exist_statement.Succeeded()){
		  LOG(ERROR)<<"SELECT query failed";
		  return false;
	  }

	  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
	       "INSERT INTO search_terms_results (url_id, lower_term, term) "
	       "VALUES (?,?,?)"));

	  statement.BindInt64(0, url_id);
	  statement.BindString16(1, base::i18n::ToLower(term));
	  statement.BindString16(2, term);
	  LOG(INFO)<<"Executing INSERT query";
	  return statement.Run();
}

bool URLDatabase::GetSearchTermResultsRow(URLID url_id,
										KeywordSearchTermRow* row) {
	 DCHECK(url_id);
	  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
	      "SELECT term FROM search_terms_results WHERE url_id=?"));
	  statement.BindInt64(0, url_id);

	  if (!statement.Step())
	    return false;

	  if (row) {
	    row->url_id = url_id;
	    row->keyword_id = -1;
	    row->term = statement.ColumnString16(0);
	  }
	  return true;
}

bool URLDatabase::GetSearchTermResultsRows(
    const string16& term,
    std::vector<KeywordSearchTermRow>* rows) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT url_id FROM search_terms_results "
      "WHERE lower_term >= ? AND lower_term < ? "));

  // NOTE: Keep this ToLower() call in sync with search_provider.cc.
  string16 lower_prefix = base::i18n::ToLower(term);
  // This magic gives us a prefix search.
  string16 next_prefix = lower_prefix;
  next_prefix[next_prefix.size() - 1] =
        next_prefix[next_prefix.size() - 1] + 1;

  statement.BindString16(0, lower_prefix);
  statement.BindString16(1, next_prefix);

  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    KeywordSearchTermRow row;
    row.url_id = statement.ColumnInt64(0);
    row.keyword_id = -1;
    row.term = term;
    rows->push_back(row);
  }
  return true;
}
// get search results from keyword
void URLDatabase::GetSearchTermsResults(
		const string16& prefix,
		std::vector<SearchProvider::NavigationResultForUrlDb>* matches) {
	DCHECK(!prefix.empty());
	LOG(INFO)<<"GetSearchTermsResults reached";
	sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
	      "SELECT DISTINCT sr.url_id, u.visit_count, u.url, u.title "
	      "FROM search_terms_results sr "
		  "JOIN urls u ON sr.url_id = u.id "
	      "WHERE sr.lower_term >= ? AND sr.lower_term < ?"));

	LOG(INFO)<<"Query statement created for: "<<prefix;
	string16 lower_prefix = base::i18n::ToLower(prefix);
	LOG(INFO)<<"lower_prefix: "<<lower_prefix;
	// This magic gives us a prefix search.
	string16 next_prefix = lower_prefix;
	LOG(INFO)<<"next_prefix: "<<next_prefix;

	next_prefix[next_prefix.size() - 1] =
	      next_prefix[next_prefix.size() - 1] + 1;

	LOG(INFO)<<"next_prefix: "<<next_prefix;

	LOG(INFO)<<"search string: "<<lower_prefix<<" | "<<next_prefix;
	if(statement.BindString16(0, lower_prefix))
		LOG(INFO)<<"lower prefix bound";
	else
		LOG(INFO)<<"lower prefix NOT bound";
	if(statement.BindString16(1, next_prefix))
		LOG(INFO)<<"next prefix bound to statement";
	else
		LOG(INFO)<<"next prefix NOT bound";

	LOG(INFO)<<"strings bound to the statement";
	//FIXME:
	GURL temp_url;
	string16 temp_string;
	SearchProvider::NavigationResultForUrlDb result(temp_url, temp_string, 0);
	LOG(INFO)<<"SQL statement: "<<statement.GetSQLStatement();
	LOG(INFO)<<"executing query in steps statement valid: "<<statement.is_valid();

	 while (statement.Step()) {
		 int url_id = statement.ColumnInt(0);
		 LOG(INFO)<<"result url id: "<<url_id;
		 std::string url = statement.ColumnString(2);
		 LOG(INFO)<<"url "<<url;
		 temp_url = GURL(url);
		 LOG(INFO)<<"result url: "<<temp_url.spec();
		 string16 title =  statement.ColumnString16(3);
		 result = SearchProvider::NavigationResultForUrlDb(temp_url,
				 title, 1400 /*FIXME: relevance */);
		matches->push_back(result);
	  }

	 LOG(INFO)<<"results populated status: "<<statement.Succeeded()
			 <<"column count"<<statement.ColumnCount();


	 sql::Statement test_statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
	 	      "SELECT * FROM search_terms_results"));
	 LOG(INFO)<<"executing test query: "<<test_statement.is_valid();
	 while (test_statement.Step()) {
			 int url_id = test_statement.ColumnInt(0);
			 LOG(INFO)<<"result url id: "<<url_id;
			 std::string test_terms = test_statement.ColumnString(1);
			 LOG(INFO)<<"test_terms "<<test_terms;

	}
	 sql::Statement test_statement2(GetDB().GetCachedStatement(SQL_FROM_HERE,
	 	 	      "SELECT * FROM urls"));
	 while (test_statement2.Step()) {
		 int url_id = test_statement2.ColumnInt(0);
		 LOG(INFO)<<"result url id: "<<url_id;
	 }
}

bool URLDatabase::DeleteSearchTermResults(const string16& term) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM search_terms_results WHERE term=?"));
  statement.BindString16(0, term);

  return statement.Run();
}
#endif

bool URLDatabase::DropStarredIDFromURLs() {
  if (!GetDB().DoesColumnExist("urls", "starred_id"))
    return true;  // urls is already updated, no need to continue.

  // Create a temporary table to contain the new URLs table.
  if (!CreateTemporaryURLTable()) {
    NOTREACHED();
    return false;
  }

  // Copy the contents.
  if (!GetDB().Execute(
      "INSERT INTO temp_urls (id, url, title, visit_count, typed_count, "
      "last_visit_time, hidden, favicon_id) "
      "SELECT id, url, title, visit_count, typed_count, last_visit_time, "
      "hidden, favicon_id FROM urls")) {
    NOTREACHED() << GetDB().GetErrorMessage();
    return false;
  }

  // Rename/commit the tmp table.
  CommitTemporaryURLTable();

  return true;
}

bool URLDatabase::CreateURLTable(bool is_temporary) {
  const char* name = is_temporary ? "temp_urls" : "urls";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append("("
      "id INTEGER PRIMARY KEY,"
      "url LONGVARCHAR,"
      "title LONGVARCHAR,"
      "visit_count INTEGER DEFAULT 0 NOT NULL,"
      "typed_count INTEGER DEFAULT 0 NOT NULL,"
      "last_visit_time INTEGER NOT NULL,"
      "hidden INTEGER DEFAULT 0 NOT NULL,"
      "favicon_id INTEGER DEFAULT 0 NOT NULL)"); // favicon_id is not used now.

  return GetDB().Execute(sql.c_str());
}

bool URLDatabase::CreateMainURLIndex() {
  // Index over URLs so we can quickly look up based on URL.
  return GetDB().Execute(
      "CREATE INDEX IF NOT EXISTS urls_url_index ON urls (url)");
}

}  // namespace history
