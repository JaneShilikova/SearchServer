#pragma once

#include <vector>
#include <string>
#include <string_view>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWordsView(const std::string_view& str);
