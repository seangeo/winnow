// Copyright (c) 2007-2010 The Kaphan Foundation
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

#ifndef _CLUE_H_
#define _CLUE_H_

#include <Judy.h>

typedef struct CLUE {
  int token_id;
  double probability;
  double strength;
} Clue;

typedef struct CLUE_LIST {
  int size;
  Pvoid_t list;
} ClueList;

Clue * new_clue  (int token_id, double probability);
void         free_clue (Clue *clue);

ClueList * new_clue_list();
Clue *     add_clue(ClueList * clues, int token_id, double probability);
Clue *     get_clue(const ClueList * clues, int token_id);
void free_clue_list(ClueList * clues);

#define clue_token_id(clue)        clue->token_id
#define clue_probability(clue)     clue->probability
#define clue_strength(clue)        clue->strength

#endif /* _CLUE_H_ */

