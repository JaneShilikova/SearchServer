#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> ids_to_remove;
    map<set<string_view>, int> existing_word_sets;

    for (auto document_id : search_server) {
        auto words_freqs = search_server.GetWordFrequencies(document_id);
        set<string_view> temp;
        transform(words_freqs.begin(), words_freqs.end(), inserter(temp, temp.begin()),
            [](const auto& elem) { return elem.first; });

        if (existing_word_sets.count(temp)) {
            ids_to_remove.push_back(document_id);
        }
        else {
            existing_word_sets.emplace(temp, document_id);
        }
    }

    for (int id : ids_to_remove) {
        cout << "Found duplicate document id " << id << endl;
        search_server.RemoveDocument(id);
    }
}
