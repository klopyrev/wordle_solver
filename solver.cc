/*
 * Copyright (c) 2022 Konstantin Lopyrev.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

std::vector<std::string> ReadWords() {
  std::ifstream fin("wordle-answers-alphabetical.txt");
  std::vector<std::string> words;
  while (true) {
    std::string word;
    if (!(fin >> word)) {
      break;
    }
    words.push_back(word);
  }
  return words;
}

// Computes a 3D vector of size N * 3^5 * N where N is the number of words.
//
// Entry all_word_pattern_matches[i][pattern][j] indicates that if word i is
// played next and the response is the given pattern, whether the word j is
// still a possible answer.
//
// Patterns are encoded an int in range [0, 3^5), for all possible patterns.
std::vector<std::vector<std::vector<bool>>> ComputeWordPatternMatches(
    const std::vector<std::string>& words) {
  static constexpr int kNumPatterns = 243;

  int shift[5];
  shift[0] = 1;
  for (int c = 1; c < 5; ++c) {
    shift[c] = shift[c - 1] * 3;
  }

  std::vector<std::vector<std::vector<bool>>> all_word_pattern_matches(
      words.size(), std::vector<std::vector<bool>>(
                        kNumPatterns, std::vector<bool>(words.size(), false)));
  std::vector<int> letters_left(26, 0);
  for (int i = 0; i < words.size(); ++i) {
    for (int j = 0; j < words.size(); ++j) {
      const std::string& guess = words[i];
      const std::string& answer = words[j];

      int pattern = 0;
      for (int c = 0; c < 5; ++c) {
        if (guess[c] == answer[c]) {
          pattern += shift[c] * 2;
        } else {
          letters_left[answer[c] - 'a'] += 1;
        }
      }
      for (int c = 0; c < 5; ++c) {
        if (guess[c] != answer[c]) {
          if (letters_left[guess[c] - 'a'] > 0) {
            pattern += shift[c] * 1;
            letters_left[guess[c] - 'a'] -= 1;
          }
        }
      }
      for (int c = 0; c < 5; ++c) {
        letters_left[answer[c] - 'a'] = 0;
      }

      all_word_pattern_matches[i][pattern][j] = true;
    }
  }
  return all_word_pattern_matches;
}

// Converts the given pattern in string format to an int.
//
//   _ = no match
//   y = correct letter wrong position
//   g = correct letter correct position.
int ToPatternInt(const std::string& pattern) {
  assert(pattern.size() == 5);
  int result = 0;
  int shift = 1;
  for (int i = 0; i < pattern.size(); ++i) {
    if (pattern[i] == 'g') {
      result += shift * 2;
    } else if (pattern[i] == 'y') {
      result += shift * 1;
    } else {
      assert(pattern[i] == '_');
    }
    shift *= 3;
  }
  return result;
}

// Trims the word patterns vector to remove patterns that have no matches. This
// is done for efficiency reasons so we avoid looking at these patterns during
// the recursion.
std::vector<std::vector<std::vector<bool>>> TrimWordPatternMatches(
    const std::vector<std::vector<std::vector<bool>>>&
        all_word_pattern_matches) {
  std::vector<std::vector<std::vector<bool>>> word_pattern_matches(
      all_word_pattern_matches.size());
  for (int i = 0; i < all_word_pattern_matches.size(); ++i) {
    for (const std::vector<bool>& pattern : all_word_pattern_matches[i]) {
      int num_matches = 0;
      for (bool val : pattern) {
        if (val) {
          ++num_matches;
        }
      }
      if (num_matches > 0) {
        word_pattern_matches[i].push_back(pattern);
      }
    }
  }
  return word_pattern_matches;
}

// Computes the expected number of guesses you need to make to win, once
// num_guesses guesses have already been made.
std::optional<float> Recurse(
    int num_guesses, int max_num_guesses,
    std::vector<std::vector<int>>& all_words_left, int num_words_left,
    const std::vector<std::vector<std::vector<bool>>>& word_pattern_matches) {
  if (num_guesses == max_num_guesses) {
    // You can't solve the puzzle.
    return std::nullopt;
  }
  if (num_words_left == 1) {
    // With only a single word left, solve right away.
    return 1;
  }
  // These are the current words left.
  const std::vector<int>& words_left = all_words_left[num_guesses - 1];
  // This is the storage location for words left after the next guess.
  std::vector<int>& next_words_left = all_words_left[num_guesses];
  std::optional<float> min_expected;
  for (int next = 0; next < num_words_left; ++next) {
    int next_word = words_left[next];
    float result = 0;
    bool valid = true;
    for (const std::vector<bool>& pattern : word_pattern_matches[next_word]) {
      int new_num_words_left = 0;
      for (int i = 0; i < num_words_left; ++i) {
        int word = words_left[i];
        if (pattern[word]) {
          next_words_left[new_num_words_left] = word;
          ++new_num_words_left;
        }
      }
      if (new_num_words_left > 0) {
        float expected;
        if (next_words_left[0] == next_word) {
          // Correct guess.
          expected = 1;
        } else {
          std::optional<float> next_result =
              Recurse(num_guesses + 1, max_num_guesses, all_words_left,
                      new_num_words_left, word_pattern_matches);
          if (!next_result) {
            // If you play this word as the next word, it's not possible to
            // always solve the puzzle.
            valid = false;
            break;
          }
          expected = 1 + *next_result;
        }
        result += new_num_words_left * expected;
      }
    }
    if (valid) {
      result /= num_words_left;

      if (!min_expected || result < *min_expected) {
        min_expected = result;
      }
    }
  }
  return min_expected;
}

// Tries every first guess in the range [start, end).
void Thread(
    int start, int end, int max_num_guesses,
    const std::vector<std::string>& words,
    const std::vector<std::vector<std::vector<bool>>>& word_pattern_matches,
    std::mutex& mu, std::ofstream& fout, std::optional<float>& min_expected,
    int& best_word) {
  // all_words_left is thread-local storage space to store the words remaining
  // at a given recursion depth.
  std::vector<std::vector<int>> all_words_left(max_num_guesses,
                                               std::vector<int>(words.size()));
  // This is the storage location for words left after the first guess.
  std::vector<int>& next_words_left = all_words_left[0];
  for (int first_word = start; first_word < end; ++first_word) {
    float result = 0;
    bool valid = true;
    for (const std::vector<bool>& pattern : word_pattern_matches[first_word]) {
      int new_num_words_left = 0;
      for (int word = 0; word < words.size(); ++word) {
        if (pattern[word]) {
          next_words_left[new_num_words_left] = word;
          ++new_num_words_left;
        }
      }
      float expected;
      if (next_words_left[0] == first_word) {
        // Correct guess.
        expected = 1;
      } else {
        std::optional<float> next_result =
            Recurse(/*num_guesses=*/1, max_num_guesses, all_words_left,
                    new_num_words_left, word_pattern_matches);
        if (!next_result) {
          // If you play this word as the first word, it's not possible to
          // always solve the puzzle.
          valid = false;
          break;
        }
        expected = 1 + *next_result;
      }
      result += new_num_words_left * expected;
    }
    if (valid) {
      result /= words.size();

      std::lock_guard<std::mutex> guard(mu);
      fout << result << " " << words[first_word] << std::endl;
      fout.flush();

      if (!min_expected || result < *min_expected ||
          (result == *min_expected && first_word < best_word)) {
        min_expected = result;
        best_word = first_word;
      }
    }
  }
}

// Sorts all the results in the given file in increased order of expected value.
void SortResults(const std::string& filename) {
  std::ifstream fin(filename);
  std::vector<std::pair<float, std::string>> results;
  while (true) {
    float expected;
    std::string word;
    if (!(fin >> expected >> word)) {
      break;
    }
    results.emplace_back(expected, word);
  }
  fin.close();
  std::sort(results.begin(), results.end());
  std::ofstream fout(filename);
  for (const std::pair<float, std::string>& result : results) {
    fout << result.first << " " << result.second << std::endl;
  }
}

int main(int args, char* argv[]) {
  static constexpr int kMaxNumGuesses = 6;
  static constexpr int kNumThreads = 24;

  int max_num_guesses = kMaxNumGuesses;

  std::vector<std::string> words = ReadWords();
  std::vector<std::vector<std::vector<bool>>> all_word_pattern_matches =
      ComputeWordPatternMatches(words);

  // Applies any guesses already made by prunning the list of words.
  assert((args - 1) % 2 == 0);
  for (int guess_i = 0; guess_i < (args - 1) / 2; ++guess_i) {
    std::string guess = argv[1 + guess_i * 2];
    std::string pattern = argv[1 + guess_i * 2 + 1];

    int word_i = std::find(words.begin(), words.end(), guess) - words.begin();
    assert(word_i != words.size());

    int pattern_int = ToPatternInt(pattern);
    const std::vector<bool>& matches =
        all_word_pattern_matches[word_i][pattern_int];

    std::vector<std::string> new_words;
    for (int i = 0; i < words.size(); ++i) {
      if (matches[i]) {
        new_words.push_back(words[i]);
      }
    }

    words = new_words;
    all_word_pattern_matches = ComputeWordPatternMatches(words);
    --max_num_guesses;
    assert(!words.empty());
  }
  assert(max_num_guesses > 0);

  std::vector<std::vector<std::vector<bool>>> word_pattern_matches =
      TrimWordPatternMatches(all_word_pattern_matches);

  std::vector<std::unique_ptr<std::thread>> threads;
  std::mutex mu;
  std::stringstream sout;
  sout << "result" << (kMaxNumGuesses - max_num_guesses + 1) << ".txt";
  std::ofstream fout(sout.str());
  std::cout << "Saving results in: " << sout.str() << std::endl;
  std::optional<float> min_expected;
  int best_word;
  for (int thread = 0; thread < kNumThreads; ++thread) {
    int start = words.size() * thread / kNumThreads;
    int end = words.size() * (thread + 1) / kNumThreads;
    threads.emplace_back(new std::thread([start, end, max_num_guesses, &words,
                                          &word_pattern_matches, &mu, &fout,
                                          &min_expected, &best_word]() {
      Thread(start, end, max_num_guesses, words, word_pattern_matches, mu, fout,
             min_expected, best_word);
    }));
  }
  for (const auto& thread : threads) {
    thread->join();
  }
  std::cout << "Computation is done. ";
  if (!min_expected) {
    std::cout << "You can't win!" << std::endl;
  } else {
    std::cout << "Play the word: " << words[best_word] << std::endl;
  }
  fout.close();

  SortResults(sout.str());
}
