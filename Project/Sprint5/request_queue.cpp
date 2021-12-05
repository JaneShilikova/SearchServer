#include "request_queue.h"

using namespace std;

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

int RequestQueue::GetNoResultRequests() const {
    return no_result_requests;
}

void RequestQueue::Add_QueryResult(string query, int count_docs) {
    if (count_docs == 0)
        ++no_result_requests;
    requests_.push_back(QueryResult(query, count_docs));
}

void RequestQueue::Remove_QueryResult() {
    if (requests_.size() > minutes_per_day) {
        if (requests_.front().count_documents == 0)
            --no_result_requests;
        requests_.pop_front();
    }
}

void RequestQueue::Update(string query, vector<Document> docs) {
    Add_QueryResult(query, docs.size());
    Remove_QueryResult();
}
