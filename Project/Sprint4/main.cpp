#include <algorithm>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "document.h"
#include "paginator.h"
#include "read_input_functions.h"
#include "request_queue.h"
#include "search_server.h"
#include "string_processing.h"

using namespace std;

// assert framework
template<typename T, typename U>
ostream& operator<<(ostream& out, const map<T, U> container) {
    out << "{";
    int i = 0;
    for (const auto& [key, value] : container) {
        out << key << ": " << value;
        if (i + 1 != container.size())
            out << ", ";
        ++i;
    }
    out << "}";
    return out;
}
template<typename T>
ostream& operator<<(ostream& out, const set<T> container) {
    out << "{";
    int i = 0;
    for (const auto& elem : container) {
        out << elem;
        if (i + 1 != container.size())
            out << ", ";
        ++i;
    }
    out << "}";
    return out;
}
template<typename T>
ostream& operator<<(ostream& out, const vector<T> container) {
    out << "[";
    int i = 0;
    for (const auto& elem : container) {
        out << elem;
        if (i + 1 != container.size())
            out << ", ";
        ++i;
    }
    out << "]";
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str,
    const string& file, const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
            if (!hint.empty()) {
                cerr << " Hint: "s << hint;
            }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func,
    unsigned line, const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(T func, const string& func_str) {
    func();
    cerr << func_str << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)
// end of assert framework
// module tests
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestAddingDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server(""s);
    ASSERT(server.GetDocumentCount() == 0);
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs.size(), 0u);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs2 = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs2.size(), 1u);
    const Document& doc0 = found_docs2[0];
    ASSERT_EQUAL(doc0.id, doc_id);
    ASSERT(server.GetDocumentCount() == 1);
}

void TestMinusWordsFromAddedDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("-cat"s).empty(), "Founded documents mustn't contain minus words"s);
    }
}

void TestMatchingDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto words_status = server.MatchDocument("cat city"s, doc_id);
        auto [words, status] = words_status;
        ASSERT_EQUAL(words.size(), 2u);
        ASSERT(status == DocumentStatus::ACTUAL);
    }
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(get<0>(server.MatchDocument("-cat"s, doc_id)).empty());
    }
}

void TestSortDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {2, 8, -3};

    const int doc_id2 = 41;
    const string content2 = "dog on the carpet"s;
    const vector<int> ratings2 = {3, 7, 2, 7};

    const int doc_id3 = 40;
    const string content3 = "sister in the kitchen"s;
    const vector<int> ratings3 = {4, 5, -12, 2, 1};
    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    const auto found_docs = server.FindTopDocuments("the"s);
    ASSERT_EQUAL(found_docs.size(), 3u);
    bool result = is_sorted(found_docs.begin(), found_docs.end(),
        [](const Document& doc1, const Document& doc2) {
    	    if (abs(doc1.relevance - doc2.relevance) < 1e-6) {
    	        return doc1.rating > doc2.rating;
            }
    	    else {
    	        return doc1.relevance > doc2.relevance;
    	    }
        });
    ASSERT_HINT(result, "Founded documents must be sorted by descending rating and relevance"s);
}

void TestComputeAverageRating() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {2, 8, -3};

    const int doc_id2 = 41;
    const string content2 = "dog on the carpet"s;
    const vector<int> ratings2 = {3, 7, 2, 7};

    const int doc_id3 = 40;
    const string content3 = "sister in the kitchen"s;
    const vector<int> ratings3 = {4, 5, -12, 2, 1};

    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    const auto found_docs = server.FindTopDocuments("the"s);
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    int rating = rating_sum / static_cast<int>(ratings.size());
    ASSERT(found_docs[1].rating == rating);

    rating_sum = 0;
    for (const int rating : ratings2) {
        rating_sum += rating;
    }
    rating = rating_sum / static_cast<int>(ratings2.size());
    ASSERT(found_docs[0].rating == rating);

    rating_sum = 0;
    for (const int rating : ratings3) {
        rating_sum += rating;
    }
    rating = rating_sum / static_cast<int>(ratings3.size());
    ASSERT(found_docs[2].rating == rating);
}

void TestFilterPredicate() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    const int doc_id2 = 41;
    const string content2 = "dog on the carpet"s;
    const vector<int> ratings2 = {1, -2, 3};
    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    const auto found_docs = server.FindTopDocuments("the"s,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id % 2 == 0;
        });
    ASSERT_EQUAL(found_docs[0].id, doc_id);
}

void TestDocumentsByStatus() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    const int doc_id2 = 41;
    const string content2 = "dog on the carpet"s;
    const vector<int> ratings2 = {1, -2, 3};
    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
    const auto found_docs = server.FindTopDocuments("the"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs[0].id, doc_id2);
}

void TestRelevanceDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("cat"s);

    const double inv_word_count = 1.0 / content.size();
    double freq = 0;
    vector<string> words = SplitIntoWords(content);
    for (const string& word : words)
        freq += inv_word_count;
    double relevance = freq * log(server.GetDocumentCount() * 1.0 / 1);
    ASSERT(found_docs[0].relevance == relevance);
}

// TestSearchServer - entry point for running module tests
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddingDocuments);
    RUN_TEST(TestMinusWordsFromAddedDocument);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortDocuments);
    RUN_TEST(TestComputeAverageRating);
    RUN_TEST(TestFilterPredicate);
    RUN_TEST(TestDocumentsByStatus);
    RUN_TEST(TestRelevanceDocument);
}
// end of module tests

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
}
