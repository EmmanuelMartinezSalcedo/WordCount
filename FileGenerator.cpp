#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <string>
#include <filesystem>
#include <sstream>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

const long long FILE_SIZE = 20LL * 1024 * 1024 * 1024; // 20GB
const int CHUNK_SIZE = 1024 * 1024;
const int DEFAULT_VOCAB_SIZE = 1000;

vector<string> load_dictionary(int vocab_size) {
  vector<string> dictionary;
  vector<string> file_paths;
  string folder = "Words";
  
  for (const auto& entry : fs::directory_iterator(folder)) {
    if (entry.path().extension() == ".txt") {
      file_paths.push_back(entry.path().string());
    }
  }

  if (file_paths.empty()) return dictionary;

  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<size_t> file_dist(0, file_paths.size() - 1);

  for (int i = 0; i < vocab_size; i += 5) {
    string selected_file = file_paths[file_dist(gen)];
    ifstream file(selected_file);
    
    if (!file) continue;

    file.seekg(0, ios::beg);
    size_t line_count = count(istreambuf_iterator<char>(file), istreambuf_iterator<char>(), '\n');
    
    if (line_count <= 4) continue;

    uniform_int_distribution<size_t> line_dist(2, line_count - 3);
    size_t target_line = line_dist(gen);

    file.seekg(0, ios::beg);
    string line;
    size_t current_line = 0;
    vector<string> lines;

    while (getline(file, line)) {
      if (current_line >= target_line - 2 && current_line <= target_line + 2) {
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (!line.empty()) {
          lines.push_back(line);
        }
      } else if (current_line > target_line + 2) {
        break;
      }
      current_line++;
    }

    if (lines.size() == 5) {
      dictionary.insert(dictionary.end(), lines.begin(), lines.end());
    }
  }

  shuffle(dictionary.begin(), dictionary.end(), gen);
  
  cout << "Loaded " << dictionary.size() << " words from dictionary files\n";
  return dictionary;
}

int main(int argc, char* argv[]) {
  int vocab_size = DEFAULT_VOCAB_SIZE;
  if (argc > 1) {
    vocab_size = atoi(argv[1]) * 5;
    if (vocab_size <= 0) {
      cerr << "Error: Vocabulary size must be positive\n";
      return 1;
    }
  }

  ofstream file("large_text.txt");
  if (!file) {
    cerr << "Error creating the file\n";
    return 1;
  }

  vector<string> dictionary = load_dictionary(vocab_size);
  if (dictionary.empty()) {
    cerr << "Error: No words loaded from dictionary\n";
    return 1;
  }

  vector<string> sorted_dict = dictionary;
  sort(sorted_dict.begin(), sorted_dict.end());

  cout << "\nSelected words:\n";
  for (size_t i = 0; i < sorted_dict.size(); ++i) {
    cout << sorted_dict[i];
    if (i < sorted_dict.size() - 1) cout << ", ";
    if ((i + 1) % 10 == 0) cout << "\n";
  }
  
  cout << "\n\nDo you want to continue with these words? (Y/n): ";
  string response;
  getline(cin, response);
  
  if (!response.empty() && tolower(response[0]) == 'n') {
    cout << "Operation cancelled by user\n";
    return 0;
  }

  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<int> word_dist(0, dictionary.size() - 1);

  long long written = 0;
  string chunk;
  chunk.reserve(CHUNK_SIZE);
  int lastPercentage = -1;
  const int barWidth = 50;

  while (written < FILE_SIZE) {
    for (int i = 0; i < 500; ++i) {
      chunk += dictionary[word_dist(gen)] + " ";
    }
    file << chunk;
    written += chunk.size();

    int currentPercentage = (written * 100) / FILE_SIZE;
    if (currentPercentage != lastPercentage) {
      float progress = (float)written / FILE_SIZE;
      int pos = barWidth * progress;
      cout << "\r[";
      for (int i = 0; i < barWidth; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
      }
      cout << "] " << currentPercentage << "%" << flush;
      lastPercentage = currentPercentage;
    }

    chunk.clear();
  }

  file.close();
  cout << "\nGenerated: large_text.txt (20GB)\n";
  cout << "\nUse: \"[System.Text.Encoding]::UTF8.GetString((Get-Content large_text.txt -Encoding Byte -TotalCount 1000))\"\n";
  cout << "To check a part of the file (Powershell)\n";
  return 0;
}