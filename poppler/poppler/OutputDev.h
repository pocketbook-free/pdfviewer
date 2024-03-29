//========================================================================
//
// OutputDev.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef OUTPUTDEV_H
#define OUTPUTDEV_H

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <poppler-config.h>
#include "goo/gtypes.h"
#include "goo/FixedPoint.h"
#include "CharTypes.h"

class Dict;
class GooHash;
class GooString;
class GfxState;
struct GfxColor;
class GfxColorSpace;
class GfxImageColorMap;
class GfxFunctionShading;
class GfxAxialShading;
class GfxRadialShading;
class Stream;
class Links;
class Link;
class Catalog;
class Page;
class Function;

//------------------------------------------------------------------------
// OutputDev
//------------------------------------------------------------------------

class OutputDev {
public:

  // Constructor.
  OutputDev() { profileHash = NULL; }

  // Destructor.
  virtual ~OutputDev() {}

  //----- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  virtual GBool upsideDown() = 0;

  // Does this device use drawChar() or drawString()?
  virtual GBool useDrawChar() = 0;

  // Does this device use tilingPatternFill()?  If this returns false,
  // tiling pattern fills will be reduced to a series of other drawing
  // operations.
  virtual GBool useTilingPatternFill() { return gFalse; }

  // Does this device use functionShadedFill(), axialShadedFill(), and
  // radialShadedFill()?  If this returns false, these shaded fills
  // will be reduced to a series of other drawing operations.
  virtual GBool useShadedFills() { return gFalse; }

  // Does this device use drawForm()?  If this returns false,
  // form-type XObjects will be interpreted (i.e., unrolled).
  virtual GBool useDrawForm() { return gFalse; }

  // Does this device use beginType3Char/endType3Char?  Otherwise,
  // text in Type 3 fonts will be drawn with drawChar/drawString.
  virtual GBool interpretType3Chars() = 0;

  // Does this device need non-text content?
  virtual GBool needNonText() { return gTrue; }

  //----- initialization and control

  // Set default transform matrix.
  virtual void setDefaultCTM(FixedPoint *ctm);

  // Check to see if a page slice should be displayed.  If this
  // returns false, the page display is aborted.  Typically, an
  // OutputDev will use some alternate means to display the page
  // before returning false.
  virtual GBool checkPageSlice(Page *page, FixedPoint hDPI, FixedPoint vDPI,
			       int rotate, GBool useMediaBox, GBool crop,
			       int sliceX, int sliceY, int sliceW, int sliceH,
			       GBool printing, Catalog * catalog,
			       GBool (* abortCheckCbk)(void *data) = NULL,
			       void * abortCheckCbkData = NULL)
    { return gTrue; }

  // Start a page.
  virtual void startPage(int pageNum, GfxState *state) {}

  // End a page.
  virtual void endPage() {}

  // Dump page contents to display.
  virtual void dump() {}

  //----- coordinate conversion

  // Convert between device and user coordinates.
  virtual void cvtDevToUser(FixedPoint dx, FixedPoint dy, FixedPoint *ux, FixedPoint *uy);
  virtual void cvtUserToDev(FixedPoint ux, FixedPoint uy, int *dx, int *dy);

  FixedPoint *getDefCTM() { return defCTM; }
  FixedPoint *getDefICTM() { return defICTM; }

  //----- save/restore graphics state
  virtual void saveState(GfxState * /*state*/) {}
  virtual void restoreState(GfxState * /*state*/) {}

  //----- update graphics state
  virtual void updateAll(GfxState *state);
  virtual void updateCTM(GfxState * /*state*/, FixedPoint /*m11*/, FixedPoint /*m12*/,
			 FixedPoint /*m21*/, FixedPoint /*m22*/, FixedPoint /*m31*/, FixedPoint /*m32*/) {}
  virtual void updateLineDash(GfxState * /*state*/) {}
  virtual void updateFlatness(GfxState * /*state*/) {}
  virtual void updateLineJoin(GfxState * /*state*/) {}
  virtual void updateLineCap(GfxState * /*state*/) {}
  virtual void updateMiterLimit(GfxState * /*state*/) {}
  virtual void updateLineWidth(GfxState * /*state*/) {}
  virtual void updateStrokeAdjust(GfxState * /*state*/) {}
  virtual void updateAlphaIsShape(GfxState * /*state*/) {}
  virtual void updateTextKnockout(GfxState * /*state*/) {}
  virtual void updateFillColorSpace(GfxState * /*state*/) {}
  virtual void updateStrokeColorSpace(GfxState * /*state*/) {}
  virtual void updateFillColor(GfxState * /*state*/) {}
  virtual void updateStrokeColor(GfxState * /*state*/) {}
  virtual void updateBlendMode(GfxState * /*state*/) {}
  virtual void updateFillOpacity(GfxState * /*state*/) {}
  virtual void updateStrokeOpacity(GfxState * /*state*/) {}
  virtual void updateFillOverprint(GfxState * /*state*/) {}
  virtual void updateStrokeOverprint(GfxState * /*state*/) {}
  virtual void updateTransfer(GfxState * /*state*/) {}

  //----- update text state
  virtual void updateFont(GfxState * /*state*/) {}
  virtual void updateTextMat(GfxState * /*state*/) {}
  virtual void updateCharSpace(GfxState * /*state*/) {}
  virtual void updateRender(GfxState * /*state*/) {}
  virtual void updateRise(GfxState * /*state*/) {}
  virtual void updateWordSpace(GfxState * /*state*/) {}
  virtual void updateHorizScaling(GfxState * /*state*/) {}
  virtual void updateTextPos(GfxState * /*state*/) {}
  virtual void updateTextShift(GfxState * /*state*/, FixedPoint /*shift*/) {}

  //----- path painting
  virtual void stroke(GfxState * /*state*/) {}
  virtual void fill(GfxState * /*state*/) {}
  virtual void eoFill(GfxState * /*state*/) {}
  virtual void tilingPatternFill(GfxState * /*state*/, Object * /*str*/,
				 int /*paintType*/, Dict * /*resDict*/,
				 FixedPoint * /*mat*/, FixedPoint * /*bbox*/,
				 int /*x0*/, int /*y0*/, int /*x1*/, int /*y1*/,
				 FixedPoint /*xStep*/, FixedPoint /*yStep*/) {}
  virtual GBool functionShadedFill(GfxState * /*state*/,
				   GfxFunctionShading * /*shading*/)
    { return gFalse; }
  virtual GBool axialShadedFill(GfxState * /*state*/, GfxAxialShading * /*shading*/)
    { return gFalse; }
  virtual GBool radialShadedFill(GfxState * /*state*/, GfxRadialShading * /*shading*/)
    { return gFalse; }

  //----- path clipping
  virtual void clip(GfxState * /*state*/) {}
  virtual void eoClip(GfxState * /*state*/) {}
  virtual void clipToStrokePath(GfxState * /*state*/) {}

  //----- text drawing
  virtual void beginStringOp(GfxState * /*state*/) {}
  virtual void endStringOp(GfxState * /*state*/) {}
  virtual void beginString(GfxState * /*state*/, GooString * /*s*/) {}
  virtual void endString(GfxState * /*state*/) {}
  virtual void drawChar(GfxState * /*state*/, FixedPoint /*x*/, FixedPoint /*y*/,
			FixedPoint /*dx*/, FixedPoint /*dy*/,
			FixedPoint /*originX*/, FixedPoint /*originY*/,
			CharCode /*code*/, int /*nBytes*/, Unicode * /*u*/, int /*uLen*/) {}
  virtual void drawString(GfxState * /*state*/, GooString * /*s*/) {}
  virtual GBool beginType3Char(GfxState * /*state*/, FixedPoint /*x*/, FixedPoint /*y*/,
			       FixedPoint /*dx*/, FixedPoint /*dy*/,
			       CharCode /*code*/, Unicode * /*u*/, int /*uLen*/);
  virtual void endType3Char(GfxState * /*state*/) {}
  virtual void endTextObject(GfxState * /*state*/) {}

  //----- image drawing
  virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
			     int width, int height, GBool invert,
			     GBool inlineImg);
  virtual void drawImage(GfxState *state, Object *ref, Stream *str,
			 int width, int height, GfxImageColorMap *colorMap,
			 int *maskColors, GBool inlineImg);
  virtual void drawMaskedImage(GfxState *state, Object *ref, Stream *str,
			       int width, int height,
			       GfxImageColorMap *colorMap,
			       Stream *maskStr, int maskWidth, int maskHeight,
			       GBool maskInvert);
  virtual void drawSoftMaskedImage(GfxState *state, Object *ref, Stream *str,
				   int width, int height,
				   GfxImageColorMap *colorMap,
				   Stream *maskStr,
				   int maskWidth, int maskHeight,
				   GfxImageColorMap *maskColorMap);

  //----- grouping operators

  virtual void endMarkedContent(GfxState *state);
  virtual void beginMarkedContent(char *name);
  virtual void beginMarkedContent(char *name, Dict *properties);
  virtual void markPoint(char *name);
  virtual void markPoint(char *name, Dict *properties);
  
  

#if OPI_SUPPORT
  //----- OPI functions
  virtual void opiBegin(GfxState *state, Dict *opiDict);
  virtual void opiEnd(GfxState *state, Dict *opiDict);
#endif

  //----- Type 3 font operators
  virtual void type3D0(GfxState * /*state*/, FixedPoint /*wx*/, FixedPoint /*wy*/) {}
  virtual void type3D1(GfxState * /*state*/, FixedPoint /*wx*/, FixedPoint /*wy*/,
		       FixedPoint /*llx*/, FixedPoint /*lly*/, FixedPoint /*urx*/, FixedPoint /*ury*/) {}

  //----- form XObjects
  virtual void drawForm(Ref /*id*/) {}

  //----- PostScript XObjects
  virtual void psXObject(Stream * /*psStream*/, Stream * /*level1Stream*/) {}

  //----- Profiling
  virtual void startProfile();
  virtual GooHash *getProfileHash() {return profileHash; }
  virtual GooHash *endProfile();

  //----- transparency groups and soft masks
  virtual void beginTransparencyGroup(GfxState * /*state*/, FixedPoint * /*bbox*/,
				      GfxColorSpace * /*blendingColorSpace*/,
				      GBool /*isolated*/, GBool /*knockout*/,
				      GBool /*forSoftMask*/) {}
  virtual void endTransparencyGroup(GfxState * /*state*/) {}
  virtual void paintTransparencyGroup(GfxState * /*state*/, FixedPoint * /*bbox*/) {}
  virtual void setSoftMask(GfxState * /*state*/, FixedPoint * /*bbox*/, GBool /*alpha*/,
			   Function * /*transferFunc*/, GfxColor * /*backdropColor*/) {}
  virtual void clearSoftMask(GfxState * /*state*/) {}

  //----- links
  virtual void processLink(Link * /*link*/, Catalog * /*catalog*/) {}

#if 1 //~tmp: turn off anti-aliasing temporarily
  virtual GBool getVectorAntialias() { return gFalse; }
  virtual void setVectorAntialias(GBool /*vaa*/) {}
#endif

private:

  FixedPoint defCTM[6];		// default coordinate transform matrix
  FixedPoint defICTM[6];		// inverse of default CTM
  GooHash *profileHash;
};

#endif
