#pragma once

#include <QString>
#include <map>

class Errors
{
public:
	enum Code
	{
		NoError = 0,
		FailedToExtractLinks,
		NetworkError,
		UserErrors = 255
	};

	static QString description(Code code);

private:
	static std::map<Code, QString> m_errors;
};
