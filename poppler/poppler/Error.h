//========================================================================
//
// Error.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef ERROR_H
#define ERROR_H

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdarg.h>
#include "poppler-config.h"

extern void CDECL error(int pos, const char *msg, ...) GCC_PRINTF_FORMAT (2, 3);
void warning(const char *msg, ...) GCC_PRINTF_FORMAT (1, 2);

void setErrorFunction(void (* f)(int , const char *, va_list args));

#endif
