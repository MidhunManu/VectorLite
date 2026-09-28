#include "query/lexer/token.hpp"

namespace vectordb
{
    const char* token_type_name(TokenType type)
    {
        switch (type)
        {
            case TokenType::Create: return "CREATE";
            case TokenType::Collection: return "COLLECTION";
            case TokenType::Insert: return "INSERT";
            case TokenType::Into: return "INTO";
            case TokenType::Values: return "VALUES";
            case TokenType::Select: return "SELECT";
            case TokenType::From: return "FROM";
            case TokenType::Where: return "WHERE";
            case TokenType::Order: return "ORDER";
            case TokenType::By: return "BY";
            case TokenType::Limit: return "LIMIT";
            case TokenType::Delete: return "DELETE";
            case TokenType::Update: return "UPDATE";
            case TokenType::Set: return "SET";
            case TokenType::Begin: return "BEGIN";
            case TokenType::Commit: return "COMMIT";
            case TokenType::Rollback: return "ROLLBACK";
            case TokenType::And: return "AND";
            case TokenType::Or: return "OR";
            case TokenType::Vector: return "VECTOR";
            case TokenType::VectorDistance: return "VECTOR_DISTANCE";
            case TokenType::Identifier: return "identifier";
            case TokenType::IntegerLiteral: return "integer literal";
            case TokenType::FloatLiteral: return "float literal";
            case TokenType::StringLiteral: return "string literal";
            case TokenType::Param: return "'?'";
            case TokenType::LParen: return "'('";
            case TokenType::RParen: return "')'";
            case TokenType::Comma: return "','";
            case TokenType::Star: return "'*'";
            case TokenType::Eq: return "'='";
            case TokenType::Ne: return "'!='";
            case TokenType::Lt: return "'<'";
            case TokenType::Gt: return "'>'";
            case TokenType::Le: return "'<='";
            case TokenType::Ge: return "'>='";
            case TokenType::Eof: return "end of input";
        }
        return "unknown token";
    }
}