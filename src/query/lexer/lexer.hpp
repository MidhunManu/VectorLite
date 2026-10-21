#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "query/lexer/token.hpp"

namespace vectordb
{
	class Lexer
	{
	public:
		explicit Lexer(std::string_view sql);
		std::vector<Token> tokenize();

	private:
		std::string_view m_sql;
		size_t m_pos = 0;

		char peek(size_t ahead = 0) const;
		char advance();
		bool at_end() const;
		void skip_whitespace_and_comments();

		Token lex_identifier_or_keyword();
		Token lex_number();
		Token lex_string();
		Token lex_punctuation();
	};
}
