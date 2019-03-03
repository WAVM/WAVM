#include <stdint.h>
#include <string>

#include "Lexer.h"
#include "Parse.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/FloatComponents.h"
#include "WAVM/Platform/Defines.h"

// Include the David Gay's dtoa code.
// #define strtod and dtoa to avoid conflicting with the C standard library versions
// This is complicated by dtoa.c including stdlib.h, so we need to also include stdlib.h
// here to ensure that it isn't included with the strtod and dtoa #define.
#include <stdlib.h>

#define IEEE_8087
#define NO_INFNAN_CHECK
#define strtod parseNonSpecialF64
#define dtoa printNonSpecialF64

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4083 4706 4701 4703 4018)
#elif defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#define Long int
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#define Long int
#endif

#include "../ThirdParty/dtoa/dtoa.c"

#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#undef Long
#endif

#undef IEEE_8087
#undef NO_STRTOD_BIGCOMP
#undef NO_INFNAN_CHECK
#undef strtod
#undef dtoa

using namespace WAVM;
using namespace WAVM::WAST;

// Parses an optional + or - sign and returns true if a - sign was parsed.
// If either a + or - sign is parsed, nextChar is advanced past it.
static bool parseSign(const char*& nextChar)
{
	if(*nextChar == '-')
	{
		++nextChar;
		return true;
	}
	else if(*nextChar == '+')
	{
		++nextChar;
	}
	return false;
}

// Parses an unsigned integer from hexits, starting with "0x", and advancing nextChar past the
// parsed hexits. be called for input that's already been accepted by the lexer as a hexadecimal
// integer.
static U64 parseHexUnsignedInt(const char*& nextChar, ParseState* parseState, U64 maxValue)
{
	const char* firstHexit = nextChar;
	wavmAssert(nextChar[0] == '0' && (nextChar[1] == 'x' || nextChar[1] == 'X'));
	nextChar += 2;

	U64 result = 0;
	U8 hexit = 0;
	while(true)
	{
		if(*nextChar == '_')
		{
			++nextChar;
			continue;
		}
		if(!tryParseHexit(nextChar, hexit)) { break; }
		if(result > (maxValue - hexit) / 16)
		{
			parseErrorf(parseState, firstHexit, "integer literal is too large");
			result = maxValue;
			while(tryParseHexit(nextChar, hexit)) {};
			break;
		}
		wavmAssert(result * 16 + hexit >= result);
		result = result * 16 + hexit;
	}
	return result;
}

// Parses an unsigned integer from digits, advancing nextChar past the parsed digits.
// Assumes it will only be called for input that's already been accepted by the lexer as a decimal
// integer.
static U64 parseDecimalUnsignedInt(const char*& nextChar,
								   ParseState* parseState,
								   U64 maxValue,
								   const char* context)
{
	U64 result = 0;
	const char* firstDigit = nextChar;
	while(true)
	{
		if(*nextChar == '_')
		{
			++nextChar;
			continue;
		}
		if(*nextChar < '0' || *nextChar > '9') { break; }

		const U8 digit = *nextChar - '0';
		++nextChar;

		if(result > U64(maxValue - digit) / 10)
		{
			parseErrorf(parseState, firstDigit, "%s is too large", context);
			result = maxValue;
			while((*nextChar >= '0' && *nextChar <= '9') || *nextChar == '_') { ++nextChar; };
			break;
		}
		wavmAssert(result * 10 + digit >= result);
		result = result * 10 + digit;
	};
	return result;
}

// Parses a floating-point NaN, advancing nextChar past the parsed characters.
// Assumes it will only be called for input that's already been accepted by the lexer as a literal
// NaN.
template<typename Float> Float parseNaN(const char*& nextChar, ParseState* parseState)
{
	const char* firstChar = nextChar;

	typedef FloatComponents<Float> FloatComponents;
	FloatComponents resultComponents;
	resultComponents.bits.sign = parseSign(nextChar) ? 1 : 0;
	resultComponents.bits.exponent = FloatComponents::maxExponentBits;

	wavmAssert(nextChar[0] == 'n' && nextChar[1] == 'a' && nextChar[2] == 'n');
	nextChar += 3;

	if(*nextChar == ':')
	{
		++nextChar;

		const U64 significandBits
			= parseHexUnsignedInt(nextChar, parseState, FloatComponents::maxSignificand);
		if(!significandBits)
		{
			parseErrorf(parseState, firstChar, "NaN significand must be non-zero");
			resultComponents.bits.significand = 1;
		}
		resultComponents.bits.significand = typename FloatComponents::Bits(significandBits);
	}
	else
	{
		// If the NaN's significand isn't specified, just set the top bit.
		resultComponents.bits.significand = typename FloatComponents::Bits(1)
											<< (FloatComponents::numSignificandBits - 1);
	}

	return resultComponents.value;
}

// Parses a floating-point infinity. Does not advance nextChar.
// Assumes it will only be called for input that's already been accepted by the lexer as a literal
// infinity.
template<typename Float> Float parseInfinity(const char* nextChar)
{
	// Floating point infinite is represented by max exponent with a zero significand.
	typedef FloatComponents<Float> FloatComponents;
	FloatComponents resultComponents;
	resultComponents.bits.sign = parseSign(nextChar) ? 1 : 0;
	resultComponents.bits.exponent = FloatComponents::maxExponentBits;
	resultComponents.bits.significand = 0;
	return resultComponents.value;
}

template<typename Float> static NO_UBSAN Float uncheckedCast(F64 f64) { return Float(f64); }

template<typename DestFloat> static F64 getMaxCastableF64()
{
	typedef FloatComponents<F64> F64Components;
	typedef FloatComponents<DestFloat> DestComponents;

	F64Components f64Components;
	f64Components.bits.sign = 0;

	// Set the F64 exponent to the maximum exponent of the destination type.
	f64Components.bits.exponent
		= U64(DestComponents::maxNormalExponent) + F64Components::exponentBias;

	// Set the F64 significand to the highest value that rounds to the maximum normal significand of
	// the destination type.
	const Uptr deltaSignificandBits
		= F64Components::numSignificandBits - DestComponents::numSignificandBits;
	f64Components.bits.significand = U64(DestComponents::maxSignificand) << deltaSignificandBits;
	f64Components.bits.significand |= ((U64(1) << deltaSignificandBits) - 1) >> 1;

	return f64Components.value;
}

// Parses a floating point literal, advancing nextChar past the parsed characters. Assumes it will
// only be called for input that's already been accepted by the lexer as a float literal.
template<typename Float> Float parseFloat(const char*& nextChar, ParseState* parseState)
{
	// Scan the token's characters for underscores, and make a copy of it without the underscores
	// for strtod.
	const char* firstChar = nextChar;
	std::string noUnderscoreString;
	bool hasUnderscores = false;
	while(true)
	{
		// Determine whether the next character is still part of the number.
		const bool isNumericChar
			= (*nextChar >= '0' && *nextChar <= '9') || (*nextChar >= 'a' && *nextChar <= 'f')
			  || (*nextChar >= 'A' && *nextChar <= 'F') || *nextChar == 'x' || *nextChar == 'X'
			  || *nextChar == 'p' || *nextChar == 'P' || *nextChar == '+' || *nextChar == '-'
			  || *nextChar == '.' || *nextChar == '_';

		if(!isNumericChar) { break; }

		if(*nextChar == '_' && !hasUnderscores)
		{
			// If this is the first underscore encountered, copy the preceding characters of the
			// number to a std::string.
			noUnderscoreString = std::string(firstChar, nextChar);
			hasUnderscores = true;
		}
		else if(*nextChar != '_' && hasUnderscores)
		{
			// If an underscore has previously been encountered, copy non-underscore characters to
			// that string.
			noUnderscoreString += *nextChar;
		}

		++nextChar;
	};

	// Pass the underscore-free string to parseNonSpecialF64 instead of the original input string.
	const char* noUnderscoreFirstChar = firstChar;
	if(hasUnderscores) { noUnderscoreFirstChar = noUnderscoreString.c_str(); }

	// Use David Gay's strtod to parse a floating point number.
	char* endChar = nullptr;
	F64 f64 = parseNonSpecialF64(noUnderscoreFirstChar, &endChar);
	if(endChar == noUnderscoreFirstChar)
	{ Errors::fatalf("strtod failed to parse number accepted by lexer"); }

	const F64 maxCastableF64 = getMaxCastableF64<Float>();
	if(f64 < -maxCastableF64 || f64 > maxCastableF64)
	{ parseErrorf(parseState, firstChar, "float literal is too large"); }

	return uncheckedCast<Float>(f64);
}

// Tries to parse an numeric literal token as an integer, advancing cursor->nextToken.
// Returns true if it matched a token.
template<typename UnsignedInt>
bool tryParseInt(CursorState* cursor,
				 UnsignedInt& outUnsignedInt,
				 I64 minSignedValue,
				 U64 maxUnsignedValue)
{
	bool isNegative = false;
	U64 u64 = 0;

	const char* nextChar = cursor->parseState->string + cursor->nextToken->begin;
	switch(cursor->nextToken->type)
	{
	case t_decimalInt:
		isNegative = parseSign(nextChar);
		u64 = parseDecimalUnsignedInt(nextChar,
									  cursor->parseState,
									  isNegative ? -U64(minSignedValue) : maxUnsignedValue,
									  "int literal");
		break;
	case t_hexInt:
		isNegative = parseSign(nextChar);
		u64 = parseHexUnsignedInt(
			nextChar, cursor->parseState, isNegative ? -U64(minSignedValue) : maxUnsignedValue);
		break;
	default: return false;
	};

	outUnsignedInt = isNegative ? UnsignedInt(-u64) : UnsignedInt(u64);

	++cursor->nextToken;
	wavmAssert(nextChar <= cursor->parseState->string + cursor->nextToken->begin);

	return true;
}

// Tries to parse a numeric literal literal token as a float, advancing cursor->nextToken.
// Returns true if it matched a token.
template<typename Float> bool tryParseFloat(CursorState* cursor, Float& outFloat)
{
	const char* nextChar = cursor->parseState->string + cursor->nextToken->begin;
	switch(cursor->nextToken->type)
	{
	case t_decimalInt:
	case t_decimalFloat: outFloat = parseFloat<Float>(nextChar, cursor->parseState); break;
	case t_hexInt:
	case t_hexFloat: outFloat = parseFloat<Float>(nextChar, cursor->parseState); break;
	case t_floatNaN: outFloat = parseNaN<Float>(nextChar, cursor->parseState); break;
	case t_floatInf: outFloat = parseInfinity<Float>(nextChar); break;
	default:
		parseErrorf(cursor->parseState, cursor->nextToken, "expected float literal");
		return false;
	};

	++cursor->nextToken;
	wavmAssert(nextChar <= cursor->parseState->string + cursor->nextToken->begin);

	return true;
}

bool WAST::tryParseI32(CursorState* cursor, U32& outI32)
{
	return tryParseInt<U32>(cursor, outI32, INT32_MIN, UINT32_MAX);
}

bool WAST::tryParseI64(CursorState* cursor, U64& outI64)
{
	return tryParseInt<U64>(cursor, outI64, INT64_MIN, UINT64_MAX);
}

bool WAST::tryParseIptr(CursorState* cursor, Uptr& outIptr)
{
	return tryParseInt<Uptr>(cursor, outIptr, INTPTR_MIN, UINTPTR_MAX);
}

U8 WAST::parseI8(CursorState* cursor)
{
	U32 result;
	if(!tryParseInt<U32>(cursor, result, INT8_MIN, UINT8_MAX))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected i8 literal");
		throw RecoverParseException();
	}
	return U8(result);
}

U16 WAST::parseI16(CursorState* cursor)
{
	U32 result;
	if(!tryParseInt<U32>(cursor, result, INT16_MIN, UINT16_MAX))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected i16 literal");
		throw RecoverParseException();
	}
	return U16(result);
}

U32 WAST::parseI32(CursorState* cursor)
{
	U32 result;
	if(!tryParseI32(cursor, result))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected i32 literal");
		throw RecoverParseException();
	}
	return result;
}

U64 WAST::parseI64(CursorState* cursor)
{
	U64 result;
	if(!tryParseI64(cursor, result))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected i64 literal");
		throw RecoverParseException();
	}
	return result;
}

Uptr WAST::parseIptr(CursorState* cursor)
{
	Uptr result;
	if(!tryParseIptr(cursor, result))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected integer literal");
		throw RecoverParseException();
	}
	return result;
}

F32 WAST::parseF32(CursorState* cursor)
{
	F32 result;
	if(!tryParseFloat(cursor, result))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected f32 literal");
		throw RecoverParseException();
	}
	return result;
}

F64 WAST::parseF64(CursorState* cursor)
{
	F64 result;
	if(!tryParseFloat(cursor, result))
	{
		parseErrorf(cursor->parseState, cursor->nextToken, "expected f64 literal");
		throw RecoverParseException();
	}
	return result;
}

V128 WAST::parseV128(CursorState* cursor)
{
	V128 result;
	switch(cursor->nextToken->type)
	{
	case t_i8x16:
		++cursor->nextToken;
		for(Uptr laneIndex = 0; laneIndex < 16; ++laneIndex)
		{ result.i8[laneIndex] = parseI8(cursor); }
		break;
	case t_i16x8:
		++cursor->nextToken;
		for(Uptr laneIndex = 0; laneIndex < 8; ++laneIndex)
		{ result.i16[laneIndex] = parseI16(cursor); }
		break;
	case t_i32x4:
		++cursor->nextToken;
		for(Uptr laneIndex = 0; laneIndex < 4; ++laneIndex)
		{ result.i32[laneIndex] = parseI32(cursor); }
		break;
	case t_i64x2:
		++cursor->nextToken;
		for(Uptr laneIndex = 0; laneIndex < 2; ++laneIndex)
		{ result.i64[laneIndex] = parseI64(cursor); }
		break;
	case t_f32x4:
		++cursor->nextToken;
		for(Uptr laneIndex = 0; laneIndex < 4; ++laneIndex)
		{ result.f32[laneIndex] = parseF32(cursor); }
		break;
	case t_f64x2:
		++cursor->nextToken;
		for(Uptr laneIndex = 0; laneIndex < 2; ++laneIndex)
		{ result.f64[laneIndex] = parseF64(cursor); }
		break;
	default:
		parseErrorf(cursor->parseState,
					cursor->nextToken,
					"expected 'i8x6', 'i16x8', 'i32x4', 'i64x2', 'f32x4', or 'f64x2'");
		throw RecoverParseException();
	};

	return result;
}
