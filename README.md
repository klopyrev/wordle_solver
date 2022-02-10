# Overview

The attached C++ binary solves Wordle optimally. The best starting word is
`plate`!

A few other really good words: tripe, scale, train, place, stole, spilt, lapse,
parse, pearl and close.

This program will always win, and guarantees the smallest average number of
guesses if you play all 2315 possible games.

It computes the next word to play, given any guesses already made. The word it
returns minimizes the expected number of guesses you would need to make,
assuming each of the 2315 possible words is equally probable.

Note that it only ever guesses words that are still possible, given all the
previous guesses. It's conceivable that there is a better strategy that sometimes
plays words that are no longer possible.

# Usage

To compile:

`g++ solver.cc -o solver -std=c++17 -pthread -O3`

Run the binary without any arguments to compute the best starting word.

To specify guesses, enter each guess in series along with the response encoded
as a 5 character string, where:

* `_` = no match
* `y` = correct letter, wrong position
* `g` = correct letter, correct position.

Example run:

`./solver plate __g_g shame y_g_g`

In the above run:

* `plate` is the first guess, and the 3rd and 5th letters are correct.
* `shame` is the second guess, the 3rd and 5th letters are correct, and the 1st
  letter is correct but in the wrong position.

The output is:

<pre>
Saving results in: result3.txt
Computation is done. Play the word: cease
</pre>

Note the computation of the starting word takes more than 2 hours. The
computation given any guesses only takes a few seconds.

# Results for First Word

The file `result1.txt` contains all guesses for the first word that guarantee a
win. They are sorted by the expected number of guesses you would have to make
if you guess that word first. The expected value includes that first guess as
well.

# Algorithm Description

The algorithm is a recursive brute force algorithm. At each step of the
recursion, it has a list of words that are still possible. It tries all next
words, and chooses the next word that minimizes the expected number of
guesses needed to win.

The expected value is computed as follows. For each guess, there is a list of
possible responses (e.g. the response gray-yellow-green-gray-gray). The
probability of a given response is given by the number of words that match that
response divided by the number of words left at the given recursion depth. This
comes from the fact that each word is equally likely. The expected number of
guesses left given that next guess and the response is computed recursively. The
expected value for a given guess is then the sum over all the responses of the
probability of the response multiplied by the expected number of guesses left
given that response. At each step of the recursion, the word with the minimum
expected value is chosen.

Many things are done to implement this recursion efficiently enough so that it
runs in a reasonable amount of time:

* All words that are possible after a given guess and a given response pattern
(of 243 patterns) are precomputed. The results are stored in a boolean array
with an entry for each of the 2315 words. This data structure makes it possible
to efficiently compute the words left given a list of words, a guess and a response
pattern.
* The computation runs in parallel using 24 threads, where each thread tries a
different set of first words. This setup is optimal for my machine since I have
12 CPU cores with hyperthreading.
* All storage needed is preallocated ensuring no memory allocations are needed
inside the recursion. Each thread allocates a vector to store the list of words
left at all possible recusion depths.
* If any of the response patterns for a given guess results in no win, the
computation is aborted early and that guess is skipped.

# Acknowledgements

I would like to thank **3Blue1Brown** for the video [Solving Wordle using
information theory](https://www.youtube.com/watch?v=v68zYyaEmEA). It gave me
some ideas that I used for the solution.
