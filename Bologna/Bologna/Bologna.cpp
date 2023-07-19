#include <iostream>
#include <string>
#include <map>

using namespace std;

namespace bologna {
    enum SyntaxKind {
        NumberToken = 0, WhiteSpaceToken, PlusToken, MinusToken, StarToken, SlashToken,
        OpenParenthesesToken, CloseParenthesesToken, EOFToken, BadToken
    };

    std::ostream& operator<<(std::ostream& out, const SyntaxKind value) {
        static std::map<SyntaxKind, std::string> strings;
        if (strings.size() == 0) {
#define INSERT_ELEMENT(p) strings[p] = #p
            INSERT_ELEMENT(NumberToken);
            INSERT_ELEMENT(WhiteSpaceToken);
            INSERT_ELEMENT(PlusToken);
            INSERT_ELEMENT(MinusToken);
            INSERT_ELEMENT(StarToken);
            INSERT_ELEMENT(SlashToken);
            INSERT_ELEMENT(OpenParenthesesToken);
            INSERT_ELEMENT(CloseParenthesesToken);
            INSERT_ELEMENT(EOFToken);
            INSERT_ELEMENT(BadToken);
#undef INSERT_ELEMENT
        }

        return out << strings[value];
    }

    class SyntaxToken {
        SyntaxKind _kind;
        int _position;
        string _text;
        int _value;
    public:
        SyntaxToken(SyntaxKind kind, int position, string text, int value) {
            _kind = kind;
            _position = position;
            _text = text;
            _value = value;
        }

        SyntaxKind Kind() const {
            return _kind;
        }
        int Position() const {
            return _position;
        }
        string Text() const {
            return _text;
        }
        int Value() const {
            return _value;
        }
    };

    class Lexer {
        string _text;
        int _position = 0;

        char Current() const {
            if (_position >= _text.length())
                return '\0';

            return _text[_position];
        }

        void Next() {
            _position++;
        }
    public:
        Lexer(string text) {
            _text = text;
        }

        SyntaxToken NextToken() {
            if (_position >= _text.length())
                return SyntaxToken(EOFToken, _position, "\0", 0);

            if (isdigit(Current())) {
                auto start = _position;

                while (isdigit(Current()))
                    Next();

                auto length = _position - start;
                auto text = _text.substr(start, length);
                auto value = stoi(text);
                return SyntaxToken(NumberToken, start, text, value);
            }
            if (Current() == ' ') {
                auto start = _position;

                while (Current() == ' ')
                    Next();

                auto length = _position - start;
                auto text = _text.substr(start, length);
                return SyntaxToken(WhiteSpaceToken, start, text, 0);
            }

            if (Current() == '+')
                return SyntaxToken(PlusToken, _position++, "+", 0);
            else if (Current() == '-')
                return SyntaxToken(MinusToken, _position++, "-", 0);
            else if (Current() == '*')
                return SyntaxToken(StarToken, _position++, "*", 0);
            else if (Current() == '/')
                return SyntaxToken(SlashToken, _position++, "/", 0);
            else if (Current() == '(')
                return SyntaxToken(OpenParenthesesToken, _position++, "(", 0);
            else if (Current() == ')')
                return SyntaxToken(CloseParenthesesToken, _position++, ")", 0);

            return SyntaxToken(BadToken, _position++, _text.substr(_position - 1, 1), 0);
        }
    };
}

int main()
{
    using namespace bologna;

    while (true) {
        cout << "> ";

        string line;
        getline(cin, line);

        auto lexer = bologna::Lexer(line);
        while (true) {
            auto token = lexer.NextToken();
            if (token.Kind() == EOFToken)
                break;

            cout << token.Kind() << ": '" << token.Text() << "' ";
            if (token.Value() != 0) {
                cout << token.Value();
            }
            cout << endl;
        }
    }

    return 0;
}
