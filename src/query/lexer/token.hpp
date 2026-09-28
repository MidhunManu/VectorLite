#pragma once

#include <cstddef>
#include <string_view>

namespace vectordb {
	enum class TokenType
	{
		Create,
		Collection,
		Insert,
		Into,
		Values,
		Select,
		From,
		Where,
		Order,
		By,
		Limit,
		Delete,
		Update,
		Set,
		Begin,
		Commit,
		Rollback,
		And,
		Or,
		Vector,
		VectorDistance,
		Identifier,
		IntegerLiteral,
		FloatLiteral,
		StringLiteral,
		Param,
		LParen,
		RParen,
		Comma,
		Star,
		Eq,
		Ne,
		Gt,
		Lt,
		Le,
		Ge,
		Eof,
	};

	struct Token
	{
		TokenType tokenType;
		std::string_view lexeme;
		size_t offset;
	};

	const char* token_type_name(TokenType type);
}
