
#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <string>

namespace Speck
{
	//Exceptions
	class Exception
	{
	protected:
		std::wstring exText;

	public:
		Exception(std::wstring exText) : exText(exText) {}
		virtual ~Exception() {}

		virtual wchar_t *Message() { return &exText[0]; }
	};
}

#define THROW(msg)										\
{														\
    std::wstring finalMsg = std::wstring(msg);			\
	finalMsg += L" (FILE: ";							\
	finalMsg += __FILEW__;								\
	finalMsg += L", LINE: ";							\
	finalMsg += std::to_wstring(__LINE__);				\
	finalMsg += L")";									\
    throw Exception(finalMsg);							\
}

#define THROW_NOT_IMPLEMENTED					\
{												\
	THROW(TEXT("Not implemented!"));			\
}

#endif
