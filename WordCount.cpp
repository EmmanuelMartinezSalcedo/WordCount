#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <vector>
#include <chrono>

using namespace std;

string stem_word(string word) {
  if (word.length() <= 3) return word;
  
  transform(word.begin(), word.end(), word.begin(), ::tolower);
  
  vector<string> suffixes = {"ing", "ed", "ly", "ful", "est", "ity", "es", "s"};
  
  for (const string& suffix : suffixes) {
    if (word.length() > suffix.length() && 
      word.substr(word.length()-suffix.length()) == suffix) {
      return word.substr(0, word.length()-suffix.length());
    }
  }
  
  return word;
}

int main() {
  const size_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
  char buffer[BUFFER_SIZE];
  unordered_map<string, int> word_count;
  string current_word;
  
  ifstream file("large_text.txt");
  if (!file) {
    cerr << "Error: Cannot open file" << endl;
    return 1;
  }

  auto start = chrono::high_resolution_clock::now();
  size_t total_words = 0;
  int lastPercentage = -1;

  file.seekg(0, ios::end);
  size_t fileSize = file.tellg();
  file.seekg(0, ios::beg);

  while (file.read(buffer, BUFFER_SIZE)) {
    size_t bytes_read = file.gcount();
    
    for (size_t i = 0; i < bytes_read; i++) {
      char c = buffer[i];
      
      if (isalpha(c)) {
        current_word += c;
      }
      else if (!current_word.empty()) {
        string stemmed = stem_word(current_word);
        word_count[stemmed]++;
        total_words++;
        current_word.clear();

        int currentPercentage = (file.tellg() * 100) / fileSize;
        if (currentPercentage != lastPercentage) {
          cout << "\rProcessing: " << currentPercentage << "% (" << total_words << " words)" << flush;
          lastPercentage = currentPercentage;
        }
      }
    }
  }

  if (!current_word.empty()) {
    string stemmed = stem_word(current_word);
    word_count[stemmed]++;
    total_words++;
  }

  auto end = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::seconds>(end - start);

  cout << "\n\nResults:" << endl;
  cout << "Total time: " << duration.count() << " seconds" << endl;
  cout << "Total words processed: " << total_words << endl;
  cout << "Unique words (after stemming): " << word_count.size() << endl;

  vector<pair<string, int>> words(word_count.begin(), word_count.end());
  sort(words.begin(), words.end(), [](const pair<string, int>& a, const pair<string, int>& b) {
    return a.second > b.second;
  });

  cout << "\nWord frequencies after stemming:" << endl;
  cout << "Word\t\tFrequency" << endl;
  cout << "------------------------" << endl;
  for (const auto& [word, count] : words) {
    cout << word;
    cout << string(max(16 - static_cast<int>(word.length()), 1), ' ');
    cout << count << endl;
  }

  return 0;
}