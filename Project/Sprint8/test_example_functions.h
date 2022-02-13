#pragma once

#include <iostream>
#include <map>
#include <set>
#include <string_view>
#include <vector>

// assert framework
template<typename T, typename U>
std::ostream& operator<<(std::ostream& out, const std::map<T, U> container) {
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
std::ostream& operator<<(std::ostream& out, const std::set<T> container) {
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
std::ostream& operator<<(std::ostream& out, const std::vector<T> container) {
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
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str,
    const std::string& file, const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "(" << line << "): " << func << ": ";
        std::cerr << "ASSERT_EQUAL(" << t_str << ", " << u_str << ") failed: ";
        std::cerr << t << " != " << u << ".";
            if (!hint.empty()) {
            	std::cerr << " Hint: " << hint;
            }
            std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
    unsigned line, const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(T func, const std::string& func_str) {
    func();
    std::cerr << func_str << " OK" << std::endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)
// end of assert framework
// module tests
void TestExcludeStopWordsFromAddedDocumentContent();

void TestAddingDocuments();

void TestMinusWordsFromAddedDocument();

void TestMatchingDocuments();

void TestSortDocuments();

void TestComputeAverageRating();

void TestFilterPredicate();

void TestDocumentsByStatus();

void TestRelevanceDocument();
// TestSearchServer - entry point for running module tests
void TestSearchServer();
// end of module tests
void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
