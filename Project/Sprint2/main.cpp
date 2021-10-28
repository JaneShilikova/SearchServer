#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

struct Document {
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL = 0,
    IRRELEVANT = 1,
    BANNED = 2,
    REMOVED = 3,
};

class SearchServer {
public:
    int GetDocumentCount() const {
        return documents_.size();
	}

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData {
                ComputeAverageRating(ratings),
                status
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus input_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query,
            [input_status](int document_id, DocumentStatus status, int rating) {
                return status == input_status;
            });
    }

    template <typename Comparator>
    vector<Document> FindTopDocuments(const string& raw_query, Comparator comp) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        //filtered - documents after comparator
        vector<Document> filtered_documents;
        for (auto elem : matched_documents)
            if(comp(elem.id, documents_.at(elem.id).status, elem.rating)) {
                filtered_documents.push_back(elem);
            }
        if (filtered_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            filtered_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return filtered_documents;
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status = DocumentStatus::ACTUAL;
    };

    struct QueryWord {
        string data;
        bool is_minus = false;
        bool is_stop = false;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

private:
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }

private:
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
};

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
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestAddingDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
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
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("-cat"s).empty(), "Founded documents mustn't contain minus words"s);
    }
}

void TestMatchingDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto words_status = server.MatchDocument("cat city"s, doc_id);
        auto [words, status] = words_status;
        ASSERT_EQUAL(words.size(), 2u);
        ASSERT(status == DocumentStatus::ACTUAL);
    }
    {
        SearchServer server;
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
    SearchServer server;
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

    SearchServer server;
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
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    const auto found_docs = server.FindTopDocuments("the"s,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id % 2 == 0;
            cout << static_cast<int>(status) << rating;
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
    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
    const auto found_docs = server.FindTopDocuments("the"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs[0].id, doc_id2);
}

void TestRelevanceDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
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

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}
