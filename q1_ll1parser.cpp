// Table-driven predictive (LL(1)) parser for the grammar behind Table 1:
//   E  -> T E'
//   E' -> + T E' | epsilon
//   T  -> F T'
//   T' -> * F T' | epsilon
//   F  -> ( E )  | id
//
// Usage:  ./predictive_parser "id+id*id"
//         (or run without arguments and type an expression)

#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <stack>
using namespace std;

using Production = vector<string>;   // right-hand side; empty vector = epsilon

// ---------- Parsing table (Table 1) ----------
map<string, map<string, Production>> buildTable() {
    map<string, map<string, Production>> M;

    M["E"]["id"] = {"T", "E'"};
    M["E"]["("]  = {"T", "E'"};

    M["E'"]["+"] = {"+", "T", "E'"};
    M["E'"][")"] = {};                // E' -> epsilon
    M["E'"]["$"] = {};                // E' -> epsilon

    M["T"]["id"] = {"F", "T'"};
    M["T"]["("]  = {"F", "T'"};

    M["T'"]["+"] = {};                // T' -> epsilon
    M["T'"]["*"] = {"*", "F", "T'"};
    M["T'"][")"] = {};                // T' -> epsilon
    M["T'"]["$"] = {};                // T' -> epsilon

    M["F"]["id"] = {"id"};
    M["F"]["("]  = {"(", "E", ")"};
    return M;
}

bool isNonTerminal(const string& s) {
    return s == "E" || s == "E'" || s == "T" || s == "T'" || s == "F";
}

string prodToString(const string& lhs, const Production& rhs) {
    string s = lhs + " -> ";
    if (rhs.empty()) return s + "e";          // e = epsilon
    for (const auto& x : rhs) s += x;
    return s;
}

// ---------- Lexer: turns text into tokens id + * ( ) ----------
bool tokenize(const string& text, vector<string>& tokens) {
    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        if (isspace((unsigned char)c)) { i++; continue; }
        if (text.compare(i, 2, "id") == 0) { tokens.push_back("id"); i += 2; }
        else if (c == '+' || c == '*' || c == '(' || c == ')') {
            tokens.push_back(string(1, c)); i++;
        } else {
            cerr << "Lexical error: unexpected character '" << c << "'\n";
            return false;
        }
    }
    tokens.push_back("$");
    return true;
}

// ---------- Helpers to print the Stack / Input columns ----------
// Stack is shown with its top at the RIGHT, e.g.  $E'T
string stackToString(const vector<string>& st) {
    string s;
    for (const auto& x : st) s += x;
    return s;
}
string inputToString(const vector<string>& toks, size_t pos) {
    string s;
    for (size_t i = pos; i < toks.size(); i++) s += toks[i];
    return s;
}

// ---------- The predictive parser ----------
bool parse(const vector<string>& tokens) {
    auto M = buildTable();
    vector<string> st = {"$", "E"};           // bottom ... top
    size_t ip = 0;

    const int W1 = 20, W2 = 20;
    cout << left << setw(W1) << "STACK" << setw(W2) << "INPUT" << "ACTION\n";
    cout << string(W1 + W2 + 30, '-') << "\n";

    while (true) {
        string X = st.back();
        string a = tokens[ip];
        string action;

        cout << left << setw(W1) << stackToString(st)
             << setw(W2) << inputToString(tokens, ip);

        if (X == "$" && a == "$") {
            cout << "Accept\n";
            return true;
        }
        if (!isNonTerminal(X)) {                      // terminal or $
            if (X == a) {
                cout << "Match " << a << "\n";
                st.pop_back();
                ip++;
            } else {
                cout << "ERROR: expected '" << X << "' but found '" << a << "'\n";
                return false;
            }
        } else {                                      // non-terminal
            auto row = M.find(X);
            auto cell = row->second.find(a);
            if (cell == row->second.end()) {
                cout << "ERROR: no entry M[" << X << ", " << a << "]\n";
                return false;
            }
            const Production& rhs = cell->second;
            cout << "Output " << prodToString(X, rhs) << "\n";
            st.pop_back();
            for (auto it = rhs.rbegin(); it != rhs.rend(); ++it)
                st.push_back(*it);                    // push RHS reversed
        }
    }
}

int main(int argc, char* argv[]) {
    string text;
    if (argc > 1) text = argv[1];
    else {
        cout << "Enter expression (e.g. id+id*id): ";
        getline(cin, text);
    }

    vector<string> tokens;
    if (!tokenize(text, tokens)) return 1;

    cout << "\nParsing: " << text << "\n\n";
    bool ok = parse(tokens);
    cout << "\nResult: " << (ok ? "String ACCEPTED" : "String REJECTED") << "\n";
    return ok ? 0 : 1;
}
