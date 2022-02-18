#include "search_server.h"

using namespace std;

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> no_documents_;

    if (document_to_word_freqs_.count(document_id)) {
        auto it = document_to_word_freqs_.find(document_id);
        return it->second;
    }
    else {
        return no_documents_;
    }
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Document id is less than zero or is used"s);
    }

    const vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for (const string_view word : words) {
        words_.emplace(word);
        auto it = words_.find(word);
        word_to_document_freqs_[*it][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][*it] += inv_word_count;
    }

    document_ids_.insert(document_id);
    documents_.emplace(document_id,
        SearchServer::DocumentData {
        SearchServer::ComputeAverageRating(ratings),
        status
    });
}

void SearchServer::RemoveDocument(int document_id) {
    //remove from word_to_document_freqs_
    auto it = document_to_word_freqs_.find(document_id);
    for (auto& [word, freq] : it->second) {
        word_to_document_freqs_[word].erase(document_id);
    }

    //remove from document_to_word_freqs_
    document_to_word_freqs_.erase(document_id);

    //remove from documents_
    documents_.erase(document_id);

    //remove from document_ids_
    document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy& policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const execution::parallel_policy& policy, int document_id) {
    //remove from word_to_document_freqs_
    auto it = document_to_word_freqs_.find(document_id);
    std::map<std::string_view, double> cur_word_freqs = it->second;
    for_each(policy, cur_word_freqs.begin(), cur_word_freqs.end(),
        [this, document_id](auto& word_freq) { word_to_document_freqs_[word_freq.first].erase(document_id); });

    //remove from document_to_word_freqs_
    document_to_word_freqs_.erase(document_id);

    //remove from documents_
    documents_.erase(document_id);

    //remove from document_ids_
    document_ids_.erase(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus input_status) const {
    return SearchServer::FindTopDocuments(raw_query,
        [input_status](int document_id, DocumentStatus status, int rating) {
            return status == input_status;
        });
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);
    vector<string_view> matched_words;

    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        auto it = word_to_document_freqs_.find(word);

        if (it->second.count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        auto it = word_to_document_freqs_.find(word);

        if (it->second.count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy& policy, const string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy& policy, const string_view raw_query, int document_id) const {
    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);

    const auto condition = [this, document_id](const string_view word) {
        auto it = word_to_document_freqs_.find(word);
        return (word_to_document_freqs_.count(word) != 0 &&
            it->second.count(document_id));
    };

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), condition)) {
        return {{}, documents_.at(document_id).status};
    }

    vector<string_view> matched_words;

    copy_if(policy, query.plus_words.begin(), query.plus_words.end(), back_inserter(matched_words), condition);

    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;

    for (const string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Invalid symbol"s);
        }

        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }

    int rating_sum = 0;

    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("No word"s);
    }

    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }

    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("No word after minus, more than 1 minus or invalid symbol"s);
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQuery(const string_view text) const {
    SearchServer::Query query;

    for (const string_view word : SplitIntoWordsView(text)) {
        const SearchServer::QueryWord query_word = SearchServer::ParseQueryWord(word);

        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    auto it = word_to_document_freqs_.find(word);
    return log(SearchServer::GetDocumentCount() * 1.0 / it->second.size());
}
