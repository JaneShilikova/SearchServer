#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "document.h"
#include "log_duration.h"
#include "string_processing.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) { }

    int GetDocumentCount() const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    std::vector<int>::const_iterator begin() const;

    std::vector<int>::const_iterator end() const;

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    void RemoveDocument(int document_id);

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus input_status = DocumentStatus::ACTUAL) const;

    template <typename Comparator>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Comparator comp) const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status = DocumentStatus::ACTUAL;
    };

    struct QueryWord {
        std::string data;
        bool is_minus = false;
        bool is_stop = false;
    };

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

private:
    bool IsStopWord(const std::string& word) const ;

    static bool IsValidWord(const std::string& word);

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string text) const;

    Query ParseQuery(const std::string& text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    std::vector<Document> FindAllDocuments(const Query& query) const;

private:
    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;
    std::map<int, std::map<std::string, double>> document_to_word_freqs_;
    std::map<std::string, double> no_documents_;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) {
    for (const std::string& word : stop_words) {
        if (!SearchServer::IsValidWord(word)) {
            throw std::invalid_argument("Stop word contains invalid symbol");
        }
        if (!word.empty()) {
            stop_words_.insert(word);
        }
    }
}

template <typename Comparator>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, Comparator comp) const {
    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);
    auto matched_documents = SearchServer::FindAllDocuments(query);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    //filtered - documents after comparator
    std::vector<Document> filtered_documents;
    for (Document elem : matched_documents)
        if(comp(elem.id, documents_.at(elem.id).status, elem.rating)) {
            filtered_documents.push_back(elem);
        }
    if (filtered_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        filtered_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return filtered_documents;
}
