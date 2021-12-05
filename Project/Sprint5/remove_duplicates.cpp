#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> ids_to_remove;
    for (auto it_id = search_server.begin(); it_id != search_server.end(); ++it_id) {
        auto words_freqs = search_server.GetWordFrequencies(*it_id);
        for (auto it_id_next = it_id + 1; it_id_next != search_server.end(); ++it_id_next) {
            auto words_freqs_next = search_server.GetWordFrequencies(*it_id_next);
            //compare 2 maps
            if (count(ids_to_remove.begin(), ids_to_remove.end(), *it_id_next) == 0
                && words_freqs.size() == words_freqs_next.size()
                && equal(words_freqs.begin(), words_freqs.end(), words_freqs_next.begin(),
                    [] (auto a, auto b) { return a.first == b.first; })) {
                ids_to_remove.push_back(*it_id_next);
            }
        }
    }
    sort(ids_to_remove.begin(), ids_to_remove.end());
    for (int id : ids_to_remove) {
        cout << "Found duplicate document id " << id << endl;
        search_server.RemoveDocument(id);
    }
}
