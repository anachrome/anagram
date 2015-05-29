#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <list>
#include <algorithm>
#include <bitset>
#include <map>
#include <sstream>

#include <locale>
#include <codecvt>

#include <getopt.h>

using namespace std;

typedef unsigned long long ullong_t; /* typedef uint64_t bitmask_t ? */

typedef vector<ullong_t> word_t;

// entry into the dictionary
struct entry_t {
    string bytes;    // used to save on conversion time when (w)couting
    wstring letters; // the letters (unicode codepoints) of the entry, in order
    word_t mask;     // the letters, represented as a vector of bitmasks
    bool same;       // true if this is an anagram of the previous entry
};

typedef list<entry_t>::const_iterator dict_iter_t;

// global modifiers
int min_letters = 0;
int max_letters = 0;
int min_words = 0;
int max_words = 0;
bool case_insensitive = false;
bool show_words = false;
wstring punctuation;
bool keep = false;

bool count(map<wchar_t, ullong_t> &letter_pos, wstring word, word_t &mask);
bool in(const word_t &needle, const word_t &haystack);
word_t remove(word_t needle, const word_t &haystack);

/*  // used in anagram
    void print(const list<entry_t> &dict, vector<dict_iter_t> &anagram);

    // used in main
    void anagram(const list<entry_t> &dict, const dict_iter_t &begin,
                 word_t word, vector<dict_iter_t> &prefix);
*/

void print(const list<entry_t> &dict, vector<dict_iter_t> anagram) {
    vector<pair<dict_iter_t, bool>> begin;
    for (auto i : anagram) {
        bool same = begin.size() && i->mask == begin.back().first->mask;
        begin.push_back({i, same});
    }

    while (true) {
        if (anagram.size()) {
            cout << (anagram[0]->bytes);
            for (int i = 1; i < anagram.size(); i++)
                cout << " " << anagram[i]->bytes;
        }
        cout << '\n';

        int i = anagram.size();
        while (i && (++anagram[i - 1] == dict.cend() || !anagram[i - 1]->same))
            i--;
        if (i - 1 < 0)
            return;
        for (; i < anagram.size(); i++)
            anagram[i] = begin[i].second ? anagram[i - 1] : begin[i].first;
    }
}

// the sortedness of dict before calling this algorithm affects its speed
void anagram(const list<entry_t> &dict, const dict_iter_t &begin,
             word_t word, vector<dict_iter_t> &prefix) {

    if (!word.size() && prefix.size() >= min_words)
        print(dict, prefix);

    if (max_words && prefix.size() == max_words)
        return;

    for (auto i = begin; i != dict.cend(); i++) {
        if (i->same)
            continue;
        if (in(i->mask, word)) {
            prefix.push_back(i);
            
            anagram(dict,
                    i,
                    remove(i->mask, word),
                    prefix);

            prefix.pop_back();
        }
    }
}

int main(int argc, char **argv) {

    // locale bullshit

    std::locale::global(std::locale(""));
    //std::wcout.imbue(std::locale());

    wstring_convert<codecvt_utf8<wchar_t>> converter;

    /*  tentative options + flags: and various TODO:s

        find default dictionary

        also there are a couple of "feature bugs" that might want removing

        perhaps --allow should be --alphabet or --whitelist?
        perhaps --ignore should be --punctuation?
        perhaps --case-insensitive should be --ignore-case?

        SEGFAULTS with -a

        --word-separator for alternative to space?

        print worse case word                          -S --biggest-word
                                                       or --worst-phrase ?
        
        filter certain anagrams, at least with regexen
        --filter, -f
        --keep, -k

        add sanity checks/error messages for bad options

        we need a --help and/or a manpage
    */

    string dictfile;
    string want;

    auto shorts = "d:l:L:w:W:isp:a:";
    struct option longs[] = {
        /* name                has_arg            flag     val */
        {  "dict",             required_argument, nullptr, 'd' },
        {  "min-letters",      required_argument, nullptr, 'l' },
        {  "max-letters",      required_argument, nullptr, 'L' },
        {  "max-words",        required_argument, nullptr, 'w' },
        {  "min-words",        required_argument, nullptr, 'W' },
        {  "case-insensitive", no_argument,       nullptr, 'i' },
        {  "show-words",       no_argument,       nullptr, 's' },
        {  "ignore",           required_argument, nullptr, 'p' },
        {  "allow",            required_argument, nullptr, 'a' },
        { 0, 0, 0, 0 } /* end of list */
    };

    int opt;
    while ((opt = getopt_long(argc, argv, shorts, longs, nullptr)) != -1) {
        if (opt == '?') {
            return 1;
        } else {
            switch (opt) {
            case 'd':
                dictfile = optarg;
                break;
            case 'l':
                min_letters = stoi(optarg);
                break;
            case 'L':
                max_letters = stoi(optarg);
                break;
            case 'w':
                max_words = stoi(optarg);
                break;
            case 'W':
                min_words = stoi(optarg);
                break;
            case 'i':
                case_insensitive = true;
                break;
            case 's':
                show_words = true;
                break;
            case 'p':
                punctuation = converter.from_bytes(optarg);
                break;
            case 'a':
                punctuation = converter.from_bytes(optarg);
                keep = true;
                break;
            }
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        cout << "bad\n";
        return 1;
    } else {
        want = *argv;
    }

    /* #### */

    wstring wwant = converter.from_bytes(want);

    if (case_insensitive)
        transform(wwant.begin(), wwant.end(), wwant.begin(), ::tolower);

    map<wchar_t, ullong_t> letter_pos;

    ullong_t letter_hash = 0x1;
    int alphabet = 0;
    for (wchar_t c : wwant) {
        if (case_insensitive)
            c = tolower(c);
        if (letter_pos.find(c) != letter_pos.cend())
            continue;
        letter_pos[c] = letter_hash;
        letter_hash <<= 1;
        alphabet++;
    }

    // if this returns false, there is a bug in the program
    word_t cwant;
    count(letter_pos, wwant, cwant);

    wifstream file(dictfile);
    if (!file.good()) {
        cout << "couldn't open " << dictfile << endl;
        return 1;
    }

    list<entry_t> dict;

    wstring wword;
    while (getline(file, wword)) {
        string word = converter.to_bytes(wword);

        if (case_insensitive)
            transform(wword.begin(), wword.end(), wword.begin(), ::tolower);

        if (keep) {
            wword.erase(remove_if(wword.begin(), wword.end(),
                [](wchar_t c) {
                    //return find(punctuation.cbegin(), punctuation.cend(),
                    //    c) == punctuation.cend();
                    return punctuation.find(c) == punctuation.npos;
                }), wword.end());
        } else {
            wword.erase(remove_if(wword.begin(), wword.end(),
                [](wchar_t c) {
                    //return find(punctuation.cbegin(), punctuation.cend(),
                    //    c) != punctuation.cend();
                    return punctuation.find(c) != punctuation.npos;
                }), wword.end());
        }

        if (!wword.size()) {
            cout << "warning: `" << word << "' is entirely punctuation."
                                         << "  skipping.\n";
            continue;
        }

        if (max_letters && wword.size() > max_letters)
            continue;
        if (wword.size() < min_letters)
            continue;

        word_t cword;
        if (!count(letter_pos, wword, cword))
            continue;
        if (!in(cword, cwant))
            continue;


        dict.push_back({ word, wword, cword, false });
    }

    dict.sort([](entry_t a, entry_t b) {
        if (a.letters.size() == b.letters.size())
            return a.mask < b.mask;
        else
            return a.letters.size() > b.letters.size();
    });

    for (auto i = dict.begin(); i != --dict.end();) {
        auto now = i->mask;
        auto next = (++i)->mask;
        if (now == next) {
            i->same = true;
        }
    }

    if (show_words) {
        for (auto e : dict)
            cout << e.bytes << endl;
        return 0;
    }

    vector<dict_iter_t> prefix;
    anagram(dict, dict.cbegin(), cwant, prefix);

    return 0;
}

bool count(map<wchar_t, ullong_t> &letter_pos, wstring word, word_t &mask) {
    for (wchar_t c : word) {
        if (letter_pos.find(c) == letter_pos.cend())
            return false;

        int index;
        for (index = mask.size(); index > 0; index--)
            if (mask[index - 1] & letter_pos[c])
                break;

        if (index == mask.size())
            mask.push_back(0x0);

        mask[index] |= letter_pos[c];
    }

    return true;
}

bool in(const word_t &needle, const word_t &haystack) {
    if (needle.size() > haystack.size())
        return false;

    for (int i = 0; i < needle.size(); i++)
        if ((needle[i] & haystack[i]) != needle[i])
            return false;

    return true;
}

// this is not guaranteed to do anything sane if needle isn't in haystack
word_t remove(word_t needle, const word_t &haystack) {
    word_t out = haystack;

    for (int i = 0; i < needle.size(); i++) {
        int end = out.size() - 1;

        while (needle[i] & ~0x0) {
            auto old_out = out[end];
            out[end] &= ~needle[i];
            needle[i] ^= (out[end] ^ old_out);

            if (!out[end])
                out.pop_back();

            end--;
        }
    }

    return out;
}
