#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>

using namespace std;

const size_t CHUNK_SIZE = 1024 * 1024 * 16; // 16MB chunks
const int MAX_WORD_LENGTH = 50;
const int NUM_STREAMS = 48;

__device__ bool d_isspace(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

__device__ char d_tolower(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c + ('a' - 'A');
  }
  return c;
}

__device__ int d_strlen(const char *str) {
  int len = 0;
  while (str[len] != '\0') {
    len++;
  }
  return len;
}

__device__ bool d_strncmp(const char *s1, const char *s2, int n) {
  for (int i = 0; i < n; i++) {
    if (s1[i] != s2[i]) {
      return false;
    }
    if (s1[i] == '\0') {
      return true;
    }
  }
  return true;
}

__global__ void processWordsKernel(char *chunk, size_t chunkSize, char *words, int *wordLengths, int *wordCount, bool isFirstChunk) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= chunkSize) {
    return;
  }

  __shared__ char sharedChunk[1024];
  __shared__ bool isWordStart[1024];

  if (threadIdx.x < 1024 && idx < chunkSize) {
    sharedChunk[threadIdx.x] = chunk[idx];
    isWordStart[threadIdx.x] = (idx == 0) || d_isspace(chunk[idx - 1]);
  }
  __syncthreads();

  if (!isFirstChunk && idx == 0) {
    return;
  }

  if (isWordStart[threadIdx.x] && !d_isspace(sharedChunk[threadIdx.x])) {
    char word[MAX_WORD_LENGTH];
    int length = 0;

    while (idx + length < chunkSize && length < MAX_WORD_LENGTH - 1 && !d_isspace(chunk[idx + length])) {
      word[length] = d_tolower(chunk[idx + length]);
      length++;
    }

    if (idx + length >= chunkSize) {
      return;
    }

    word[length] = '\0';

    if (length > 3) {
      const char *suffixes[] = {"ing", "ed", "ly", "ful", "est", "ity", "es", "s"};
      for (const char *suffix : suffixes) {
        int suffixLen = d_strlen(suffix);
        if (length > suffixLen + 1 && d_strncmp(&word[length - suffixLen], suffix, suffixLen)) {
          length -= suffixLen;
          word[length] = '\0';
          break;
        }
      }
    }

    if (length > 0) {
      int wordIdx = atomicAdd(wordCount, 1);
      memcpy(&words[wordIdx * MAX_WORD_LENGTH], word, length + 1);
      wordLengths[wordIdx] = length;
    }
  }
}

int main() {
  auto start_time = chrono::high_resolution_clock::now();

  cudaStream_t streams[NUM_STREAMS];
  for (int i = 0; i < NUM_STREAMS; i++) {
    cudaStreamCreate(&streams[i]);
  }

  ifstream file("large_text.txt", ios::binary);
  if (!file) {
    cerr << "Error opening file\n";
    return 1;
  }

  file.seekg(0, ios::end);
  size_t fileSize = file.tellg();
  file.seekg(0, ios::beg);

  char *h_chunk, *d_chunk;
  char *d_words;
  int *d_wordLengths, *d_wordCount;

  cudaMallocHost(&h_chunk, CHUNK_SIZE);
  cudaMalloc(&d_chunk, CHUNK_SIZE);
  cudaMalloc(&d_words, CHUNK_SIZE * MAX_WORD_LENGTH);
  cudaMalloc(&d_wordLengths, CHUNK_SIZE * sizeof(int));
  cudaMalloc(&d_wordCount, sizeof(int));

  unordered_map<string, int> wordFrequency;
  int currentStream = 0;

  float process_time = 0;

  for (size_t offset = 0; offset < fileSize; offset += CHUNK_SIZE) {
    auto chunk_start = chrono::high_resolution_clock::now();

    size_t currentChunkSize = min(CHUNK_SIZE, fileSize - offset);

    file.read(h_chunk, currentChunkSize);

    cudaMemcpyAsync(d_chunk, h_chunk, currentChunkSize, cudaMemcpyHostToDevice, streams[currentStream]);

    cudaMemsetAsync(d_wordCount, 0, sizeof(int), streams[currentStream]);

    int blockSize = 256;
    int numBlocks = (currentChunkSize + blockSize - 1) / blockSize;

    processWordsKernel<<<numBlocks, blockSize, 0, streams[currentStream]>>>(d_chunk, currentChunkSize, d_words, d_wordLengths, d_wordCount, offset == 0);

    int h_wordCount;
    cudaMemcpyAsync(&h_wordCount, d_wordCount, sizeof(int), cudaMemcpyDeviceToHost, streams[currentStream]);

    cudaStreamSynchronize(streams[currentStream]);

    char *h_words = new char[h_wordCount * MAX_WORD_LENGTH];
    int *h_wordLengths = new int[h_wordCount];

    cudaMemcpy(h_words, d_words, h_wordCount * MAX_WORD_LENGTH, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_wordLengths, d_wordLengths, h_wordCount * sizeof(int), cudaMemcpyDeviceToHost);

    for (int i = 0; i < h_wordCount; i++) {
      string word(&h_words[i * MAX_WORD_LENGTH]);
      wordFrequency[word]++;
    }

    delete[] h_words;
    delete[] h_wordLengths;

    currentStream = (currentStream + 1) % NUM_STREAMS;

    auto chunk_end = chrono::high_resolution_clock::now();
    process_time += chrono::duration<float>(chunk_end - chunk_start).count();

    float progress = (float)offset / fileSize * 100;
    cout << "\rProgress: " << progress << "%" << flush;
  }

  vector<pair<string, int>> wordList;
  for (const auto &pair : wordFrequency) {
    wordList.push_back(pair);
  }

  sort(wordList.begin(), wordList.end(), [](const auto &a, const auto &b) {
    return a.second > b.second;
  });

  auto end_time = chrono::high_resolution_clock::now();
  float total_time = chrono::duration<float>(end_time - start_time).count();

  cout << "\n\nPerformance Statistics:\n";
  cout << "----------------------\n";
  cout << "Total time: " << total_time << " seconds\n";

  cout << "\n\nWord frequencies:\n";
  cout << "Word\t\tCount\n";
  cout << "-------------------\n";
  for (const auto &pair : wordList) {
    cout << pair.first << "\t\t" << pair.second << "\n";
  }

  cudaFreeHost(h_chunk);
  cudaFree(d_chunk);
  cudaFree(d_words);
  cudaFree(d_wordLengths);
  cudaFree(d_wordCount);

  for (int i = 0; i < NUM_STREAMS; i++) {
    cudaStreamDestroy(streams[i]);
  }

  return 0;
}

// Compile: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
//          nvcc -O3 -arch=sm_89 -std=c++17 WordCountCU.cu -o WordCountCU.exe
// Run:     WordCountCU.exe