#include "./WordIndex.h"

namespace searchserver {

WordIndex::WordIndex() {
  // TODO: implement
}

size_t WordIndex::num_words() {
  // TODO: implement
  return 0U;
}

void WordIndex::record(const string& word, const string& doc_name) {
  // TODO: implement
}

list<Result> WordIndex::lookup_word(const string& word) {
  list<Result> result;
  // TODO: implement
  return result;
}

list<Result> WordIndex::lookup_query(const vector<string>& query) {
  list<Result> results;

  // TODO: implement
  return results;
}

}  // namespace searchserver
