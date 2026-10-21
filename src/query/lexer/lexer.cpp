#include "query/lexer/lexer.hpp"
#include "query/query_error.hpp"

#include <cctype>
#include <unordered_map>

namespace vectordb
{
	namespace
	{
		const std::unordered_map<std::string, TokenType> &keyword_map()
		{
			static const std::unordered_map<std::string, TokenType> map = {
				{"CREATE", TokenType::Create},
				{"COLLECTION", TokenType::Collection},
				{"INSERT", TokenType::Insert},
				{"INTO", TokenType::Into},
				{"VALUES", TokenType::Values},
				{"SELECT", TokenType::Select},
				{"FROM", TokenType::From},
				{"WHERE", TokenType::Where},
				{"ORDER", TokenType::Order},
				{"BY", TokenType::By},
				{"LIMIT", TokenType::Limit},
				{"DELETE", TokenType::Delete},
				{"UPDATE", TokenType::Update},
				{"SET", TokenType::Set},
				{"BEGIN", TokenType::Begin},
				{"COMMIT", TokenType::Commit},
				{"ROLLBACK", TokenType::Rollback},
				{"AND", TokenType::And},
				{"OR", TokenType::Or},
				{"VECTOR", TokenType::Vector},
				{"VECTOR_DISTANCE", TokenType::VectorDistance},
			};
			return map;
		}

		std::string to_upper(std::string_view s)
		{
			std::string r(s);
			for (char &c : r)
				c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			return r;
		}
	}

	Lexer::Lexer(std::string_view sql) : m_sql(sql) {}

	bool Lexer::at_end() const { return m_pos >= m_sql.size(); }

	char Lexer::peek(size_t ahead) const
	{
		size_t idx = m_pos + ahead;
		return idx < m_sql.size() ? m_sql[idx] : '\0';
	}

	char Lexer::advance() { return m_sql[m_pos++]; }

	void Lexer::skip_whitespace_and_comments()
	{
		while (!at_end())
		{
			char c = peek();
			if (std::isspace(static_cast<unsigned char>(c)))
			{
				advance();
			}
			else if (c == '-' && peek(1) == '-')
			{
				while (!at_end() && peek() != '\n')
					advance();
			}
			else
			{
				break;
			}
		}
	}

	std::vector<Token> Lexer::tokenize()
	{
		std::vector<Token> tokens;

		while (true)
		{
			skip_whitespace_and_comments();
			if (at_end())
			{
				tokens.push_back(Token{TokenType::Eof, {}, m_pos});
				break;
			}

			char c = peek();
			size_t start = m_pos;

			if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
				tokens.push_back(lex_identifier_or_keyword());
			else if (std::isdigit(static_cast<unsigned char>(c)))
				tokens.push_back(lex_number());
			else if (c == '\'')
				tokens.push_back(lex_string());
			else if (c == '?')
			{
				advance();
				tokens.push_back(Token{TokenType::Param, m_sql.substr(start, 1), start});
			}
			else
				tokens.push_back(lex_punctuation());
		}

		return tokens;
	}

	Token Lexer::lex_identifier_or_keyword()
	{
		size_t start = m_pos;
		while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
		{
			advance();
		}

		std::string_view lexeme = m_sql.substr(start, m_pos - start);
		auto it = keyword_map().find(to_upper(lexeme));
		TokenType type = (it != keyword_map().end()) ? it->second : TokenType::Identifier;
		return Token{type, lexeme, start};
	}

	Token Lexer::lex_number()
	{
		size_t start = m_pos;
		bool is_float = false;

		while (!at_end() && std::isdigit(static_cast<unsigned char>(peek())))
			advance();

		if (!at_end() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))
		{
			is_float = true;
			advance();
			while (!at_end() && std::isdigit(static_cast<unsigned char>(peek())))
				advance();
		}

		std::string_view lexeme = m_sql.substr(start, m_pos - start);
		return Token{is_float ? TokenType::FloatLiteral : TokenType::IntegerLiteral, lexeme, start};
	}

	Token Lexer::lex_string()
	{
		size_t start = m_pos;
		advance(); // opening quote

		while (true)
		{
			if (at_end())
			{
				throw QueryError("unterminated string literal", start);
			}

			char c = advance();
			if (c == '\'')
			{
				if (!at_end() && peek() == '\'')
					advance(); // escaped '' -> keep scanning
				else
					break;
			}
		}

		std::string_view lexeme = m_sql.substr(start, m_pos - start);
		return Token{TokenType::StringLiteral, lexeme, start};
	}

	Token Lexer::lex_punctuation()
	{
		size_t start = m_pos;
		char c = advance();

		switch (c)
		{
		case '(':
			return Token{TokenType::LParen, m_sql.substr(start, 1), start};
		case ')':
			return Token{TokenType::RParen, m_sql.substr(start, 1), start};
		case ',':
			return Token{TokenType::Comma, m_sql.substr(start, 1), start};
		case '*':
			return Token{TokenType::Star, m_sql.substr(start, 1), start};
		case '=':
			return Token{TokenType::Eq, m_sql.substr(start, 1), start};
		case '<':
			if (!at_end() && peek() == '=')
			{
				advance();
				return Token{TokenType::Le, m_sql.substr(start, 2), start};
			}
			return Token{TokenType::Lt, m_sql.substr(start, 1), start};
		case '>':
			if (!at_end() && peek() == '=')
			{
				advance();
				return Token{TokenType::Ge, m_sql.substr(start, 2), start};
			}
			return Token{TokenType::Gt, m_sql.substr(start, 1), start};
		case '!':
			if (!at_end() && peek() == '=')
			{
				advance();
				return Token{TokenType::Ne, m_sql.substr(start, 2), start};
			}
			throw QueryError("unexpected character '!'", start);
		default:
			throw QueryError(std::string("unexpected character '") + c + "'", start);
		}
	}
}
