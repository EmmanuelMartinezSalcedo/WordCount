#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <vector>
#include <chrono>
#include <omp.h>

using namespace std;

string stem_word(string word) {
    if (word.length() <= 3) return word;
    transform(word.begin(), word.end(), word.begin(), ::tolower);
    vector<string> suffixes = {"ing", "ed", "ly", "ful", "est", "ity", "es", "s"};
    for (const string& suffix : suffixes) {
        if (word.length() > suffix.length() && word.substr(word.length() - suffix.length()) == suffix) {
            return word.substr(0, word.length() - suffix.length());
        }
    }
    return word;
}

int main() {
    const size_t BUFFER_SIZE = 1024 * 1024;
    char buffer[BUFFER_SIZE];
    unordered_map<string, int> global_word_count;
    vector<unordered_map<string, int>> thread_word_counts;
    
    omp_set_num_threads(12);

    ifstream file("large_text.txt", ios::in | ios::binary);
    if (!file) {
        cerr << "Error: Cannot open file" << endl;
        return 1;
    }

    auto start = chrono::high_resolution_clock::now();
    file.seekg(0, ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, ios::beg);

    thread_word_counts.resize(12);
    size_t total_words = 0;
    int lastPercentage = -1;

    #pragma omp parallel 
    {
        int tid = omp_get_thread_num();
        unordered_map<string, int>& local_word_count = thread_word_counts[tid];
        string current_word;
        size_t local_words = 0;
        string leftover_word;  // Store word from previous chunk
        
        #pragma omp for schedule(dynamic)
        for (size_t chunk_start = 0; chunk_start < fileSize; chunk_start += BUFFER_SIZE) {
            file.seekg(chunk_start);
            file.read(buffer, BUFFER_SIZE);
            size_t bytes_read = file.gcount();
            
            // Handle the first word of the chunk
            size_t start_pos = 0;
            if (chunk_start > 0) {
                // Find first space or newline
                while (start_pos < bytes_read && !isspace(buffer[start_pos])) {
                    start_pos++;
                }
                start_pos++; // Skip the space
            }
            
            // Process words in the chunk
            for (size_t i = start_pos; i < bytes_read; i++) {
                char c = buffer[i];
                if (isalpha(c)) {
                    current_word += c;
                } else if (isspace(c)) {
                    if (!current_word.empty()) {
                        string stemmed = stem_word(current_word);
                        local_word_count[stemmed]++;
                        local_words++;
                        current_word.clear();
                    }
                }
            }
            
            // Don't process the last incomplete word of the chunk
            if (chunk_start + bytes_read < fileSize && !current_word.empty()) {
                current_word.clear();
            }
            
            // Update progress
            #pragma omp atomic
            total_words += local_words;
            int currentPercentage = (chunk_start * 100) / fileSize;
            if (currentPercentage != lastPercentage) {
                #pragma omp critical
                {
                    cout << "\rProcessing: " << currentPercentage << "% (" << total_words << " words)" << flush;
                    lastPercentage = currentPercentage;
                }
            }
        }
    }
    
    for (const auto& thread_count : thread_word_counts) {
        for (const auto& entry : thread_count) {
            global_word_count[entry.first] += entry.second;
        }
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(end - start);

    cout << "\n\nResults:" << endl;
    cout << "Total time: " << duration.count() << " seconds" << endl;
    cout << "Total words processed: " << total_words << endl;
    cout << "Unique words (after stemming): " << global_word_count.size() << endl;

    vector<pair<string, int>> words(global_word_count.begin(), global_word_count.end());
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
