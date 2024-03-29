//========================================================================
//
// FoFiBase.cc
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <stdio.h>
#include "goo/gmem.h"
#include "poppler/Error.h"
#include "FoFiBase.h"
#include <inkview.h>

//------------------------------------------------------------------------
// FoFiBase
//------------------------------------------------------------------------

FoFiBase::FoFiBase(char *fileA, int lenA, GBool freeFileDataA) {
  fileData = file = (Guchar *)fileA;
  len = lenA;
  freeFileData = freeFileDataA;
}

FoFiBase::~FoFiBase() {
  if (freeFileData) {
    gfree(fileData);
  }
}

char *FoFiBase::readFile(char *fileName, int *fileLen) {
  FILE *f;
  char *buf;
  int n;

  if (!(f = iv_fopen(fileName, "rb"))) {
    error(-1, "Cannot open '%s'", fileName);
    return NULL;
  }
  if (iv_fseek(f, 0, SEEK_END) != 0) {
    error(-1, "Cannot seek to end of '%s'", fileName);
    fclose(f);
    return NULL;
  }
  n = (int)iv_ftell(f);
  if (iv_fseek(f, 0, SEEK_SET) != 0) {
    error(-1, "Cannot seek to start of '%s'", fileName);
    iv_fclose(f);
    return NULL;
  }
  buf = (char *)gmalloc(n);
  if ((int)iv_fread(buf, 1, n, f) != n) {
    gfree(buf);
    iv_fclose(f);
    return NULL;
  }
  iv_fclose(f);
  *fileLen = n;
  return buf;
}

int FoFiBase::getS8(int pos, GBool *ok) {
  int x;

  if (pos < 0 || pos >= len) {
    *ok = gFalse;
    return 0;
  }
  x = file[pos];
  if (x & 0x80) {
    x |= ~0xff;
  }
  return x;
}

int FoFiBase::getU8(int pos, GBool *ok) {
  if (pos < 0 || pos >= len) {
    *ok = gFalse;
    return 0;
  }
  return file[pos];
}

int FoFiBase::getS16BE(int pos, GBool *ok) {
  int x;

  if (pos < 0 || pos+1 >= len) {
    *ok = gFalse;
    return 0;
  }
  x = file[pos];
  x = (x << 8) + file[pos+1];
  if (x & 0x8000) {
    x |= ~0xffff;
  }
  return x;
}

int FoFiBase::getU16BE(int pos, GBool *ok) {
  int x;

  if (pos < 0 || pos+1 >= len) {
    *ok = gFalse;
    return 0;
  }
  x = file[pos];
  x = (x << 8) + file[pos+1];
  return x;
}

int FoFiBase::getS32BE(int pos, GBool *ok) {
  int x;

  if (pos < 0 || pos+3 >= len) {
    *ok = gFalse;
    return 0;
  }
  x = file[pos];
  x = (x << 8) + file[pos+1];
  x = (x << 8) + file[pos+2];
  x = (x << 8) + file[pos+3];
  if (x & 0x80000000) {
    x |= ~0xffffffff;
  }
  return x;
}

Guint FoFiBase::getU32BE(int pos, GBool *ok) {
  Guint x;

  if (pos < 0 || pos+3 >= len) {
    *ok = gFalse;
    return 0;
  }
  x = file[pos];
  x = (x << 8) + file[pos+1];
  x = (x << 8) + file[pos+2];
  x = (x << 8) + file[pos+3];
  return x;
}

Guint FoFiBase::getUVarBE(int pos, int size, GBool *ok) {
  Guint x;
  int i;

  if (pos < 0 || pos + size > len) {
    *ok = gFalse;
    return 0;
  }
  x = 0;
  for (i = 0; i < size; ++i) {
    x = (x << 8) + file[pos + i];
  }
  return x;
}

GBool FoFiBase::checkRegion(int pos, int size) {
  return pos >= 0 &&
         pos + size >= pos &&
         pos + size <= len;
}
