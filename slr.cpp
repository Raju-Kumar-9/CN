// SLR(1) Parser driven by the given parsing table (Table 1)
//
// Grammar (implied by the reduce actions in the table):
//   1. E -> E + T
//   2. E -> T
//   3. T -> T * F
//   4. T -> F
//   5. F -> ( E )
//   6. F -> id
//
// Usage: run the program and type an expression, e.g.  id+id*id
//        (spaces are optional; $ is appended automatically)

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
using namespace std;

// ---------- Grammar productions ----------
struct Production {
    string lhs;
    int rhsLen;      // number of symbols on the right-hand side
    string text;     // for display
};

const Production prods[7] = {
    {"",  0, ""},               // index 0 unused
    {"E", 3, "E -> E + T"},
    {"E", 1, "E -> T"},
    {"T", 3, "T -> T * F"},
    {"T", 1, "T -> F"},
    {"F", 3, "F -> ( E )"},
    {"F", 1, "F -> id"}
};

// ---------- Parsing table ----------
// Terminal columns: id + * ( ) $
const map<string, int> termCol = {
    {"id", 0}, {"+", 1}, {"*", 2}, {"(", 3}, {")", 4}, {"$", 5}
};
// Non-terminal columns: E T F
const map<string, int> nontermCol = {{"E", 0}, {"T", 1}, {"F", 2}};

const int NUM_STATES = 12;

// ACTION[state][terminal]; "" means error
const string ACTION[NUM_STATES][6] = {
    //  id    +     *     (     )     $
    {"s5", "",   "",   "s4", "",   ""   },  // 0
    {"",   "s6", "",   "",   "",   "acc"},  // 1
    {"",   "r2", "s7", "",   "r2", "r2" },  // 2
    {"",   "r4", "r4", "",   "r4", "r4" },  // 3
    {"s5", "",   "",   "s4", "",   ""   },  // 4
    {"",   "r6", "r6", "",   "r6", "r6" },  // 5
    {"s5", "",   "",   "s4", "",   ""   },  // 6
    {"s5", "",   "",   "s4", "",   ""   },  // 7
    {"",   "s6", "",   "",   "s11","" },    // 8
    {"",   "r1", "s7", "",   "r1", "r1" },  // 9
    {"",   "r3", "r3", "",   "r3", "r3" },  // 10
    {"",   "r5", "r5", "",   "r5", "r5" }   // 11
};

// GOTO[state][non-terminal]; -1 means error
const int GOTO[NUM_STATES][3] = {
    //  E   T   F
    { 1,  2,  3},   // 0
    {-1, -1, -1},   // 1
    {-1, -1, -1},   // 2
    {-1, -1, -1},   // 3
    { 8,  2,  3},   // 4
    {-1, -1, -1},   // 5
    {-1,  9,  3},   // 6
    {-1, -1, 10},   // 7
    {-1, -1, -1},   // 8
    {-1, -1, -1},   // 9
    {-1, -1, -1},   // 10
    {-1, -1, -1}    // 11
};

// ---------- Tokenizer ----------
// Returns false if an invalid character is found.
bool tokenize(const string& line, vector<string>& tokens) {
    size_t i = 0;
    while (i < line.size()) {
        char c = line[i];
        if (isspace((unsigned char)c)) { i++; continue; }
        if (c == 'i' && i + 1 < line.size() && line[i + 1] == 'd') {
            tokens.push_back("id");
            i += 2;
        } else if (c == '+' || c == '*' || c == '(' || c == ')') {
            tokens.push_back(string(1, c));
            i++;
        } else {
            cerr << "Invalid character '" << c << "' in input.\n";
            return false;
        }
    }
    tokens.push_back("$");
    return true;
}

// ---------- Helpers for printing ----------
string stackToString(const vector<string>& st) {
    string s;
    for (size_t i = 0; i < st.size(); i++) {
        if (i) s += " ";
        s += st[i];
    }
    return s;
}

string inputToString(const vector<string>& toks, size_t pos) {
    string s;
    for (size_t i = pos; i < toks.size(); i++) s += toks[i];
    return s;
}

// ---------- SLR parse driver ----------
bool parse(const vector<string>& tokens) {
    // Stack holds both states (numbers) and grammar symbols, interleaved:
    // 0 id 5 + 6 ...
    vector<string> stack;
    stack.push_back("0");
    size_t ip = 0;
    int step = 1;

    const int W_STEP = 6, W_STACK = 34, W_INPUT = 16;
    cout << left << setw(W_STEP) << "Step"
         << setw(W_STACK) << "Stack"
         << right << setw(W_INPUT) << "Input" << "   "
         << left << "Action" << "\n";
    cout << string(W_STEP + W_STACK + W_INPUT + 3 + 24, '-') << "\n";

    while (true) {
        int state = stoi(stack.back());
        const string& a = tokens[ip];
        int col = termCol.at(a);
        string act = ACTION[state][col];

        cout << left << setw(W_STEP) << step++
             << setw(W_STACK) << stackToString(stack)
             << right << setw(W_INPUT) << inputToString(tokens, ip) << "   ";

        if (act.empty()) {
            cout << left << "ERROR (no action for state " << state
                 << " on '" << a << "')\n";
            return false;
        }

        if (act == "acc") {
            cout << left << "Accept\n";
            return true;
        }

        if (act[0] == 's') {                       // shift
            int next = stoi(act.substr(1));
            cout << left << "Shift " << next << "\n";
            stack.push_back(a);
            stack.push_back(to_string(next));
            ip++;
        } else {                                   // reduce
            int p = stoi(act.substr(1));
            cout << left << "Reduce by " << p << ": " << prods[p].text << "\n";
            // pop 2 * |rhs| entries (symbol + state for each)
            for (int k = 0; k < 2 * prods[p].rhsLen; k++) stack.pop_back();
            int top = stoi(stack.back());
            int g = GOTO[top][nontermCol.at(prods[p].lhs)];
            if (g < 0) {
                cout << "ERROR (missing GOTO)\n";
                return false;
            }
            stack.push_back(prods[p].lhs);
            stack.push_back(to_string(g));
        }
    }
}

int main() {
    cout << "SLR Parser\n";
    cout << "Grammar:\n";
    for (int i = 1; i <= 6; i++) cout << "  (" << i << ") " << prods[i].text << "\n";
    cout << "\nEnter input string (e.g. id+id*id): ";

    string line;
    getline(cin, line);

    vector<string> tokens;
    if (!tokenize(line, tokens)) return 1;

    cout << "\n";
    bool ok = parse(tokens);
    cout << "\nResult: " << (ok ? "String ACCEPTED" : "String REJECTED") << "\n";
    return ok ? 0 : 1;
}
