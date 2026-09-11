/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"

/* Basic utilities */

char *new_string(char *str) {
    return strcpy((char *)malloc(strlen(str) + 1), str);
}

void init_words(WordCount **wclist) {
    /* Initialize word count. */
    *wclist = NULL;
}

size_t len_words(WordCount *wchead) {
    size_t len = 0;

    WordCount *current = wchead;

    while (current != NULL) {
        len++;
        current = current->next;
    }

    return len;
}

WordCount *find_word(WordCount *wchead, char *word) {
    /* Return the WordCount node if the word exists. */

    WordCount *current = wchead;

    while (current != NULL) {

        if (strcmp(current->word, word) == 0) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

void add_word(WordCount **wclist, char *word) {
    /*
     * If word is already present, increment its count.
     * Otherwise create a new WordCount node with count 1.
     */

    WordCount *existing = find_word(*wclist, word);

    if (existing != NULL) {
        existing->count++;
        return;
    }

    WordCount *new_node = malloc(sizeof(WordCount));

    if (new_node == NULL) {
        perror("malloc");
        return;
    }

    /*
     * Make a copy of the word because the buffer used by
     * count_words() will be reused for later words.
     */
    new_node->word = new_string(word);
    new_node->count = 1;

    /*
     * Insert the new node at the beginning of the linked list.
     */
    new_node->next = *wclist;
    *wclist = new_node;
}

void fprint_words(WordCount *wchead, FILE *ofile) {
    /* Print word counts to a file. */

    WordCount *wc;

    for (wc = wchead; wc; wc = wc->next) {
        fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
    }
}