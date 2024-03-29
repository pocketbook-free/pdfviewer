//========================================================================
//
// GlobalParams.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifdef ENABLE_PLUGINS
#  ifndef WIN32
#    include <dlfcn.h>
#  endif
#endif
#ifdef WIN32
#  include <shlobj.h>
#endif
#include "goo/gmem.h"
#include "goo/GooString.h"
#include "goo/GooList.h"
#include "goo/GooHash.h"
#include "goo/gfile.h"
#include "Error.h"
#include "NameToCharCode.h"
#include "CharCodeToUnicode.h"
#include "UnicodeMap.h"
#include "CMap.h"
#include "BuiltinFontTables.h"
#include "FontEncodingTables.h"
#ifdef ENABLE_PLUGINS
#  include "XpdfPluginAPI.h"
#endif
#include "GlobalParams.h"
#include "GfxFont.h"

#ifdef WIN32
#  define strcasecmp stricmp
#endif

#if MULTITHREADED
#  define lockGlobalParams            gLockMutex(&mutex)
#  define lockUnicodeMapCache         gLockMutex(&unicodeMapCacheMutex)
#  define lockCMapCache               gLockMutex(&cMapCacheMutex)
#  define unlockGlobalParams          gUnlockMutex(&mutex)
#  define unlockUnicodeMapCache       gUnlockMutex(&unicodeMapCacheMutex)
#  define unlockCMapCache             gUnlockMutex(&cMapCacheMutex)
#else
#  define lockGlobalParams
#  define lockUnicodeMapCache
#  define lockCMapCache
#  define unlockGlobalParams
#  define unlockUnicodeMapCache
#  define unlockCMapCache
#endif

#ifndef FC_WEIGHT_BOOK
#define FC_WEIGHT_BOOK 75
#endif

#include "NameToUnicodeTable.h"
#include "UnicodeMapTables.h"
#include "UTF8.h"

#ifdef ENABLE_PLUGINS
#  ifdef WIN32
extern XpdfPluginVecTable xpdfPluginVecTable;
#  endif
#endif

#include <inkview.h>

//------------------------------------------------------------------------

#define cidToUnicodeCacheSize     4
#define unicodeToUnicodeCacheSize 4

//------------------------------------------------------------------------

GlobalParams *globalParams = NULL;

//------------------------------------------------------------------------
// DisplayFontParam
//------------------------------------------------------------------------

DisplayFontParam::DisplayFontParam(GooString *nameA,
				   DisplayFontParamKind kindA) {
  name = nameA;
  kind = kindA;
  switch (kind) {
  case displayFontT1:
    t1.fileName = NULL;
    break;
  case displayFontTT:
    tt.fileName = NULL;
    break;
  }
}

DisplayFontParam::~DisplayFontParam() {
  delete name;
  switch (kind) {
  case displayFontT1:
    if (t1.fileName) {
      delete t1.fileName;
    }
    break;
  case displayFontTT:
    if (tt.fileName) {
      delete tt.fileName;
    }
    break;
  }
}

#ifdef WIN32

//------------------------------------------------------------------------
// WinFontInfo
//------------------------------------------------------------------------

class WinFontInfo: public DisplayFontParam {
public:

  GBool bold, italic;

  static WinFontInfo *make(GooString *nameA, GBool boldA, GBool italicA,
			   HKEY regKey, char *winFontDir);
  WinFontInfo(GooString *nameA, GBool boldA, GBool italicA,
	      GooString *fileNameA);
  virtual ~WinFontInfo();
  GBool equals(WinFontInfo *fi);
};

WinFontInfo *WinFontInfo::make(GooString *nameA, GBool boldA, GBool italicA,
			       HKEY regKey, char *winFontDir) {
  GooString *regName;
  GooString *fileNameA;
  char buf[MAX_PATH];
  DWORD n;
  char c;
  int i;

  //----- find the font file
  fileNameA = NULL;
  regName = nameA->copy();
  if (boldA) {
    regName->append(" Bold");
  }
  if (italicA) {
    regName->append(" Italic");
  }
  regName->append(" (TrueType)");
  n = sizeof(buf);
  if (RegQueryValueEx(regKey, regName->getCString(), NULL, NULL,
		      (LPBYTE)buf, &n) == ERROR_SUCCESS) {
    fileNameA = new GooString(winFontDir);
    fileNameA->append('\\')->append(buf);
  }
  delete regName;
  if (!fileNameA) {
    delete nameA;
    return NULL;
  }

  //----- normalize the font name
  i = 0;
  while (i < nameA->getLength()) {
    c = nameA->getChar(i);
    if (c == ' ' || c == ',' || c == '-') {
      nameA->del(i);
    } else {
      ++i;
    }
  }

  return new WinFontInfo(nameA, boldA, italicA, fileNameA);
}

WinFontInfo::WinFontInfo(GooString *nameA, GBool boldA, GBool italicA,
			 GooString *fileNameA):
  DisplayFontParam(nameA, displayFontTT)
{
  bold = boldA;
  italic = italicA;
  tt.fileName = fileNameA;
}

WinFontInfo::~WinFontInfo() {
}

GBool WinFontInfo::equals(WinFontInfo *fi) {
  return !name->cmp(fi->name) && bold == fi->bold && italic == fi->italic;
}

//------------------------------------------------------------------------
// WinFontList
//------------------------------------------------------------------------

class WinFontList {
public:

  WinFontList(char *winFontDirA);
  ~WinFontList();
  WinFontInfo *find(GooString *font);

private:

  void add(WinFontInfo *fi);
  static int CALLBACK enumFunc1(CONST LOGFONT *font,
				CONST TEXTMETRIC *metrics,
				DWORD type, LPARAM data);
  static int CALLBACK enumFunc2(CONST LOGFONT *font,
				CONST TEXTMETRIC *metrics,
				DWORD type, LPARAM data);

  GooList *fonts;			// [WinFontInfo]
  HDC dc;			// (only used during enumeration)
  HKEY regKey;			// (only used during enumeration)
  char *winFontDir;		// (only used during enumeration)
};

WinFontList::WinFontList(char *winFontDirA) {
  OSVERSIONINFO version;
  char *path;

  fonts = new GooList();
  dc = GetDC(NULL);
  winFontDir = winFontDirA;
  version.dwOSVersionInfoSize = sizeof(version);
  GetVersionEx(&version);
  if (version.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    path = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts\\";
  } else {
    path = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Fonts\\";
  }
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0,
		   KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
		   &regKey) == ERROR_SUCCESS) {
    EnumFonts(dc, NULL, &WinFontList::enumFunc1, (LPARAM)this);
    RegCloseKey(regKey);
  }
  ReleaseDC(NULL, dc);
}

WinFontList::~WinFontList() {
  deleteGooList(fonts, WinFontInfo);
}

void WinFontList::add(WinFontInfo *fi) {
  int i;

  for (i = 0; i < fonts->getLength(); ++i) {
    if (((WinFontInfo *)fonts->get(i))->equals(fi)) {
      delete fi;
      return;
    }
  }
  fonts->append(fi);
}

WinFontInfo *WinFontList::find(GooString *font) {
  GooString *name;
  GBool bold, italic;
  WinFontInfo *fi;
  char c;
  int n, i;

  name = font->copy();

  // remove space, comma, dash chars
  i = 0;
  while (i < name->getLength()) {
    c = name->getChar(i);
    if (c == ' ' || c == ',' || c == '-') {
      name->del(i);
    } else {
      ++i;
    }
  }
  n = name->getLength();

  // remove trailing "MT" (Foo-MT, Foo-BoldMT, etc.)
  if (!strcmp(name->getCString() + n - 2, "MT")) {
    name->del(n - 2, 2);
    n -= 2;
  }

  // look for "Italic"
  if (!strcmp(name->getCString() + n - 6, "Italic")) {
    name->del(n - 6, 6);
    italic = gTrue;
    n -= 6;
  } else {
    italic = gFalse;
  }

  // look for "Bold"
  if (!strcmp(name->getCString() + n - 4, "Bold")) {
    name->del(n - 4, 4);
    bold = gTrue;
    n -= 4;
  } else {
    bold = gFalse;
  }

  // remove trailing "MT" (FooMT-Bold, etc.)
  if (!strcmp(name->getCString() + n - 2, "MT")) {
    name->del(n - 2, 2);
    n -= 2;
  }

  // remove trailing "PS"
  if (!strcmp(name->getCString() + n - 2, "PS")) {
    name->del(n - 2, 2);
    n -= 2;
  }

  // search for the font
  fi = NULL;
  for (i = 0; i < fonts->getLength(); ++i) {
    fi = (WinFontInfo *)fonts->get(i);
    if (!fi->name->cmp(name) && fi->bold == bold && fi->italic == italic) {
      break;
    }
    fi = NULL;
  }

  delete name;
  return fi;
}

int CALLBACK WinFontList::enumFunc1(CONST LOGFONT *font,
				    CONST TEXTMETRIC *metrics,
				    DWORD type, LPARAM data) {
  WinFontList *fl = (WinFontList *)data;

  EnumFonts(fl->dc, font->lfFaceName, &WinFontList::enumFunc2, (LPARAM)fl);
  return 1;
}

int CALLBACK WinFontList::enumFunc2(CONST LOGFONT *font,
				    CONST TEXTMETRIC *metrics,
				    DWORD type, LPARAM data) {
  WinFontList *fl = (WinFontList *)data;
  WinFontInfo *fi;

  if (type & TRUETYPE_FONTTYPE) {
    if ((fi = WinFontInfo::make(new GooString(font->lfFaceName),
				font->lfWeight >= 600,
				font->lfItalic ? gTrue : gFalse,
				fl->regKey, fl->winFontDir))) {
      fl->add(fi);
    }
  }
  return 1;
}

#endif // WIN32

//------------------------------------------------------------------------
// PSFontParam
//------------------------------------------------------------------------

PSFontParam::PSFontParam(GooString *pdfFontNameA, int wModeA,
			 GooString *psFontNameA, GooString *encodingA) {
  pdfFontName = pdfFontNameA;
  wMode = wModeA;
  psFontName = psFontNameA;
  encoding = encodingA;
}

PSFontParam::~PSFontParam() {
  delete pdfFontName;
  delete psFontName;
  if (encoding) {
    delete encoding;
  }
}

#ifdef ENABLE_PLUGINS
//------------------------------------------------------------------------
// Plugin
//------------------------------------------------------------------------

class Plugin {
public:

  static Plugin *load(char *type, char *name);
  ~Plugin();

private:

#ifdef WIN32
  Plugin(HMODULE libA);
  HMODULE lib;
#else
  Plugin(void *dlA);
  void *dl;
#endif
};

Plugin *Plugin::load(char *type, char *name) {
  GooString *path;
  Plugin *plugin;
  XpdfPluginVecTable *vt;
  XpdfBool (*xpdfInitPlugin)(void);
#ifdef WIN32
  HMODULE libA;
#else
  void *dlA;
#endif

  path = globalParams->getBaseDir();
  appendToPath(path, "plugins");
  appendToPath(path, type);
  appendToPath(path, name);

#ifdef WIN32
  path->append(".dll");
  if (!(libA = LoadLibrary(path->getCString()))) {
    error(-1, "Failed to load plugin '%s'",
	  path->getCString());
    goto err1;
  }
  if (!(vt = (XpdfPluginVecTable *)
	         GetProcAddress(libA, "xpdfPluginVecTable"))) {
    error(-1, "Failed to find xpdfPluginVecTable in plugin '%s'",
	  path->getCString());
    goto err2;
  }
#else
  //~ need to deal with other extensions here
  path->append(".so");
  if (!(dlA = dlopen(path->getCString(), RTLD_NOW))) {
    error(-1, "Failed to load plugin '%s': %s",
	  path->getCString(), dlerror());
    goto err1;
  }
  if (!(vt = (XpdfPluginVecTable *)dlsym(dlA, "xpdfPluginVecTable"))) {
    error(-1, "Failed to find xpdfPluginVecTable in plugin '%s'",
	  path->getCString());
    goto err2;
  }
#endif

  if (vt->version != xpdfPluginVecTable.version) {
    error(-1, "Plugin '%s' is wrong version", path->getCString());
    goto err2;
  }
  memcpy(vt, &xpdfPluginVecTable, sizeof(xpdfPluginVecTable));

#ifdef WIN32
  if (!(xpdfInitPlugin = (XpdfBool (*)(void))
	                     GetProcAddress(libA, "xpdfInitPlugin"))) {
    error(-1, "Failed to find xpdfInitPlugin in plugin '%s'",
	  path->getCString());
    goto err2;
  }
#else
  if (!(xpdfInitPlugin = (XpdfBool (*)(void))dlsym(dlA, "xpdfInitPlugin"))) {
    error(-1, "Failed to find xpdfInitPlugin in plugin '%s'",
	  path->getCString());
    goto err2;
  }
#endif

  if (!(*xpdfInitPlugin)()) {
    error(-1, "Initialization of plugin '%s' failed",
	  path->getCString());
    goto err2;
  }

#ifdef WIN32
  plugin = new Plugin(libA);
#else
  plugin = new Plugin(dlA);
#endif

  delete path;
  return plugin;

 err2:
#ifdef WIN32
  FreeLibrary(libA);
#else
  dlclose(dlA);
#endif
 err1:
  delete path;
  return NULL;
}

#ifdef WIN32
Plugin::Plugin(HMODULE libA) {
  lib = libA;
}
#else
Plugin::Plugin(void *dlA) {
  dl = dlA;
}
#endif

Plugin::~Plugin() {
  void (*xpdfFreePlugin)(void);

#ifdef WIN32
  if ((xpdfFreePlugin = (void (*)(void))
                            GetProcAddress(lib, "xpdfFreePlugin"))) {
    (*xpdfFreePlugin)();
  }
  FreeLibrary(lib);
#else
  if ((xpdfFreePlugin = (void (*)(void))dlsym(dl, "xpdfFreePlugin"))) {
    (*xpdfFreePlugin)();
  }
  dlclose(dl);
#endif
}

#endif // ENABLE_PLUGINS

//------------------------------------------------------------------------
// parsing
//------------------------------------------------------------------------

GlobalParams::GlobalParams() {
  UnicodeMap *map;
  int i;

#ifndef _MSC_VER  
  FcInit();
  FCcfg = FcConfigGetCurrent();
#endif

#if MULTITHREADED
  gInitMutex(&mutex);
  gInitMutex(&unicodeMapCacheMutex);
  gInitMutex(&cMapCacheMutex);
#endif

  initBuiltinFontTables();

  // scan the encoding in reverse because we want the lowest-numbered
  // index for each char name ('space' is encoded twice)
  macRomanReverseMap = new NameToCharCode();
  for (i = 255; i >= 0; --i) {
    if (macRomanEncoding[i]) {
      macRomanReverseMap->add(macRomanEncoding[i], (CharCode)i);
    }
  }

#ifdef WIN32
  // baseDir will be set by a call to setBaseDir
  baseDir = new GooString();
#else
  baseDir = appendToPath(getHomeDir(), ".xpdf");
#endif
  nameToUnicode = new NameToCharCode();
  cidToUnicodes = new GooHash(gTrue);
  unicodeToUnicodes = new GooHash(gTrue);
  residentUnicodeMaps = new GooHash();
  unicodeMaps = new GooHash(gTrue);
  cMapDirs = new GooHash(gTrue);
  toUnicodeDirs = new GooList();
  displayFonts = new GooHash();
  psExpandSmaller = gFalse;
  psShrinkLarger = gTrue;
  psCenter = gTrue;
  psLevel = psLevel2;
  psFonts = new GooHash();
  psNamedFonts16 = new GooList();
  psFonts16 = new GooList();
  psEmbedType1 = gTrue;
  psEmbedTrueType = gTrue;
  psEmbedCIDPostScript = gTrue;
  psEmbedCIDTrueType = gTrue;
  psPreload = gFalse;
  psOPI = gFalse;
  psASCIIHex = gFalse;
  textEncoding = new GooString("UTF-8");
#if defined(WIN32)
  textEOL = eolDOS;
#elif defined(MACOS)
  textEOL = eolMac;
#else
  textEOL = eolUnix;
#endif
  textPageBreaks = gTrue;
  textKeepTinyChars = gFalse;
  fontDirs = new GooList();
  enableFreeType = gTrue;
  enableT1lib = gTrue;
  antialias = gTrue;
  vectorAntialias = gTrue;
  strokeAdjust = gTrue;
  screenType = screenUnset;
  screenSize = -1;
  screenDotRadius = -1;
  screenGamma = 1.0;
  screenBlackThreshold = 0.0;
  screenWhiteThreshold = 1.0;
  mapNumericCharNames = gTrue;
  mapUnknownCharNames = gFalse;
  printCommands = gFalse;
  profileCommands = gFalse;
  errQuiet = gFalse;

  cidToUnicodeCache = new CharCodeToUnicodeCache(cidToUnicodeCacheSize);
  unicodeToUnicodeCache =
      new CharCodeToUnicodeCache(unicodeToUnicodeCacheSize);
  unicodeMapCache = new UnicodeMapCache();
  cMapCache = new CMapCache();

#ifdef WIN32
  winFontList = NULL;
#endif

#ifdef ENABLE_PLUGINS
  plugins = new GooList();
  securityHandlers = new GooList();
#endif

  // set up the initial nameToUnicode table
  for (i = 0; nameToUnicodeTab[i].name; ++i) {
    nameToUnicode->add(nameToUnicodeTab[i].name, nameToUnicodeTab[i].u);
  }

  // set up the residentUnicodeMaps table
  map = new UnicodeMap("Latin1", gFalse,
		       latin1UnicodeMapRanges, latin1UnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("ASCII7", gFalse,
		       ascii7UnicodeMapRanges, ascii7UnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("Symbol", gFalse,
		       symbolUnicodeMapRanges, symbolUnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("ZapfDingbats", gFalse, zapfDingbatsUnicodeMapRanges,
		       zapfDingbatsUnicodeMapLen);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("UTF-8", gTrue, &mapUTF8);
  residentUnicodeMaps->add(map->getEncodingName(), map);
  map = new UnicodeMap("UCS-2", gTrue, &mapUCS2);
  residentUnicodeMaps->add(map->getEncodingName(), map);

  scanEncodingDirs();
}

void GlobalParams::scanEncodingDirs() {
  GDir *dir;
  GDirEntry *entry;

  dir = new GDir(POPPLER_DATADIR "/nameToUnicode", gTrue);
  while (entry = dir->getNextEntry(), entry != NULL) {
    if (!entry->isDir()) {
      parseNameToUnicode(entry->getFullPath());
    }
    delete entry;
  }
  delete dir;

  dir = new GDir(POPPLER_DATADIR "/cidToUnicode", gFalse);
  while (entry = dir->getNextEntry(), entry != NULL) {
    addCIDToUnicode(entry->getName(), entry->getFullPath());
    delete entry;
  }
  delete dir;

  dir = new GDir(POPPLER_DATADIR "/unicodeMap", gFalse);
  while (entry = dir->getNextEntry(), entry != NULL) {
    addUnicodeMap(entry->getName(), entry->getFullPath());
    delete entry;
  }
  delete dir;

  dir = new GDir(POPPLER_DATADIR "/cMap", gFalse);
  while (entry = dir->getNextEntry(), entry != NULL) {
    addCMapDir(entry->getName(), entry->getFullPath());
    toUnicodeDirs->append(entry->getFullPath()->copy());
    delete entry;
  }
  delete dir;
}

void GlobalParams::parseNameToUnicode(GooString *name) {
  char *tok1, *tok2;
  FILE *f;
  char buf[256];
  int line;
  Unicode u;

  if (!(f = iv_fopen(name->getCString(), "r"))) {
    error(-1, "Couldn't open 'nameToUnicode' file '%s'",
	  name->getCString());
    return;
  }
  line = 1;
  while (getLine(buf, sizeof(buf), f)) {
    tok1 = strtok(buf, " \t\r\n");
    tok2 = strtok(NULL, " \t\r\n");
    if (tok1 && tok2) {
      sscanf(tok1, "%x", &u);
      nameToUnicode->add(tok2, u);
    } else {
      error(-1, "Bad line in 'nameToUnicode' file (%s:%d)",
	    name->getCString(), line);
    }
    ++line;
  }
  iv_fclose(f);
}

void GlobalParams::addCIDToUnicode(GooString *collection,
				   GooString *fileName) {
  GooString *old;

  if ((old = (GooString *)cidToUnicodes->remove(collection))) {
    delete old;
  }
  cidToUnicodes->add(collection->copy(), fileName->copy());
}

void GlobalParams::addUnicodeMap(GooString *encodingName, GooString *fileName)
{
  GooString *old;

  if ((old = (GooString *)unicodeMaps->remove(encodingName))) {
    delete old;
  }
  unicodeMaps->add(encodingName->copy(), fileName->copy());
}

void GlobalParams::addCMapDir(GooString *collection, GooString *dir) {
  GooList *list;

  if (!(list = (GooList *)cMapDirs->lookup(collection))) {
    list = new GooList();
    cMapDirs->add(collection->copy(), list);
  }
  list->append(dir->copy());
}

GBool GlobalParams::parseYesNo2(const char *token, GBool *flag) {
  if (!strcmp(token, "yes")) {
    *flag = gTrue;
  } else if (!strcmp(token, "no")) {
    *flag = gFalse;
  } else {
    return gFalse;
  }
  return gTrue;
}

GlobalParams::~GlobalParams() {
  freeBuiltinFontTables();

  delete macRomanReverseMap;

  delete baseDir;
  delete nameToUnicode;
  deleteGooHash(cidToUnicodes, GooString);
  deleteGooHash(unicodeToUnicodes, GooString);
  deleteGooHash(residentUnicodeMaps, UnicodeMap);
  deleteGooHash(unicodeMaps, GooString);
  deleteGooList(toUnicodeDirs, GooString);
  deleteGooHash(displayFonts, DisplayFontParam);
#ifdef WIN32
  delete winFontList;
#endif
  deleteGooHash(psFonts, PSFontParam);
  deleteGooList(psNamedFonts16, PSFontParam);
  deleteGooList(psFonts16, PSFontParam);
  delete textEncoding;
  deleteGooList(fontDirs, GooString);

  GooHashIter *iter;
  GooString *key;
  cMapDirs->startIter(&iter);
  void *val;
  while (cMapDirs->getNext(&iter, &key, &val)) {
    GooList* list = (GooList*)val;
    deleteGooList(list, GooString);
  }
  delete cMapDirs;

  delete cidToUnicodeCache;
  delete unicodeToUnicodeCache;
  delete unicodeMapCache;
  delete cMapCache;

#ifdef ENABLE_PLUGINS
  delete securityHandlers;
  deleteGooList(plugins, Plugin);
#endif

#if MULTITHREADED
  gDestroyMutex(&mutex);
  gDestroyMutex(&unicodeMapCacheMutex);
  gDestroyMutex(&cMapCacheMutex);
#endif
}

//------------------------------------------------------------------------

void GlobalParams::setBaseDir(char *dir) {
  delete baseDir;
  baseDir = new GooString(dir);
}

//------------------------------------------------------------------------
// accessors
//------------------------------------------------------------------------

CharCode GlobalParams::getMacRomanCharCode(char *charName) {
  // no need to lock - macRomanReverseMap is constant
  return macRomanReverseMap->lookup(charName);
}

GooString *GlobalParams::getBaseDir() {
  GooString *s;

  lockGlobalParams;
  s = baseDir->copy();
  unlockGlobalParams;
  return s;
}

Unicode GlobalParams::mapNameToUnicode(char *charName) {
  // no need to lock - nameToUnicode is constant
  return nameToUnicode->lookup(charName);
}

UnicodeMap *GlobalParams::getResidentUnicodeMap(GooString *encodingName) {
  UnicodeMap *map;

  lockGlobalParams;
  map = (UnicodeMap *)residentUnicodeMaps->lookup(encodingName);
  unlockGlobalParams;
  if (map) {
    map->incRefCnt();
  }
  return map;
}

FILE *GlobalParams::getUnicodeMapFile(GooString *encodingName) {
  GooString *fileName;
  FILE *f;

  lockGlobalParams;
  if ((fileName = (GooString *)unicodeMaps->lookup(encodingName))) {
    f = iv_fopen(fileName->getCString(), "r");
  } else {
    f = NULL;
  }
  unlockGlobalParams;
  return f;
}

FILE *GlobalParams::findCMapFile(GooString *collection, GooString *cMapName) {
  GooList *list;
  GooString *dir;
  GooString *fileName;
  FILE *f;
  int i;

  lockGlobalParams;
  if (!(list = (GooList *)cMapDirs->lookup(collection))) {
    unlockGlobalParams;
    return NULL;
  }
  for (i = 0; i < list->getLength(); ++i) {
    dir = (GooString *)list->get(i);
    fileName = appendToPath(dir->copy(), cMapName->getCString());
    f = iv_fopen(fileName->getCString(), "r");
    delete fileName;
    if (f) {
      unlockGlobalParams;
      return f;
    }
  }
  unlockGlobalParams;
  return NULL;
}

FILE *GlobalParams::findToUnicodeFile(GooString *name) {
  GooString *dir, *fileName;
  FILE *f;
  int i;

  lockGlobalParams;
  for (i = 0; i < toUnicodeDirs->getLength(); ++i) {
    dir = (GooString *)toUnicodeDirs->get(i);
    fileName = appendToPath(dir->copy(), name->getCString());
    f = iv_fopen(fileName->getCString(), "r");
    delete fileName;
    if (f) {
      unlockGlobalParams;
      return f;
    }
  }
  unlockGlobalParams;
  return NULL;
}

GBool findModifier(const char *name, const char *modifier, const char **start)
{
  const char *match;

  if (name == NULL)
    return gFalse;

  match = strstr(name, modifier);
  if (match) {
    if (*start == NULL || match < *start)
      *start = match;
    return gTrue;
  }
  else {
    return gFalse;
  }
}

#ifndef _MSC_VER
static FcPattern *buildFcPattern(GfxFont *font)
{
  int weight = -1,
      slant = -1,
      width = -1,
      spacing = -1;
  bool deleteFamily = false;
  char *family, *name, *lang, *modifiers;
  const char *start;
  FcPattern *p;

  // this is all heuristics will be overwritten if font had proper info
  name = font->getName()->getCString();
  
  modifiers = strchr (name, ',');
  if (modifiers == NULL)
    modifiers = strchr (name, '-');
  
  // remove the - from the names, for some reason, Fontconfig does not
  // understand "MS-Mincho" but does with "MS Mincho"
  int len = strlen(name);
  for (int i = 0; i < len; i++)
    name[i] = (name[i] == '-' ? ' ' : name[i]);

  start = NULL;
  findModifier(modifiers, "Regular", &start);
  findModifier(modifiers, "Roman", &start);
  
  if (findModifier(modifiers, "Oblique", &start))
    slant = FC_SLANT_OBLIQUE;
  if (findModifier(modifiers, "Italic", &start))
    slant = FC_SLANT_ITALIC;
  if (findModifier(modifiers, "Bold", &start))
    weight = FC_WEIGHT_BOLD;
  if (findModifier(modifiers, "Light", &start))
    weight = FC_WEIGHT_LIGHT;
  if (findModifier(modifiers, "Condensed", &start))
    width = FC_WIDTH_CONDENSED;
  
  if (start) {
    // There have been "modifiers" in the name, crop them to obtain
    // the family name
    family = new char[len+1];
    strcpy(family, name);
    int pos = (modifiers - name);
    family[pos] = '\0';
    deleteFamily = true;
  }
  else {
    family = name;
  }
  
  // use font flags
  if (font->isFixedWidth())
    spacing = FC_MONO;
  if (font->isBold())
    weight = FC_WEIGHT_BOLD;
  if (font->isItalic())
    slant = FC_SLANT_ITALIC;
  
  // if the FontDescriptor specified a family name use it
  if (font->getFamily()) {
    if (deleteFamily) {
      delete[] family;
      deleteFamily = false;
    }
    family = font->getFamily()->getCString();
  }
  
  // if the FontDescriptor specified a weight use it
  switch (font -> getWeight())
  {
    case GfxFont::W100: weight = FC_WEIGHT_EXTRALIGHT; break; 
    case GfxFont::W200: weight = FC_WEIGHT_LIGHT; break; 
    case GfxFont::W300: weight = FC_WEIGHT_BOOK; break; 
    case GfxFont::W400: weight = FC_WEIGHT_NORMAL; break; 
    case GfxFont::W500: weight = FC_WEIGHT_MEDIUM; break; 
    case GfxFont::W600: weight = FC_WEIGHT_DEMIBOLD; break; 
    case GfxFont::W700: weight = FC_WEIGHT_BOLD; break; 
    case GfxFont::W800: weight = FC_WEIGHT_EXTRABOLD; break; 
    case GfxFont::W900: weight = FC_WEIGHT_BLACK; break; 
    default: break; 
  }
  
  // if the FontDescriptor specified a width use it
  switch (font -> getStretch())
  {
    case GfxFont::UltraCondensed: width = FC_WIDTH_ULTRACONDENSED; break; 
    case GfxFont::ExtraCondensed: width = FC_WIDTH_EXTRACONDENSED; break; 
    case GfxFont::Condensed: width = FC_WIDTH_CONDENSED; break; 
    case GfxFont::SemiCondensed: width = FC_WIDTH_SEMICONDENSED; break; 
    case GfxFont::Normal: width = FC_WIDTH_NORMAL; break; 
    case GfxFont::SemiExpanded: width = FC_WIDTH_SEMIEXPANDED; break; 
    case GfxFont::Expanded: width = FC_WIDTH_EXPANDED; break; 
    case GfxFont::ExtraExpanded: width = FC_WIDTH_EXTRAEXPANDED; break; 
    case GfxFont::UltraExpanded: width = FC_WIDTH_ULTRAEXPANDED; break; 
    default: break; 
  }
  
  // find the language we want the font to support
  if (font->isCIDFont())
  {
    GooString *collection = ((GfxCIDFont *)font)->getCollection();
    if (collection)
    {
      if (strcmp(collection->getCString(), "Adobe-GB1") == 0)
		lang = (char *) "zh-cn"; // Simplified Chinese
      else if (strcmp(collection->getCString(), "Adobe-CNS1") == 0)
		lang = (char *) "zh-tw"; // Traditional Chinese
      else if (strcmp(collection->getCString(), "Adobe-Japan1") == 0)
		lang = (char *) "ja"; // Japanese
      else if (strcmp(collection->getCString(), "Adobe-Japan2") == 0)
		lang = (char *) "ja"; // Japanese
      else if (strcmp(collection->getCString(), "Adobe-Korea1") == 0)
		lang = (char *) "ko"; // Korean
      else if (strcmp(collection->getCString(), "Adobe-UCS") == 0)
		lang = (char *) "xx";
      else if (strcmp(collection->getCString(), "Adobe-Identity") == 0)
		lang = (char *) "xx";
      else
      {
        error(-1, "Unknown CID font collection, please report to poppler bugzilla.");
		lang = (char *) "xx";
      }
    }
	else lang = (char *) "xx";
  }
  else lang = (char *) "xx";
  
  p = FcPatternBuild(NULL,
                    FC_FAMILY, FcTypeString, family,
                    FC_LANG, FcTypeString, lang,
                    NULL);
  if (slant != -1) FcPatternAddInteger(p, FC_SLANT, slant);
  if (weight != -1) FcPatternAddInteger(p, FC_WEIGHT, weight);
  if (width != -1) FcPatternAddInteger(p, FC_WIDTH, width);
  if (spacing != -1) FcPatternAddInteger(p, FC_SPACING, spacing);

  if (deleteFamily)
    delete[] family;
  return p;
}
#endif

/* if you can't or don't want to use Fontconfig, you need to implement
   this function for your platform. For Windows, it's in GlobalParamsWin.cc
*/
#ifndef _MSC_VER
DisplayFontParam *GlobalParams::getDisplayFont(GfxFont *font) {
  DisplayFontParam *dfp;
  FcPattern *p=0;

  GooString *fontName = font->getName();
  if (!fontName) return NULL;
  
  lockGlobalParams;
  dfp = font->dfp;
  if (!dfp)
  {
    FcChar8* s;
    char * ext;
    FcResult res;
    FcFontSet *set;
    int i;
    p = buildFcPattern(font);

    if (!p)
      goto fin;
    FcConfigSubstitute(FCcfg, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    set = FcFontSort(FCcfg, p, FcFalse, NULL, &res);
    if (!set)
      goto fin;
    for (i = 0; i < set->nfont; ++i)
    {
      res = FcPatternGetString(set->fonts[i], FC_FILE, 0, &s);
      if (res != FcResultMatch || !s)
        continue;
      ext = strrchr((char*)s,'.');
      if (!ext)
        continue;
      if (!strncasecmp(ext,".ttf",4) || !strncasecmp(ext, ".ttc", 4))
      {
        dfp = new DisplayFontParam(fontName->copy(), displayFontTT);  
        dfp->tt.fileName = new GooString((char*)s);
        FcPatternGetInteger(set->fonts[i], FC_INDEX, 0, &(dfp->tt.faceIndex));
      }
      else if (!strncasecmp(ext,".pfa",4) || !strncasecmp(ext,".pfb",4)) 
      {
        dfp = new DisplayFontParam(fontName->copy(), displayFontT1);  
        dfp->t1.fileName = new GooString((char*)s);
      }
      else
        continue;
      font->dfp = dfp;
      break;
    }
    FcFontSetDestroy(set);
  }
fin:
  if (p)
    FcPatternDestroy(p);

  unlockGlobalParams;
  return dfp;
}
#endif

GBool GlobalParams::getPSExpandSmaller() {
  GBool f;

  lockGlobalParams;
  f = psExpandSmaller;
  unlockGlobalParams;
  return f;
}

GBool GlobalParams::getPSShrinkLarger() {
  GBool f;

  lockGlobalParams;
  f = psShrinkLarger;
  unlockGlobalParams;
  return f;
}

GBool GlobalParams::getPSCenter() {
  GBool f;

  lockGlobalParams;
  f = psCenter;
  unlockGlobalParams;
  return f;
}

PSLevel GlobalParams::getPSLevel() {
  PSLevel level;

  lockGlobalParams;
  level = psLevel;
  unlockGlobalParams;
  return level;
}

PSFontParam *GlobalParams::getPSFont(GooString *fontName) {
  PSFontParam *p;

  lockGlobalParams;
  p = (PSFontParam *)psFonts->lookup(fontName);
  unlockGlobalParams;
  return p;
}

PSFontParam *GlobalParams::getPSFont16(GooString *fontName,
				       GooString *collection, int wMode) {
  PSFontParam *p;
  int i;

  lockGlobalParams;
  p = NULL;
  if (fontName) {
    for (i = 0; i < psNamedFonts16->getLength(); ++i) {
      p = (PSFontParam *)psNamedFonts16->get(i);
      if (!p->pdfFontName->cmp(fontName) &&
	  p->wMode == wMode) {
	break;
      }
      p = NULL;
    }
  }
  if (!p && collection) {
    for (i = 0; i < psFonts16->getLength(); ++i) {
      p = (PSFontParam *)psFonts16->get(i);
      if (!p->pdfFontName->cmp(collection) &&
	  p->wMode == wMode) {
	break;
      }
      p = NULL;
    }
  }
  unlockGlobalParams;
  return p;
}

GBool GlobalParams::getPSEmbedType1() {
  GBool e;

  lockGlobalParams;
  e = psEmbedType1;
  unlockGlobalParams;
  return e;
}

GBool GlobalParams::getPSEmbedTrueType() {
  GBool e;

  lockGlobalParams;
  e = psEmbedTrueType;
  unlockGlobalParams;
  return e;
}

GBool GlobalParams::getPSEmbedCIDPostScript() {
  GBool e;

  lockGlobalParams;
  e = psEmbedCIDPostScript;
  unlockGlobalParams;
  return e;
}

GBool GlobalParams::getPSEmbedCIDTrueType() {
  GBool e;

  lockGlobalParams;
  e = psEmbedCIDTrueType;
  unlockGlobalParams;
  return e;
}

GBool GlobalParams::getPSPreload() {
  GBool preload;

  lockGlobalParams;
  preload = psPreload;
  unlockGlobalParams;
  return preload;
}

GBool GlobalParams::getPSOPI() {
  GBool opi;

  lockGlobalParams;
  opi = psOPI;
  unlockGlobalParams;
  return opi;
}

GBool GlobalParams::getPSASCIIHex() {
  GBool ah;

  lockGlobalParams;
  ah = psASCIIHex;
  unlockGlobalParams;
  return ah;
}

GooString *GlobalParams::getTextEncodingName() {
  GooString *s;

  lockGlobalParams;
  s = textEncoding->copy();
  unlockGlobalParams;
  return s;
}

EndOfLineKind GlobalParams::getTextEOL() {
  EndOfLineKind eol;

  lockGlobalParams;
  eol = textEOL;
  unlockGlobalParams;
  return eol;
}

GBool GlobalParams::getTextPageBreaks() {
  GBool pageBreaks;

  lockGlobalParams;
  pageBreaks = textPageBreaks;
  unlockGlobalParams;
  return pageBreaks;
}

GBool GlobalParams::getTextKeepTinyChars() {
  GBool tiny;

  lockGlobalParams;
  tiny = textKeepTinyChars;
  unlockGlobalParams;
  return tiny;
}

GooString *GlobalParams::findFontFile(GooString *fontName, const char **exts) {
  GooString *dir, *fileName;
  char **ext;
  FILE *f;
  int i;

  lockGlobalParams;
  for (i = 0; i < fontDirs->getLength(); ++i) {
    dir = (GooString *)fontDirs->get(i);
	for (ext = (char **) exts; *ext; ++ext) {
      fileName = appendToPath(dir->copy(), fontName->getCString());
      fileName->append(*ext);
      if ((f = iv_fopen(fileName->getCString(), "rb"))) {
	iv_fclose(f);
	unlockGlobalParams;
	return fileName;
      }
      delete fileName;
    }
  }
  unlockGlobalParams;
  return NULL;
}

GBool GlobalParams::getEnableFreeType() {
  GBool f;

  lockGlobalParams;
  f = enableFreeType;
  unlockGlobalParams;
  return f;
}

GBool GlobalParams::getEnableT1lib() {
  GBool f;

  lockGlobalParams;
  f = enableT1lib;
  unlockGlobalParams;
  return f;
}


GBool GlobalParams::getAntialias() {
  GBool f;

  lockGlobalParams;
  f = antialias;
  unlockGlobalParams;
  return f;
}

GBool GlobalParams::getVectorAntialias() {
  GBool f;

  lockGlobalParams;
  f = vectorAntialias;
  unlockGlobalParams;
  return f;
}

GBool GlobalParams::getStrokeAdjust() {
  GBool f;

  lockGlobalParams;
  f = strokeAdjust;
  unlockGlobalParams;
  return f;
}

ScreenType GlobalParams::getScreenType() {
  ScreenType t;

  lockGlobalParams;
  t = screenType;
  unlockGlobalParams;
  return t;
}

int GlobalParams::getScreenSize() {
  int size;

  lockGlobalParams;
  size = screenSize;
  unlockGlobalParams;
  return size;
}

int GlobalParams::getScreenDotRadius() {
  int r;

  lockGlobalParams;
  r = screenDotRadius;
  unlockGlobalParams;
  return r;
}

double GlobalParams::getScreenGamma() {
  double gamma;

  lockGlobalParams;
  gamma = screenGamma;
  unlockGlobalParams;
  return gamma;
}

double GlobalParams::getScreenBlackThreshold() {
  double thresh;

  lockGlobalParams;
  thresh = screenBlackThreshold;
  unlockGlobalParams;
  return thresh;
}

double GlobalParams::getScreenWhiteThreshold() {
  double thresh;

  lockGlobalParams;
  thresh = screenWhiteThreshold;
  unlockGlobalParams;
  return thresh;
}

GBool GlobalParams::getMapNumericCharNames() {
  GBool map;

  lockGlobalParams;
  map = mapNumericCharNames;
  unlockGlobalParams;
  return map;
}

GBool GlobalParams::getMapUnknownCharNames() {
  GBool map;

  lockGlobalParams;
  map = mapUnknownCharNames;
  unlockGlobalParams;
  return map;
}

GBool GlobalParams::getPrintCommands() {
  GBool p;

  lockGlobalParams;
  p = printCommands;
  unlockGlobalParams;
  return p;
}

GBool GlobalParams::getProfileCommands() {
  GBool p;

  lockGlobalParams;
  p = profileCommands;
  unlockGlobalParams;
  return p;
}

GBool GlobalParams::getErrQuiet() {
  // no locking -- this function may get called from inside a locked
  // section
  return errQuiet;
}

CharCodeToUnicode *GlobalParams::getCIDToUnicode(GooString *collection) {
  GooString *fileName;
  CharCodeToUnicode *ctu;

  lockGlobalParams;
  if (!(ctu = cidToUnicodeCache->getCharCodeToUnicode(collection))) {
    if ((fileName = (GooString *)cidToUnicodes->lookup(collection)) &&
	(ctu = CharCodeToUnicode::parseCIDToUnicode(fileName, collection))) {
      cidToUnicodeCache->add(ctu);
    }
  }
  unlockGlobalParams;
  return ctu;
}

CharCodeToUnicode *GlobalParams::getUnicodeToUnicode(GooString *fontName) {
  lockGlobalParams;
  GooHashIter *iter;
  unicodeToUnicodes->startIter(&iter);
  GooString *fileName = NULL;
  GooString *fontPattern;
  void *val;
  while (!fileName && unicodeToUnicodes->getNext(&iter, &fontPattern, &val)) {
    if (strstr(fontName->getCString(), fontPattern->getCString())) {
      unicodeToUnicodes->killIter(&iter);
      fileName = (GooString*)val;
    }
  }
  CharCodeToUnicode *ctu = NULL;
  if (fileName) {
    ctu = unicodeToUnicodeCache->getCharCodeToUnicode(fileName);
    if (!ctu) {
      ctu = CharCodeToUnicode::parseUnicodeToUnicode(fileName);
      if (ctu)
         unicodeToUnicodeCache->add(ctu);
    }
  }
  unlockGlobalParams;
  return ctu;
}

UnicodeMap *GlobalParams::getUnicodeMap(GooString *encodingName) {
  return getUnicodeMap2(encodingName);
}

UnicodeMap *GlobalParams::getUnicodeMap2(GooString *encodingName) {
  UnicodeMap *map;

  if (!(map = getResidentUnicodeMap(encodingName))) {
    lockUnicodeMapCache;
    map = unicodeMapCache->getUnicodeMap(encodingName);
    unlockUnicodeMapCache;
  }
  return map;
}

CMap *GlobalParams::getCMap(GooString *collection, GooString *cMapName) {
  CMap *cMap;

  lockCMapCache;
  cMap = cMapCache->getCMap(collection, cMapName);
  unlockCMapCache;
  return cMap;
}

UnicodeMap *GlobalParams::getTextEncoding() {
  return getUnicodeMap2(textEncoding);
}

//------------------------------------------------------------------------
// functions to set parameters
//------------------------------------------------------------------------

void GlobalParams::setPSExpandSmaller(GBool expand) {
  lockGlobalParams;
  psExpandSmaller = expand;
  unlockGlobalParams;
}

void GlobalParams::setPSShrinkLarger(GBool shrink) {
  lockGlobalParams;
  psShrinkLarger = shrink;
  unlockGlobalParams;
}

void GlobalParams::setPSCenter(GBool center) {
  lockGlobalParams;
  psCenter = center;
  unlockGlobalParams;
}

void GlobalParams::setPSLevel(PSLevel level) {
  lockGlobalParams;
  psLevel = level;
  unlockGlobalParams;
}

void GlobalParams::setPSEmbedType1(GBool embed) {
  lockGlobalParams;
  psEmbedType1 = embed;
  unlockGlobalParams;
}

void GlobalParams::setPSEmbedTrueType(GBool embed) {
  lockGlobalParams;
  psEmbedTrueType = embed;
  unlockGlobalParams;
}

void GlobalParams::setPSEmbedCIDPostScript(GBool embed) {
  lockGlobalParams;
  psEmbedCIDPostScript = embed;
  unlockGlobalParams;
}

void GlobalParams::setPSEmbedCIDTrueType(GBool embed) {
  lockGlobalParams;
  psEmbedCIDTrueType = embed;
  unlockGlobalParams;
}

void GlobalParams::setPSPreload(GBool preload) {
  lockGlobalParams;
  psPreload = preload;
  unlockGlobalParams;
}

void GlobalParams::setPSOPI(GBool opi) {
  lockGlobalParams;
  psOPI = opi;
  unlockGlobalParams;
}

void GlobalParams::setPSASCIIHex(GBool hex) {
  lockGlobalParams;
  psASCIIHex = hex;
  unlockGlobalParams;
}

void GlobalParams::setTextEncoding(char *encodingName) {
  lockGlobalParams;
  delete textEncoding;
  textEncoding = new GooString(encodingName);
  unlockGlobalParams;
}

GBool GlobalParams::setTextEOL(char *s) {
  lockGlobalParams;
  if (!strcmp(s, "unix")) {
    textEOL = eolUnix;
  } else if (!strcmp(s, "dos")) {
    textEOL = eolDOS;
  } else if (!strcmp(s, "mac")) {
    textEOL = eolMac;
  } else {
    unlockGlobalParams;
    return gFalse;
  }
  unlockGlobalParams;
  return gTrue;
}

void GlobalParams::setTextPageBreaks(GBool pageBreaks) {
  lockGlobalParams;
  textPageBreaks = pageBreaks;
  unlockGlobalParams;
}

void GlobalParams::setTextKeepTinyChars(GBool keep) {
  lockGlobalParams;
  textKeepTinyChars = keep;
  unlockGlobalParams;
}

GBool GlobalParams::setEnableFreeType(const char *s) {
  GBool ok;

  lockGlobalParams;
  ok = parseYesNo2(s, &enableFreeType);
  unlockGlobalParams;
  return ok;
}


GBool GlobalParams::setAntialias(char *s) {
  GBool ok;

  lockGlobalParams;
  ok = parseYesNo2(s, &antialias);
  unlockGlobalParams;
  return ok;
}

GBool GlobalParams::setVectorAntialias(char *s) {
  GBool ok;

  lockGlobalParams;
  ok = parseYesNo2(s, &vectorAntialias);
  unlockGlobalParams;
  return ok;
}

void GlobalParams::setStrokeAdjust(GBool adjust)
{
  lockGlobalParams;
  strokeAdjust = adjust;
  unlockGlobalParams;
}

void GlobalParams::setScreenType(ScreenType st)
{
  lockGlobalParams;
  screenType = st;
  unlockGlobalParams;
}

void GlobalParams::setScreenSize(int size)
{
  lockGlobalParams;
  screenSize = size;
  unlockGlobalParams;
}

void GlobalParams::setScreenDotRadius(int radius)
{
  lockGlobalParams;
  screenDotRadius = radius;
  unlockGlobalParams;
}

void GlobalParams::setScreenGamma(double gamma)
{
  lockGlobalParams;
  screenGamma = gamma;
  unlockGlobalParams;
}

void GlobalParams::setScreenBlackThreshold(double blackThreshold)
{
  lockGlobalParams;
  screenBlackThreshold = blackThreshold;
  unlockGlobalParams;
}

void GlobalParams::setScreenWhiteThreshold(double whiteThreshold)
{
  lockGlobalParams;
  screenWhiteThreshold = whiteThreshold;
  unlockGlobalParams;
}

void GlobalParams::setMapNumericCharNames(GBool map) {
  lockGlobalParams;
  mapNumericCharNames = map;
  unlockGlobalParams;
}

void GlobalParams::setMapUnknownCharNames(GBool map) {
  lockGlobalParams;
  mapUnknownCharNames = map;
  unlockGlobalParams;
}

void GlobalParams::setPrintCommands(GBool printCommandsA) {
  lockGlobalParams;
  printCommands = printCommandsA;
  unlockGlobalParams;
}

void GlobalParams::setProfileCommands(GBool profileCommandsA) {
  lockGlobalParams;
  profileCommands = profileCommandsA;
  unlockGlobalParams;
}

void GlobalParams::setErrQuiet(GBool errQuietA) {
  lockGlobalParams;
  errQuiet = errQuietA;
  unlockGlobalParams;
}

void GlobalParams::addSecurityHandler(XpdfSecurityHandler *handler) {
#ifdef ENABLE_PLUGINS
  lockGlobalParams;
  securityHandlers->append(handler);
  unlockGlobalParams;
#endif
}

XpdfSecurityHandler *GlobalParams::getSecurityHandler(char *name) {
#ifdef ENABLE_PLUGINS
  XpdfSecurityHandler *hdlr;
  int i;

  lockGlobalParams;
  for (i = 0; i < securityHandlers->getLength(); ++i) {
    hdlr = (XpdfSecurityHandler *)securityHandlers->get(i);
    if (!strcasecmp(hdlr->name, name)) {
      unlockGlobalParams;
      return hdlr;
    }
  }
  unlockGlobalParams;

  if (!loadPlugin("security", name)) {
    return NULL;
  }
  deleteGooList(keyBindings, KeyBinding);

  lockGlobalParams;
  for (i = 0; i < securityHandlers->getLength(); ++i) {
    hdlr = (XpdfSecurityHandler *)securityHandlers->get(i);
    if (!strcmp(hdlr->name, name)) {
      unlockGlobalParams;
      return hdlr;
    }
  }
  unlockGlobalParams;
#else
  (void)name;
#endif

  return NULL;
}

#ifdef ENABLE_PLUGINS
//------------------------------------------------------------------------
// plugins
//------------------------------------------------------------------------

GBool GlobalParams::loadPlugin(char *type, char *name) {
  Plugin *plugin;

  if (!(plugin = Plugin::load(type, name))) {
    return gFalse;
  }
  lockGlobalParams;
  plugins->append(plugin);
  unlockGlobalParams;
  return gTrue;
}

#endif // ENABLE_PLUGINS
