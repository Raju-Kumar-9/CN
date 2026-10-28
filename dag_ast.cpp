// Expression -> Abstract Syntax Tree (AST) -> Directed Acyclic Graph (DAG)
//
// The DAG is built from the AST by sharing identical sub-expressions
// (value numbering): a node is identified by (operator, left id, right id),
// and a leaf by its name. If the key already exists, the old node is reused.
//
// Supports: identifiers / numbers, + - * / and parentheses.
//
// Compile: g++ -std=c++11 ast_dag.cpp -o ast_dag
// Run:     ./ast_dag            (then type an expression)
//          ./ast_dag "a+a*(b-c)+(b-c)*d"

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cctype>
#include <stdexcept>
using namespace std;

struct Node {
    string label;       // operator or operand name
    Node *left, *right; // children (nullptr for leaves)
    int id;             // node number
    Node(const string &l, Node *a = nullptr, Node *b = nullptr, int i = 0)
        : label(l), left(a), right(b), id(i) {}
    bool isLeaf() const { return !left && !right; }
};

/* ------------------------- Parser (recursive descent) ------------------------- */
class Parser {
    string s;
    size_t pos = 0;
    int counter = 0;

    void skip() { while (pos < s.size() && isspace((unsigned char)s[pos])) pos++; }
    Node *make(const string &l, Node *a = nullptr, Node *b = nullptr) {
        return new Node(l, a, b, ++counter);
    }

    // expr   -> term   { (+|-) term }
    Node *expr() {
        Node *n = term();
        for (skip(); pos < s.size() && (s[pos] == '+' || s[pos] == '-'); skip()) {
            string op(1, s[pos++]);
            Node *r = term();
            n = make(op, n, r);
        }
        return n;
    }
    // term   -> factor { (*|/) factor }
    Node *term() {
        Node *n = factor();
        for (skip(); pos < s.size() && (s[pos] == '*' || s[pos] == '/'); skip()) {
            string op(1, s[pos++]);
            Node *r = factor();
            n = make(op, n, r);
        }
        return n;
    }
    // factor -> identifier | number | '(' expr ')'
    Node *factor() {
        skip();
        if (pos >= s.size()) throw runtime_error("unexpected end of expression");
        if (s[pos] == '(') {
            pos++;
            Node *n = expr();
            skip();
            if (pos >= s.size() || s[pos] != ')') throw runtime_error("missing ')'");
            pos++;
            return n;
        }
        if (isalnum((unsigned char)s[pos]) || s[pos] == '_') {
            size_t st = pos;
            while (pos < s.size() && (isalnum((unsigned char)s[pos]) || s[pos] == '_')) pos++;
            return make(s.substr(st, pos - st));
        }
        throw runtime_error(string("unexpected character '") + s[pos] + "'");
    }

public:
    Node *parse(const string &text) {
        s = text; pos = 0; counter = 0;
        Node *root = expr();
        skip();
        if (pos != s.size()) throw runtime_error("unexpected trailing input");
        return root;
    }
};

/* ------------------------------ AST utilities ------------------------------ */
void collect(Node *n, vector<Node *> &out, map<Node *, bool> &seen) {
    if (!n || seen[n]) return;
    seen[n] = true;
    collect(n->left, out, seen);
    collect(n->right, out, seen);
    out.push_back(n); // post-order => children come before parents
}

// Sideways tree: right subtree on top, left subtree at the bottom
void printTree(Node *n, int depth = 0) {
    if (!n) return;
    printTree(n->right, depth + 1);
    cout << string(depth * 6, ' ') << n->label << "\n";
    printTree(n->left, depth + 1);
}

void printTable(Node *root, const string &title) {
    vector<Node *> nodes;
    map<Node *, bool> seen;
    collect(root, nodes, seen);
    cout << title << "\n";
    cout << "  id   label   left   right\n";
    for (Node *n : nodes) {
        cout << "  " << n->id << "\t" << n->label << "\t"
             << (n->left ? to_string(n->left->id) : "-") << "\t"
             << (n->right ? to_string(n->right->id) : "-") << "\n";
    }
    cout << "  Total nodes: " << nodes.size() << "\n";
}

/* ------------------------------ DAG construction ------------------------------ */
class DAGBuilder {
    map<string, Node *> table; // key -> existing node
    int counter = 0;

public:
    Node *build(Node *ast) {
        if (!ast) return nullptr;
        Node *l = build(ast->left);
        Node *r = build(ast->right);

        string key = ast->label;
        if (l) key += "|" + to_string(l->id);
        if (r) key += "|" + to_string(r->id);

        auto it = table.find(key);
        if (it != table.end()) return it->second;   // reuse shared sub-expression

        Node *n = new Node(ast->label, l, r, ++counter);
        table[key] = n;
        return n;
    }
};

// Prints the DAG as a tree, but a shared node is expanded only the first time;
// later occurrences are shown as a reference "(-> #id)".
void printDAG(Node *n, map<Node *, bool> &printed, int depth = 0) {
    if (!n) return;
    string pad(depth * 4, ' ');
    if (printed[n]) {
        cout << pad << n->label << "  (shared -> #" << n->id << ")\n";
        return;
    }
    printed[n] = true;
    cout << pad << n->label << "  #" << n->id << "\n";
    printDAG(n->left, printed, depth + 1);
    printDAG(n->right, printed, depth + 1);
}

// Expression back to text (for sanity check)
string toInfix(Node *n) {
    if (n->isLeaf()) return n->label;
    return "(" + toInfix(n->left) + n->label + toInfix(n->right) + ")";
}

int main(int argc, char *argv[]) {
    string input;
    if (argc > 1) input = argv[1];
    else {
        cout << "Enter expression: ";
        getline(cin, input);
    }

    try {
        Parser parser;
        Node *ast = parser.parse(input);

        cout << "\nFully parenthesized: " << toInfix(ast) << "\n";

        cout << "\n===== ABSTRACT SYNTAX TREE (AST) =====\n";
        cout << "(rotated 90 degrees: root on the left, right subtree on top)\n\n";
        printTree(ast);
        cout << "\n";
        printTable(ast, "AST node table:");

        DAGBuilder dag;
        Node *root = dag.build(ast);

        cout << "\n===== DIRECTED ACYCLIC GRAPH (DAG) =====\n\n";
        map<Node *, bool> printed;
        printDAG(root, printed);
        cout << "\n";
        printTable(root, "DAG node table:");
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
