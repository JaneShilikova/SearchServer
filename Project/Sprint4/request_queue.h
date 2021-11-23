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
        QueryResult(bool res) : no_result(res) { }

        bool no_result = false;
    };

    std::deque<QueryResult> requests_;
    int no_result_requests = 0;
    const SearchServer& search_server_;
    int curr_minutes = 0;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    ++curr_minutes;
    if (curr_minutes == 1441) {
        if (requests_.front().no_result)
            --no_result_requests;
        requests_.pop_front();
        --curr_minutes;
    }
    auto res = search_server_.FindTopDocuments(raw_query, document_predicate);
    bool flag = res.empty();
    if (flag)
        ++no_result_requests;
    requests_.push_back(QueryResult(flag));
    return res;
}
