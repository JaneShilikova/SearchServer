#pragma once

#include <iostream>

struct Document {
    Document() = default;

    Document(int id_doc, double rel, int rat) : id(id_doc), relevance(rel), rating(rat) { }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

std::ostream& operator<<(std::ostream& out, const Document& document);

enum class DocumentStatus {
    ACTUAL = 0,
    IRRELEVANT = 1,
    BANNED = 2,
    REMOVED = 3,
};
