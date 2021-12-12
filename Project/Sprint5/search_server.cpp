#include "search_server.h"

using namespace std;

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> no_documents_;
    if (document_to_word_freqs_.count(document_id))
        return document_to_word_freqs_.at(document_id);
    else
        return no_documents_;
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Document id is less than zero or is used"s);
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
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
    auto it = word_to_document_freqs_.begin();
    while (it != word_to_document_freqs_.end()) {
        map<int, double> doc_freqs = it->second;
        if (doc_freqs.count(document_id)) {
            doc_freqs.erase(document_id);
            it = word_to_document_freqs_.erase(it);
        }
        else
            ++it;
    }
    //remove from document_to_word_freqs_
    if (document_to_word_freqs_.count(document_id)) {
    	map<string, double> word_freqs = document_to_word_freqs_.at(document_id);
        word_freqs.erase(word_freqs.begin(), word_freqs.end());
        document_to_word_freqs_.erase(document_id);
    }
    //remove from documents_
    documents_.erase(document_id);
    //remove from document_ids_
    document_ids_.erase(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus input_status) const {
    return SearchServer::FindTopDocuments(raw_query,
        [input_status](int document_id, DocumentStatus status, int rating) {
            return status == input_status;
        });
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
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

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    SearchServer::Query query;
    for (const string& word : SplitIntoWords(text)) {
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
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query) const {
    map<int, double> document_to_relevance;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            document_to_relevance[document_id] += term_freq * inverse_document_freq;
        }
    }

    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}
