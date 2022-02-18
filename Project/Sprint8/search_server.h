#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <execution>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int CONCURRENT_BUCKET_COUNT = 10000;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) { }

    explicit SearchServer(const std::string_view stop_words_text)
        : SearchServer(SplitIntoWordsView(stop_words_text)) { }

    int GetDocumentCount() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy& policy, int document_id);

    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus input_status = DocumentStatus::ACTUAL) const;

    template <typename Comparator>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, Comparator comp) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, DocumentStatus input_status = DocumentStatus::ACTUAL) const;

    template <typename ExecutionPolicy, typename Comparator>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, Comparator comp) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy& policy, const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy& policy, const std::string_view raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status = DocumentStatus::ACTUAL;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus = false;
        bool is_stop = false;
    };

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

private:
    bool IsStopWord(const std::string_view word) const ;

    static bool IsValidWord(const std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view text) const;

    Query ParseQuery(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename Comparator>
    std::vector<Document> FindAllDocuments(const Query& query, Comparator comp) const;

    template <typename Comparator>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& policy, const Query& query, Comparator comp) const;

    template <typename Comparator>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy, const Query& query, Comparator comp) const;

private:
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<std::string, std::less<>> words_;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) {
    for (const std::string_view word : stop_words) {
        if (!SearchServer::IsValidWord(word)) {
            throw std::invalid_argument("Stop word contains invalid symbol");
        }

        if (!word.empty()) {
            stop_words_.insert((std::string)word);
        }
    }
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, DocumentStatus input_status) const {
    return SearchServer::FindTopDocuments(policy, raw_query,
        [input_status](int document_id, DocumentStatus status, int rating) {
            return status == input_status;
        });
}

template <typename Comparator>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, Comparator comp) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, comp);
}

template <typename ExecutionPolicy, typename Comparator>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, Comparator comp) const {
    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);
    auto matched_documents = SearchServer::FindAllDocuments(policy, query, comp);

    sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Comparator>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query, Comparator comp) const {
    return SearchServer::FindAllDocuments(std::execution::seq, query, comp);
}

template <typename Comparator>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy& policy, const SearchServer::Query& query, Comparator comp) const {
    std::map<int, double> document_to_relevance;

    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
        auto it = word_to_document_freqs_.find(word);

        for (const auto [document_id, term_freq] : it->second) {
            const auto &document_data = documents_.at(document_id);

            if (comp(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        auto it = word_to_document_freqs_.find(word);

        for (const auto [document_id, _] : it->second) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;

    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}

template <typename Comparator>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy& policy, const SearchServer::Query& query, Comparator comp) const {
    ConcurrentMap<int, double> document_to_relevance(CONCURRENT_BUCKET_COUNT);
    ConcurrentSet<int> id_docs_minus(CONCURRENT_BUCKET_COUNT);

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [this, &id_docs_minus](const std::string_view word) {
        if (word_to_document_freqs_.count(word) != 0) {
            auto it = word_to_document_freqs_.find(word);

            for (const auto [document_id, _] : it->second) {
                id_docs_minus.insert(document_id);
            }
        }
    });

    static constexpr int PART_COUNT = 10;
    const auto part_length = size(query.plus_words) / PART_COUNT;
    auto part_begin = query.plus_words.begin();
    auto part_end = std::next(part_begin, part_length);

    auto function = [this, &document_to_relevance, &id_docs_minus, &comp](const std::string_view word) {
        if (word_to_document_freqs_.count(word) != 0) {
            const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
            auto it = word_to_document_freqs_.find(word);

            for (const auto [document_id, term_freq] : it->second) {
                const auto &document_data = documents_.at(document_id);

                if (comp(document_id, document_data.status, document_data.rating) && id_docs_minus.count(document_id) == 0) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    };

    std::vector<std::future<void>> futures;

    for (int i = 0; i < PART_COUNT; ++i, part_begin = part_end, part_end = (i == PART_COUNT - 1 ? query.plus_words.end() : std::next(part_begin, part_length))) {
        futures.push_back(std::async([function, part_begin, part_end] {
            std::for_each(part_begin, part_end, function);
        }));
    }

    for (int i = 0; i < futures.size(); ++i) {
        futures[i].get();
    }

    auto build = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;

    for (const auto [document_id, relevance] : build) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}
