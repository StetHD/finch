#include <iostream> // for debugging
#include <cstdlib>

#include "Lexer.h"
#include "IErrorReporter.h"
#include "ILineReader.h"

namespace Finch
{
    bool Lexer::IsInfinite() const
    {
        return mReader.IsInfinite();
    }

    Token Lexer::ReadToken()
    {
        Token token;
        
        while (token.Type() == TOKEN_NONE)
        {
            char c = mLine[mIndex];
            
            switch (mState)
            {
                case LEX_NEED_LINE:
                    if (mReader.EndOfLines())
                    {
                        mState = LEX_DONE;
                    }
                    else
                    {
                        mLine = mReader.NextLine();
                        mIndex = 0;
                        mState = LEX_DEFAULT;
                    }
                    break;
                    
                case LEX_DEFAULT:
                    switch (c)
                    {
                        case '(': token = SingleToken(TOKEN_LEFT_PAREN); break;
                        case ')': token = SingleToken(TOKEN_RIGHT_PAREN); break;
                        case '[': token = SingleToken(TOKEN_LEFT_BRACKET); break;
                        case ']': token = SingleToken(TOKEN_RIGHT_BRACKET); break;
                        case '{': token = SingleToken(TOKEN_LEFT_BRACE); break;
                        case '}': token = SingleToken(TOKEN_RIGHT_BRACE); break;
                        case ',': token = SingleToken(TOKEN_COMMA); break;
                        case '.': token = SingleToken(TOKEN_DOT); break;
                        case ';': token = SingleToken(TOKEN_LINE); break;
                        case '\\': token = SingleToken(TOKEN_IGNORE_LINE); break;
                        case '|': token = SingleToken(TOKEN_PIPE); break;
                            
                        case '-': StartState(LEX_IN_MINUS); break;
                        case ':': StartState(LEX_IN_COLON); break;
                            
                        case '"':
                            mEscapedString = "";
                            StartState(LEX_IN_STRING);
                            break;
                            
                        case '\'':
                            mState = LEX_IN_COMMENT;
                            break;
                            
                        case '\0':
                            token = Token(TOKEN_LINE);
                            mState = LEX_NEED_LINE;
                            break;
                        
                        case ' ': Consume(); break;
                            
                        default:
                            if (IsDigit(c))         StartState(LEX_IN_NUMBER);
                            else if (IsAlpha(c))    StartState(LEX_IN_NAME);
                            else if (IsOperator(c)) StartState(LEX_IN_OPERATOR);
                            else
                            {
                                String message = String::Format("Unknown character '%c'.", c);
                                mErrorReporter.Error(message);
                                
                                // just ignore the character and continue
                                Consume();
                            }
                    }
                    break;
                    
                case LEX_IN_MINUS:
                    if (IsDigit(c))         ChangeState(LEX_IN_NUMBER);
                    else if (IsOperator(c)) ChangeState(LEX_IN_OPERATOR);
                    else
                    {
                        token = Token(TOKEN_OPERATOR, "-");
                        mState = LEX_DEFAULT;
                    }
                    break;
                    
                case LEX_IN_NUMBER:
                    if (IsDigit(c)) Consume();
                    else if (c =='.')
                    {
                        ChangeState(LEX_IN_DECIMAL);
                    }
                    else
                    {
                        String text = mLine.Substring(mTokenStart, mIndex - mTokenStart);
                        double number = atof(text.CString());
                        token = Token(TOKEN_NUMBER, number);
                        
                        mState = LEX_DEFAULT;
                    }
                    break;
                    
                case LEX_IN_DECIMAL:
                    if (IsDigit(c)) Consume();
                    else
                    {
                        String text = mLine.Substring(mTokenStart, mIndex - mTokenStart);
                        double number = atof(text.CString());
                        token = Token(TOKEN_NUMBER, number);
                        
                        mState = LEX_DEFAULT;
                    }
                    break;
                    
                case LEX_IN_NAME:
                    if (IsAlpha(c) || IsDigit(c) || IsOperator(c)) Consume();
                    else if (c == ':')
                    {
                        Consume();
                        
                        String name = mLine.Substring(mTokenStart, mIndex - mTokenStart);
                        token = Token(TOKEN_KEYWORD, name);

                        mState = LEX_DEFAULT;
                    }
                    else
                    {
                        String name = mLine.Substring(mTokenStart, mIndex - mTokenStart);
                        
                        // see if it's a reserved word
                        if (name == "self") token = Token(TOKEN_SELF);
                        else if (name == "undefined") token = Token(TOKEN_UNDEFINED);
                        else if (name == "break") token = Token(TOKEN_BREAK);
                        else if (name == "return") token = Token(TOKEN_RETURN);
                        else token = Token(TOKEN_NAME, name);
                        
                        mState = LEX_DEFAULT;
                    }
                    break;
                    
                case LEX_IN_OPERATOR:
                    if (IsOperator(c))   Consume();
                    else if (IsAlpha(c)) mState = LEX_IN_NAME;
                    else
                    {
                        String name = mLine.Substring(mTokenStart, mIndex - mTokenStart);
                        
                        // see if it's a reserved operator
                        if (name == "<-") token = Token(TOKEN_ARROW);
                        else if (name == "<--") token = Token(TOKEN_LONG_ARROW);
                        else token = Token(TOKEN_OPERATOR, name);
                        
                        mState = LEX_DEFAULT;
                    }
                    break;
                    
                case LEX_IN_STRING:
                    switch (c)
                    {
                        case '"':
                            token = Token(TOKEN_STRING, mEscapedString);
                            ChangeState(LEX_DEFAULT);
                            break;
                            
                        case '\\':
                            ChangeState(LEX_IN_STRING_ESCAPE);
                            break;
                            
                        case '\0':
                            mErrorReporter.Error("String is missing closing '\"'.");
                            // just ditch the token and end the line
                            mState = LEX_NEED_LINE;
                            break;

                        default:
                            // allow other characters in string
                            mEscapedString += c;
                            Consume();
                            break;
                    }
                    break;
                    
                case LEX_IN_STRING_ESCAPE:
                    switch (c)
                    {
                        case '"':  EscapeCharacter('"'); break;
                        case 'n':  EscapeCharacter('\n'); break;
                        case '\\': EscapeCharacter('\\'); break;
                        
                        case '\0':
                            mErrorReporter.Error("String is missing escape code and closing '\"'.");
                            
                            // just ditch the token and end the line
                            mState = LEX_NEED_LINE;
                            break;
                            
                        default:
                            String message = String::Format("Unrecognized string escape code '\\%c'.", c);
                            mErrorReporter.Error(message);

                            // ignore the escape sequence and keep going
                            mState = LEX_IN_STRING;
                            Consume();
                            break;
                    }
                    break;
                    
                case LEX_IN_COMMENT:
                    if (c == '\0')
                    {
                        token = Token(TOKEN_LINE);
                        mState = LEX_NEED_LINE;
                    }
                    else Consume();
                    break;
                    
                case LEX_IN_COLON:
                    if (c == ':')
                    {
                        token = Token(TOKEN_BIND);
                        ChangeState(LEX_DEFAULT);
                    }
                    else
                    {
                        // emit the first colon as a keyword
                        token = Token(TOKEN_KEYWORD);
                        // don't consume the character after the :
                        mState = LEX_DEFAULT;
                    }
                    break;
                    
                case LEX_DONE:
                    token = Token(TOKEN_EOF);
                    break;
                    
                default:
                    ASSERT(false, "Uknown lexer state.");
            }
        }
        
        return token;
    }
    
    void Lexer::Consume()
    {
        mIndex++;
    }
    
    Token Lexer::SingleToken(TokenType type)
    {
        Consume();
        return Token(type);
    }
    
    void Lexer::StartState(State state)
    {
        mState = state;
        mTokenStart = mIndex;
        Consume();
    }

    void Lexer::ChangeState(State state)
    {
        Consume();
        mState = state;
    }
    
    void Lexer::EscapeCharacter(char c)
    {
        mEscapedString += c;
        ChangeState(LEX_IN_STRING);
    }
    
    bool Lexer::IsAlpha(char c) const
    {
        return (c == '_') ||
               ((c >= 'a') && (c <= 'z')) ||
               ((c >= 'A') && (c <= 'Z'));
    }
    
    bool Lexer::IsDigit(char c) const
    {
        return (c >= '0') && (c <= '9');
    }
    
    bool Lexer::IsOperator(char c) const
    {
        return (c != '\0') &&
               (strchr("-+=/<>?~!@#$%^&*", c) != NULL);
    }
}

