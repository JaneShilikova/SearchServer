#pragma once

#include <deque>
#include <vector>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : search_server_(search_server) { }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        QueryResult(std::string query, int docs) : raw_query(query), count_documents(docs) { }

        std::string raw_query;
        int count_documents = 0;
    };

    void Add_QueryResult(std::string query, int docs);

    void Remove_QueryResult();

    void Update(std::string query, std::vector<Document> docs);

private:
    std::deque<QueryResult> requests_;
    int no_result_requests = 0;
    const SearchServer& search_server_;
    static constexpr int minutes_per_day = 1440;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    auto res = search_server_.FindTopDocuments(raw_query, document_predicate);
    Update(raw_query, res);
    return res;
}
