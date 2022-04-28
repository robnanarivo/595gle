#include "./WordIndex.h"

namespace searchserver {

WordIndex::WordIndex() {
  // TODO: implement
}

size_t WordIndex::num_words() {
  return word_to_doc_.size();
}

void WordIndex::record(const string& word, const string& doc_name) {
  if (word_to_doc_.find(word) == word_to_doc_.end()) {
    std::unordered_map<std::string, int> doc_to_count;
    doc_to_count[doc_name] = 1;
    word_to_doc_[word] = doc_to_count;
  } else {
    if (word_to_doc_[word].find(doc_name) == word_to_doc_[word].end()) {
      word_to_doc_[word][doc_name] = 1;
    } else {
      word_to_doc_[word][doc_name] += 1;
    }
  }
}

list<Result> WordIndex::lookup_word(const string& word) {
  list<Result> result;
  for (auto &it : word_to_doc_[word]) {
    result.push_back(Result(it.first, it.second));
  }
  result.sort();
  return result;
}

list<Result> WordIndex::lookup_query(const vector<string>& query) {
  list<Result> results;
  
  if (query.size() == 0) {
    return results;
  }

  if (word_to_doc_.find(query[0]) == word_to_doc_.end()) {
    return results;
  }
  
  vector<std::string> docs_to_check;
  docs_to_check.reserve(word_to_doc_[query[0]].size());
  for (auto &it : word_to_doc_[query[0]]) {
    docs_to_check.push_back(it.first);
  }

  for (string& doc : docs_to_check) {
    int count = 0;
    bool contains_all = true;
    for (int i = 0; i < query.size(); i++) {
      if (word_to_doc_[query[i]].find(doc) == word_to_doc_[query[i]].end()) {
        contains_all = false;
        break;
      } else {
        count += word_to_doc_[query[i]][doc];
      }
    }
    if (contains_all) {
      results.push_back(Result(doc, count));
    }
  }

  results.sort();

  return results;
}

}  // namespace searchserver
