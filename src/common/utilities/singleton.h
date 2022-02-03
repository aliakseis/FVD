#pragma once

#include "utilities/utils.h"

template <class Ty>
class Singleton : public Ty
{
public:
	static Ty& Instance()
	{
		static Ty single_instance;
		return single_instance;
	}

private:
	Singleton();

	DISALLOW_COPY_AND_ASSIGN(Singleton);
};
