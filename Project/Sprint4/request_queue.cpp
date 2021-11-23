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
