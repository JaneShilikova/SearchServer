#include "process_queries.h"

#include <algorithm>
#include <execution>

using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {
    vector<vector<Document>> res(queries.size());
    transform(execution::par, queries.begin(), queries.end(), res.begin(),
        [&search_server](string query) { return search_server.FindTopDocuments(query); });

    return res;
}

vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    auto temp = ProcessQueries(search_server, queries);
    vector<Document> res;
    size_t size = 0;
    for (auto const& items: temp){
        size += items.size();
    }
    res.reserve(size);
    for (auto& items: temp){
        move(items.begin(), items.end(), back_inserter(res));
    }
    return res;
}
