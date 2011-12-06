#pragma once

#include "Macros.h"
#include "Token.h"
#include "ITokenSource.h"

namespace Finch
{
    class IErrorReporter;
    class ILineReader;
    
    class Lexer : public ITokenSource
    {
    public:
        Lexer(ILineReader & reader)
        :   mReader(reader),
            mNeedsLine(true),
            mPos(0),
            mStart(0)
        {}
        
        // Will be true if the line reader is.
        virtual bool IsInfinite() const;
        
        // Lexes and returns the next full Token read from the source. If the
        // ILineReader is out of lines, this will return an EOF Token.
        virtual Token ReadToken();
        
    private:
        bool IsDone() const;
        
        char Current() const { return mLine[mPos]; }
        
        char Advance();
                
        Token SingleToken(TokenType type);
        Token ReadString();
        Token ReadNumber();
        Token ReadName();
        Token ReadOperator();
        
        bool IsWhitespace(char c) const;
        bool IsAlpha(char c) const;
        bool IsDigit(char c) const;
        bool IsOperator(char c) const;
        
        ILineReader    & mReader;
        
        bool    mNeedsLine;
        String  mLine;
        int     mPos;
        int     mStart;
        
        NO_COPY(Lexer);
    };
}

