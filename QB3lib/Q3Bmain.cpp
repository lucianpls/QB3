#if defined(_WIN32) && defined(QB3_EXPORTS)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#include "../qb3_tables.h"
#include "../QB3encode.h"
#include "../QB3decode.h"
