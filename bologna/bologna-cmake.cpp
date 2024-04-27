#include "bologna-cmake.h"

#include <iostream>
#include <string>
#include <map>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace bologna {
    //===----------------------------------------------------------------------===//
    // Lexer
    //===----------------------------------------------------------------------===//

    // The lexer returns tokens [0-255] if it is an unknown character, otherwise one
    // of these for known things.
    enum Token {
        tok_eof = -1,

        // commands
        tok_def = -2,
        tok_extern = -3,

        // primary
        tok_identifier = -4,
        tok_number = -5
    };

    static string IdentifierStr; // Filled in if tok_identifier
    static double NumVal;             // Filled in if tok_number

    /// gettok - Return the next token from standard input.
    static int gettok() {
        static int LastChar = ' ';

        // Skip any whitespace.
        while (isspace(LastChar))
            LastChar = getchar();

        if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
            IdentifierStr = LastChar;
            while (isalnum((LastChar = getchar())))
                IdentifierStr += LastChar;

            if (IdentifierStr == "def")
                return tok_def;
            if (IdentifierStr == "extern")
                return tok_extern;
            return tok_identifier;
        }

        if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
            string NumStr;
            do {
                NumStr += LastChar;
                LastChar = getchar();
            } while (isdigit(LastChar) || LastChar == '.');

            NumVal = strtod(NumStr.c_str(), nullptr);
            return tok_number;
        }

        if (LastChar == '#') {
            // Comment until end of line.
            do
                LastChar = getchar();
            while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

            if (LastChar != EOF)
            return gettok();
        }

        // Check for end of file.  Don't eat the EOF.
        if (LastChar == EOF)
            return tok_eof;

        // Otherwise, just return the character as its ascii value.
        int ThisChar = LastChar;
        LastChar = getchar();
        return ThisChar;
    }

    //===----------------------------------------------------------------------===//
    // Abstract Syntax Tree (aka Parse Tree)
    //===----------------------------------------------------------------------===//

    namespace {

    /// ExprAST - Base class for all expression nodes.
    class ExprAST {
    public:
        virtual ~ExprAST() = default;
    };

    /// NumberExprAST - Expression class for numeric literals like "1.0".
    class NumberExprAST : public ExprAST {
        double Val;

    public:
        NumberExprAST(double Val) : Val(Val) {}
    };

    /// VariableExprAST - Expression class for referencing a variable, like "a".
    class VariableExprAST : public ExprAST {
        string Name;

    public:
        VariableExprAST(const string &Name) : Name(Name) {}
    };

    /// BinaryExprAST - Expression class for a binary operator.
    class BinaryExprAST : public ExprAST {
        char Op;
        unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char Op, unique_ptr<ExprAST> LHS,
                        unique_ptr<ExprAST> RHS)
            : Op(Op), LHS(move(LHS)), RHS(move(RHS)) {}
    };

    /// CallExprAST - Expression class for function calls.
    class CallExprAST : public ExprAST {
        string Callee;
        vector<unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(const string &Callee,
                    vector<unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(move(Args)) {}
    };

    /// PrototypeAST - This class represents the "prototype" for a function,
    /// which captures its name, and its argument names (thus implicitly the number
    /// of arguments the function takes).
    class PrototypeAST {
        string Name;
        vector<string> Args;

    public:
        PrototypeAST(const string &Name, vector<string> Args)
            : Name(Name), Args(move(Args)) {}

        const string &getName() const { return Name; }
    };

    /// FunctionAST - This class represents a function definition itself.
    class FunctionAST {
        unique_ptr<PrototypeAST> Proto;
        unique_ptr<ExprAST> Body;

    public:
        FunctionAST(unique_ptr<PrototypeAST> Proto,
                    unique_ptr<ExprAST> Body)
            : Proto(move(Proto)), Body(move(Body)) {}
        };

    } // end anonymous namespace

    //===----------------------------------------------------------------------===//
    // Parser
    //===----------------------------------------------------------------------===//

    /// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
    /// token the parser is looking at.  getNextToken reads another token from the
    /// lexer and updates CurTok with its results.
    static int CurTok;
    static int getNextToken() { return CurTok = gettok(); }

    /// BinopPrecedence - This holds the precedence for each binary operator that is
    /// defined.
    static map<char, int> BinopPrecedence;

    /// GetTokPrecedence - Get the precedence of the pending binary operator token.
    static int GetTokPrecedence() {
        if (!isascii(CurTok))
            return -1;

        // Make sure it's a declared binop.
        int TokPrec = BinopPrecedence[CurTok];
        if (TokPrec <= 0)
            return -1;
        return TokPrec;
    }

    /// LogError* - These are little helper functions for error handling.
    unique_ptr<ExprAST> LogError(const char *Str) {
        fprintf(stderr, "Error: %s\n", Str);
        return nullptr;
    }
    unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
        LogError(Str);
        return nullptr;
    }

    static unique_ptr<ExprAST> ParseExpression();

    /// numberexpr ::= number
    static unique_ptr<ExprAST> ParseNumberExpr() {
        auto Result = make_unique<NumberExprAST>(NumVal);
        getNextToken(); // consume the number
        return move(Result);
    }

    /// parenexpr ::= '(' expression ')'
    static unique_ptr<ExprAST> ParseParenExpr() {
        getNextToken(); // eat (.
        auto V = ParseExpression();
        if (!V)
            return nullptr;

        if (CurTok != ')')
            return LogError("expected ')'");
        getNextToken(); // eat ).
        return V;
    }

    /// identifierexpr
    ///   ::= identifier
    ///   ::= identifier '(' expression* ')'
    static unique_ptr<ExprAST> ParseIdentifierExpr() {
        string IdName = IdentifierStr;

        getNextToken(); // eat identifier.

        if (CurTok != '(') // Simple variable ref.
            return make_unique<VariableExprAST>(IdName);

        // Call.
        getNextToken(); // eat (
        vector<unique_ptr<ExprAST>> Args;
        if (CurTok != ')') {
            while (true) {
            if (auto Arg = ParseExpression())
                Args.push_back(move(Arg));
            else
                return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
            }
        }

        // Eat the ')'.
        getNextToken();

        return make_unique<CallExprAST>(IdName, move(Args));
    }

    /// primary
    ///   ::= identifierexpr
    ///   ::= numberexpr
    ///   ::= parenexpr
    static unique_ptr<ExprAST> ParsePrimary() {
        switch (CurTok) {
        default:
            return LogError("unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        }
    }

    /// binoprhs
    ///   ::= ('+' primary)*
    static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                                    unique_ptr<ExprAST> LHS) {
        // If this is a binop, find its precedence.
        while (true) {
            int TokPrec = GetTokPrecedence();

            // If this is a binop that binds at least as tightly as the current binop,
            // consume it, otherwise we are done.
            if (TokPrec < ExprPrec)
                return LHS;

            // Okay, we know this is a binop.
            int BinOp = CurTok;
            getNextToken(); // eat binop

            // Parse the primary expression after the binary operator.
            auto RHS = ParsePrimary();
            if (!RHS)
                return nullptr;

            // If BinOp binds less tightly with RHS than the operator after RHS, let
            // the pending operator take RHS as its LHS.
            int NextPrec = GetTokPrecedence();
            if (TokPrec < NextPrec) {
                RHS = ParseBinOpRHS(TokPrec + 1, move(RHS));
                if (!RHS)
                    return nullptr;
            }

            // Merge LHS/RHS.
            LHS = make_unique<BinaryExprAST>(BinOp, move(LHS), move(RHS));
        }
    }

    /// expression
    ///   ::= primary binoprhs
    ///
    static unique_ptr<ExprAST> ParseExpression() {
        auto LHS = ParsePrimary();
        if (!LHS)
            return nullptr;

        return ParseBinOpRHS(0, move(LHS));
    }

    /// prototype
    ///   ::= id '(' id* ')'
    static unique_ptr<PrototypeAST> ParsePrototype() {
        if (CurTok != tok_identifier)
            return LogErrorP("Expected function name in prototype");

        string FnName = IdentifierStr;
        getNextToken();

        if (CurTok != '(')
            return LogErrorP("Expected '(' in prototype");

        vector<string> ArgNames;
        while (getNextToken() == tok_identifier)
            ArgNames.push_back(IdentifierStr);
        if (CurTok != ')')
            return LogErrorP("Expected ')' in prototype");

        // success.
        getNextToken(); // eat ')'.

        return make_unique<PrototypeAST>(FnName, move(ArgNames));
    }

    /// definition ::= 'def' prototype expression
    static unique_ptr<FunctionAST> ParseDefinition() {
        getNextToken(); // eat def.
        auto Proto = ParsePrototype();
        if (!Proto)
            return nullptr;

        if (auto E = ParseExpression())
            return make_unique<FunctionAST>(move(Proto), move(E));
        return nullptr;
    }

    /// toplevelexpr ::= expression
    static unique_ptr<FunctionAST> ParseTopLevelExpr() {
        if (auto E = ParseExpression()) {
            // Make an anonymous proto.
            auto Proto = make_unique<PrototypeAST>("__anon_expr",
                                                        vector<string>());
            return make_unique<FunctionAST>(move(Proto), move(E));
        }
        return nullptr;
    }

    /// external ::= 'extern' prototype
    static unique_ptr<PrototypeAST> ParseExtern() {
        getNextToken(); // eat extern.
        return ParsePrototype();
    }

    //===----------------------------------------------------------------------===//
    // Top-Level parsing
    //===----------------------------------------------------------------------===//

    static void HandleDefinition() {
        if (ParseDefinition()) {
            fprintf(stderr, "Parsed a function definition.\n");
        } else {
            // Skip token for error recovery.
            getNextToken();
        }
    }

    static void HandleExtern() {
        if (ParseExtern()) {
            fprintf(stderr, "Parsed an extern\n");
        } else {
            // Skip token for error recovery.
            getNextToken();
        }
    }

    static void HandleTopLevelExpression() {
        // Evaluate a top-level expression into an anonymous function.
        if (ParseTopLevelExpr()) {
            fprintf(stderr, "Parsed a top-level expr\n");
        } else {
            // Skip token for error recovery.
            getNextToken();
        }
    }

    /// top ::= definition | external | expression | ';'
    static void MainLoop() {
    while (true) {
        fprintf(stderr, "> ");
        switch (CurTok) {
        case tok_eof:
            return;
        case ';': // ignore top-level semicolons.
            getNextToken();
            break;
        case tok_def:
            HandleDefinition();
            break;
        case tok_extern:
            HandleExtern();
            break;
        default:
            HandleTopLevelExpression();
            break;
        }
    }
    }

    //===----------------------------------------------------------------------===//
    // Main driver code.
    //===----------------------------------------------------------------------===//

    int main() {
        // Install standard binary operators.
        // 1 is lowest precedence.
        BinopPrecedence['<'] = 10;
        BinopPrecedence['+'] = 20;
        BinopPrecedence['-'] = 20;
        BinopPrecedence['*'] = 40; // highest.

        // Prime the first token.
        fprintf(stderr, "Bologna v0.1.0\n");
        fprintf(stderr, "> ");
        getNextToken();

        // Run the main "interpreter loop" now.
        MainLoop();

        return 0;
    }
}

int main() {
    return bologna::main();
}
