
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkAdvancedTypefaceMetrics.h"
#include "SkBase64.h"
#include "SkColorPriv.h"
#include "SkData.h"
#include "SkDescriptor.h"
#include "SkFontDescriptor.h"
#include "SkFontHost.h"
#include "SkGlyph.h"
#include "SkMaskGamma.h"
#include "SkOTUtils.h"
#include "SkPath.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkThread.h"
#include "SkTypeface_win.h"
#include "SkTypefaceCache.h"
#include "SkUtils.h"

#include "SkTypes.h"
#include <tchar.h>
#include <usp10.h>
#include <objbase.h>

static void (*gEnsureLOGFONTAccessibleProc)(const LOGFONT&);

void SkTypeface_SetEnsureLOGFONTAccessibleProc(void (*proc)(const LOGFONT&)) {
    gEnsureLOGFONTAccessibleProc = proc;
}

static void call_ensure_accessible(const LOGFONT& lf) {
    if (gEnsureLOGFONTAccessibleProc) {
        gEnsureLOGFONTAccessibleProc(lf);
    }
}

///////////////////////////////////////////////////////////////////////////////

// always packed xxRRGGBB
typedef uint32_t SkGdiRGB;

template <typename T> T* SkTAddByteOffset(T* ptr, size_t byteOffset) {
    return (T*)((char*)ptr + byteOffset);
}

// define this in your Makefile or .gyp to enforce AA requests
// which GDI ignores at small sizes. This flag guarantees AA
// for rotated text, regardless of GDI's notions.
//#define SK_ENFORCE_ROTATED_TEXT_AA_ON_WINDOWS

// client3d has to undefine this for now
#define CAN_USE_LOGFONT_NAME

static bool isLCD(const SkScalerContext::Rec& rec) {
    return SkMask::kLCD16_Format == rec.fMaskFormat ||
           SkMask::kLCD32_Format == rec.fMaskFormat;
}

static bool bothZero(SkScalar a, SkScalar b) {
    return 0 == a && 0 == b;
}

// returns false if there is any non-90-rotation or skew
static bool isAxisAligned(const SkScalerContext::Rec& rec) {
    return 0 == rec.fPreSkewX &&
           (bothZero(rec.fPost2x2[0][1], rec.fPost2x2[1][0]) ||
            bothZero(rec.fPost2x2[0][0], rec.fPost2x2[1][1]));
}

static bool needToRenderWithSkia(const SkScalerContext::Rec& rec) {
#ifdef SK_ENFORCE_ROTATED_TEXT_AA_ON_WINDOWS
    // What we really want to catch is when GDI will ignore the AA request and give
    // us BW instead. Smallish rotated text is one heuristic, so this code is just
    // an approximation. We shouldn't need to do this for larger sizes, but at those
    // sizes, the quality difference gets less and less between our general
    // scanconverter and GDI's.
    if (SkMask::kA8_Format == rec.fMaskFormat && !isAxisAligned(rec)) {
        return true;
    }
#endif
    // false means allow GDI to generate the bits
    return false;
}

using namespace skia_advanced_typeface_metrics_utils;

static const uint16_t BUFFERSIZE = (16384 - 32);
static uint8_t glyphbuf[BUFFERSIZE];

/**
 *  Since LOGFONT wants its textsize as an int, and we support fractional sizes,
 *  and since we have a cache of LOGFONTs for our tyepfaces, we always set the
 *  lfHeight to a canonical size, and then we use the 2x2 matrix to achieve the
 *  actual requested size.
 */
static const int gCanonicalTextSize = 64;

static void tchar_to_skstring(const TCHAR t[], SkString* s) {
#ifdef UNICODE
    size_t sSize = WideCharToMultiByte(CP_UTF8, 0, t, -1, NULL, 0, NULL, NULL);
    s->resize(sSize);
    WideCharToMultiByte(CP_UTF8, 0, t, -1, s->writable_str(), sSize, NULL, NULL);
#else
    s->set(t);
#endif
}

static void make_canonical(LOGFONT* lf) {
    lf->lfHeight = -gCanonicalTextSize;
    lf->lfQuality = CLEARTYPE_QUALITY;//PROOF_QUALITY;
    lf->lfCharSet = DEFAULT_CHARSET;
//    lf->lfClipPrecision = 64;
}

static SkTypeface::Style get_style(const LOGFONT& lf) {
    unsigned style = 0;
    if (lf.lfWeight >= FW_BOLD) {
        style |= SkTypeface::kBold;
    }
    if (lf.lfItalic) {
        style |= SkTypeface::kItalic;
    }
    return static_cast<SkTypeface::Style>(style);
}

static void setStyle(LOGFONT* lf, SkTypeface::Style style) {
    lf->lfWeight = (style & SkTypeface::kBold) != 0 ? FW_BOLD : FW_NORMAL ;
    lf->lfItalic = ((style & SkTypeface::kItalic) != 0);
}

static inline FIXED SkFixedToFIXED(SkFixed x) {
    return *(FIXED*)(&x);
}
static inline SkFixed SkFIXEDToFixed(FIXED x) {
    return *(SkFixed*)(&x);
}

static inline FIXED SkScalarToFIXED(SkScalar x) {
    return SkFixedToFIXED(SkScalarToFixed(x));
}

static unsigned calculateOutlineGlyphCount(HDC hdc) {
    // The 'maxp' table stores the number of glyphs at offset 4, in 2 bytes.
    const DWORD maxpTag =
        SkEndian_SwapBE32(SkSetFourByteTag('m', 'a', 'x', 'p'));
    uint16_t glyphs;
    if (GetFontData(hdc, maxpTag, 4, &glyphs, sizeof(glyphs)) != GDI_ERROR) {
        return SkEndian_SwapBE16(glyphs);
    }

    // Binary search for glyph count.
    static const MAT2 mat2 = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
    int32_t max = SK_MaxU16 + 1;
    int32_t min = 0;
    GLYPHMETRICS gm;
    while (min < max) {
        int32_t mid = min + ((max - min) / 2);
        if (GetGlyphOutlineW(hdc, mid, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0,
                             NULL, &mat2) == GDI_ERROR) {
            max = mid;
        } else {
            min = mid + 1;
        }
    }
    SkASSERT(min == max);
    return min;
}

class LogFontTypeface : public SkTypeface {
public:
    LogFontTypeface(SkTypeface::Style style, SkFontID fontID, const LOGFONT& lf, bool serializeAsStream = false) :
        SkTypeface(style, fontID, false), fLogFont(lf), fSerializeAsStream(serializeAsStream) {

        // If the font has cubic outlines, it will not be rendered with ClearType.
        HFONT font = CreateFontIndirect(&lf);

        HDC deviceContext = ::CreateCompatibleDC(NULL);
        HFONT savefont = (HFONT)SelectObject(deviceContext, font);

        TEXTMETRIC textMetric;
        if (0 == GetTextMetrics(deviceContext, &textMetric)) {
            call_ensure_accessible(lf);
            if (0 == GetTextMetrics(deviceContext, &textMetric)) {
                textMetric.tmPitchAndFamily = TMPF_TRUETYPE;
            }
        }
        if (deviceContext) {
            ::SelectObject(deviceContext, savefont);
            ::DeleteDC(deviceContext);
        }
        if (font) {
            ::DeleteObject(font);
        }

        // The fixed pitch bit is set if the font is *not* fixed pitch.
        this->setIsFixedPitch((textMetric.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

        // Used a logfont on a memory context, should never get a device font.
        // Therefore all TMPF_DEVICE will be PostScript (cubic) fonts.
        fCanBeLCD = !((textMetric.tmPitchAndFamily & TMPF_VECTOR) &&
                      (textMetric.tmPitchAndFamily & TMPF_DEVICE));
    }

    void getFamilyName(SkString* name) const {
        tchar_to_skstring(fLogFont.lfFaceName, name);
    }

    LOGFONT fLogFont;
    bool fSerializeAsStream;
    bool fCanBeLCD;

    static LogFontTypeface* Create(const LOGFONT& lf) {
        SkTypeface::Style style = get_style(lf);
        SkFontID fontID = SkTypefaceCache::NewFontID();
        return new LogFontTypeface(style, fontID, lf);
    }

    static void EnsureAccessible(const SkTypeface* face) {
        call_ensure_accessible(static_cast<const LogFontTypeface*>(face)->fLogFont);
    }

protected:
    virtual SkStream* onOpenStream(int* ttcIndex) const SK_OVERRIDE;
    virtual SkScalerContext* onCreateScalerContext(const SkDescriptor*) const SK_OVERRIDE;
    virtual void onFilterRec(SkScalerContextRec*) const SK_OVERRIDE;
    virtual SkAdvancedTypefaceMetrics* onGetAdvancedTypefaceMetrics(
                                SkAdvancedTypefaceMetrics::PerGlyphInfo,
                                const uint32_t*, uint32_t) const SK_OVERRIDE;
    virtual void onGetFontDescriptor(SkFontDescriptor*, bool*) const SK_OVERRIDE;
};

class FontMemResourceTypeface : public LogFontTypeface {
public:
    /**
     *  Takes ownership of fontMemResource.
     */
    FontMemResourceTypeface(SkTypeface::Style style, SkFontID fontID, const LOGFONT& lf, HANDLE fontMemResource) :
        LogFontTypeface(style, fontID, lf, true), fFontMemResource(fontMemResource) {
    }

    HANDLE fFontMemResource;

    /**
     *  The created FontMemResourceTypeface takes ownership of fontMemResource.
     */
    static FontMemResourceTypeface* Create(const LOGFONT& lf, HANDLE fontMemResource) {
        SkTypeface::Style style = get_style(lf);
        SkFontID fontID = SkTypefaceCache::NewFontID();
        return new FontMemResourceTypeface(style, fontID, lf, fontMemResource);
    }

protected:
    virtual void weak_dispose() const SK_OVERRIDE {
        RemoveFontMemResourceEx(fFontMemResource);
        //SkTypefaceCache::Remove(this);
        INHERITED::weak_dispose();
    }

private:
    typedef LogFontTypeface INHERITED;
};

static const LOGFONT& get_default_font() {
    static LOGFONT gDefaultFont;
    return gDefaultFont;
}

static bool FindByLogFont(SkTypeface* face, SkTypeface::Style requestedStyle, void* ctx) {
    LogFontTypeface* lface = static_cast<LogFontTypeface*>(face);
    const LOGFONT* lf = reinterpret_cast<const LOGFONT*>(ctx);

    return lface &&
           get_style(lface->fLogFont) == requestedStyle &&
           !memcmp(&lface->fLogFont, lf, sizeof(LOGFONT));
}

/**
 *  This guy is public. It first searches the cache, and if a match is not found,
 *  it creates a new face.
 */
SkTypeface* SkCreateTypefaceFromLOGFONT(const LOGFONT& origLF) {
    LOGFONT lf = origLF;
    make_canonical(&lf);
    SkTypeface* face = SkTypefaceCache::FindByProcAndRef(FindByLogFont, &lf);
    if (NULL == face) {
        face = LogFontTypeface::Create(lf);
        SkTypefaceCache::Add(face, get_style(lf));
    }
    return face;
}

/**
 *  The created SkTypeface takes ownership of fontMemResource.
 */
SkTypeface* SkCreateFontMemResourceTypefaceFromLOGFONT(const LOGFONT& origLF, HANDLE fontMemResource) {
    LOGFONT lf = origLF;
    make_canonical(&lf);
    FontMemResourceTypeface* face = FontMemResourceTypeface::Create(lf, fontMemResource);
    SkTypefaceCache::Add(face, get_style(lf), false);
    return face;
}

/**
 *  This guy is public
 */
void SkLOGFONTFromTypeface(const SkTypeface* face, LOGFONT* lf) {
    if (NULL == face) {
        *lf = get_default_font();
    } else {
        *lf = static_cast<const LogFontTypeface*>(face)->fLogFont;
    }
}

static void GetLogFontByID(SkFontID fontID, LOGFONT* lf) {
    LogFontTypeface* face = static_cast<LogFontTypeface*>(SkTypefaceCache::FindByID(fontID));
    if (face) {
        *lf = face->fLogFont;
    } else {
        sk_bzero(lf, sizeof(LOGFONT));
    }
}

// Construct Glyph to Unicode table.
// Unicode code points that require conjugate pairs in utf16 are not
// supported.
// TODO(arthurhsu): Add support for conjugate pairs. It looks like that may
// require parsing the TTF cmap table (platform 4, encoding 12) directly instead
// of calling GetFontUnicodeRange().
static void populate_glyph_to_unicode(HDC fontHdc, const unsigned glyphCount,
                                      SkTDArray<SkUnichar>* glyphToUnicode) {
    DWORD glyphSetBufferSize = GetFontUnicodeRanges(fontHdc, NULL);
    if (!glyphSetBufferSize) {
        return;
    }

    SkAutoTDeleteArray<BYTE> glyphSetBuffer(new BYTE[glyphSetBufferSize]);
    GLYPHSET* glyphSet =
        reinterpret_cast<LPGLYPHSET>(glyphSetBuffer.get());
    if (GetFontUnicodeRanges(fontHdc, glyphSet) != glyphSetBufferSize) {
        return;
    }

    glyphToUnicode->setCount(glyphCount);
    memset(glyphToUnicode->begin(), 0, glyphCount * sizeof(SkUnichar));
    for (DWORD i = 0; i < glyphSet->cRanges; ++i) {
        // There is no guarantee that within a Unicode range, the corresponding
        // glyph id in a font file are continuous. So, even if we have ranges,
        // we can't just use the first and last entry of the range to compute
        // result. We need to enumerate them one by one.
        int count = glyphSet->ranges[i].cGlyphs;
        SkAutoTArray<WCHAR> chars(count + 1);
        chars[count] = 0;  // termintate string
        SkAutoTArray<WORD> glyph(count);
        for (USHORT j = 0; j < count; ++j) {
            chars[j] = glyphSet->ranges[i].wcLow + j;
        }
        GetGlyphIndicesW(fontHdc, chars.get(), count, glyph.get(),
                         GGI_MARK_NONEXISTING_GLYPHS);
        // If the glyph ID is valid, and the glyph is not mapped, then we will
        // fill in the char id into the vector. If the glyph is mapped already,
        // skip it.
        // TODO(arthurhsu): better improve this. e.g. Get all used char ids from
        // font cache, then generate this mapping table from there. It's
        // unlikely to have collisions since glyph reuse happens mostly for
        // different Unicode pages.
        for (USHORT j = 0; j < count; ++j) {
            if (glyph[j] != 0xffff && glyph[j] < glyphCount &&
                (*glyphToUnicode)[glyph[j]] == 0) {
                (*glyphToUnicode)[glyph[j]] = chars[j];
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////

static int alignTo32(int n) {
    return (n + 31) & ~31;
}

struct MyBitmapInfo : public BITMAPINFO {
    RGBQUAD fMoreSpaceForColors[1];
};

class HDCOffscreen {
public:
    HDCOffscreen() {
        fFont = 0;
        fDC = 0;
        fBM = 0;
        fBits = NULL;
        fWidth = fHeight = 0;
        fIsBW = false;
    }

    ~HDCOffscreen() {
        if (fDC) {
            DeleteDC(fDC);
        }
        if (fBM) {
            DeleteObject(fBM);
        }
    }

    void init(HFONT font, const XFORM& xform) {
        fFont = font;
        fXform = xform;
    }

    const void* draw(const SkGlyph&, bool isBW, size_t* srcRBPtr);

private:
    HDC     fDC;
    HBITMAP fBM;
    HFONT   fFont;
    XFORM   fXform;
    void*   fBits;  // points into fBM
    int     fWidth;
    int     fHeight;
    bool    fIsBW;

    enum {
        // will always trigger us to reset the color, since we
        // should only store 0 or 0x00FFFFFF or gray (0x007F7F7F)
        kInvalid_Color = 12345
    };
};

const void* HDCOffscreen::draw(const SkGlyph& glyph, bool isBW,
                               size_t* srcRBPtr) {
    if (0 == fDC) {
        fDC = CreateCompatibleDC(0);
        if (0 == fDC) {
            return NULL;
        }
        SetGraphicsMode(fDC, GM_ADVANCED);
        SetBkMode(fDC, TRANSPARENT);
        SetTextAlign(fDC, TA_LEFT | TA_BASELINE);
        SelectObject(fDC, fFont);

        COLORREF color = 0x00FFFFFF;
        SkDEBUGCODE(COLORREF prev =) SetTextColor(fDC, color);
        SkASSERT(prev != CLR_INVALID);
    }

    if (fBM && (fIsBW != isBW || fWidth < glyph.fWidth || fHeight < glyph.fHeight)) {
        DeleteObject(fBM);
        fBM = 0;
    }
    fIsBW = isBW;

    fWidth = SkMax32(fWidth, glyph.fWidth);
    fHeight = SkMax32(fHeight, glyph.fHeight);

    int biWidth = isBW ? alignTo32(fWidth) : fWidth;

    if (0 == fBM) {
        MyBitmapInfo info;
        sk_bzero(&info, sizeof(info));
        if (isBW) {
            RGBQUAD blackQuad = { 0, 0, 0, 0 };
            RGBQUAD whiteQuad = { 0xFF, 0xFF, 0xFF, 0 };
            info.bmiColors[0] = blackQuad;
            info.bmiColors[1] = whiteQuad;
        }
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = biWidth;
        info.bmiHeader.biHeight = fHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = isBW ? 1 : 32;
        info.bmiHeader.biCompression = BI_RGB;
        if (isBW) {
            info.bmiHeader.biClrUsed = 2;
        }
        fBM = CreateDIBSection(fDC, &info, DIB_RGB_COLORS, &fBits, 0, 0);
        if (0 == fBM) {
            return NULL;
        }
        SelectObject(fDC, fBM);
    }

    // erase
    size_t srcRB = isBW ? (biWidth >> 3) : (fWidth << 2);
    size_t size = fHeight * srcRB;
    memset(fBits, 0, size);

    XFORM xform = fXform;
    xform.eDx = (float)-glyph.fLeft;
    xform.eDy = (float)-glyph.fTop;
    SetWorldTransform(fDC, &xform);

    uint16_t glyphID = glyph.getGlyphID();
    BOOL ret = ExtTextOutW(fDC, 0, 0, ETO_GLYPH_INDEX, NULL, reinterpret_cast<LPCWSTR>(&glyphID), 1, NULL);
    GdiFlush();
    if (0 == ret) {
        return NULL;
    }
    *srcRBPtr = srcRB;
    // offset to the start of the image
    return (const char*)fBits + (fHeight - glyph.fHeight) * srcRB;
}

//////////////////////////////////////////////////////////////////////////////

class SkScalerContext_Windows : public SkScalerContext {
public:
    SkScalerContext_Windows(SkTypeface*, const SkDescriptor* desc);
    virtual ~SkScalerContext_Windows();

protected:
    virtual unsigned generateGlyphCount() SK_OVERRIDE;
    virtual uint16_t generateCharToGlyph(SkUnichar uni) SK_OVERRIDE;
    virtual void generateAdvance(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateMetrics(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateImage(const SkGlyph& glyph) SK_OVERRIDE;
    virtual void generatePath(const SkGlyph& glyph, SkPath* path) SK_OVERRIDE;
    virtual void generateFontMetrics(SkPaint::FontMetrics* mX,
                                     SkPaint::FontMetrics* mY) SK_OVERRIDE;

private:
    HDCOffscreen fOffscreen;
    SkScalar     fScale;  // to get from canonical size to real size
    MAT2         fMat22;
    HDC          fDDC;
    HFONT        fSavefont;
    HFONT        fFont;
    SCRIPT_CACHE fSC;
    int          fGlyphCount;

    HFONT        fHiResFont;
    MAT2         fMat22Identity;
    SkMatrix     fHiResMatrix;
    enum Type {
        kTrueType_Type, kBitmap_Type,
    } fType;
    TEXTMETRIC fTM;
};

static float mul2float(SkScalar a, SkScalar b) {
    return SkScalarToFloat(SkScalarMul(a, b));
}

static FIXED float2FIXED(float x) {
    return SkFixedToFIXED(SkFloatToFixed(x));
}

SK_DECLARE_STATIC_MUTEX(gFTMutex);

#define HIRES_TEXTSIZE  2048
#define HIRES_SHIFT     11
static inline SkFixed HiResToFixed(int value) {
    return value << (16 - HIRES_SHIFT);
}

static bool needHiResMetrics(const SkScalar mat[2][2]) {
    return mat[1][0] || mat[0][1];
}

static BYTE compute_quality(const SkScalerContext::Rec& rec) {
    switch (rec.fMaskFormat) {
        case SkMask::kBW_Format:
            return NONANTIALIASED_QUALITY;
        case SkMask::kLCD16_Format:
        case SkMask::kLCD32_Format:
            return CLEARTYPE_QUALITY;
        default:
            if (rec.fFlags & SkScalerContext::kGenA8FromLCD_Flag) {
                return CLEARTYPE_QUALITY;
            } else {
                return ANTIALIASED_QUALITY;
            }
    }
}

SkScalerContext_Windows::SkScalerContext_Windows(SkTypeface* rawTypeface,
                                                 const SkDescriptor* desc)
        : SkScalerContext(rawTypeface, desc)
        , fDDC(0)
        , fFont(0)
        , fSavefont(0)
        , fSC(0)
        , fGlyphCount(-1) {
    SkAutoMutexAcquire  ac(gFTMutex);

    LogFontTypeface* typeface = reinterpret_cast<LogFontTypeface*>(rawTypeface);

    fDDC = ::CreateCompatibleDC(NULL);
    SetGraphicsMode(fDDC, GM_ADVANCED);
    SetBkMode(fDDC, TRANSPARENT);

    // Scaling by the DPI is inconsistent with how Skia draws elsewhere
    //SkScalar height = -(fRec.fTextSize * GetDeviceCaps(ddc, LOGPIXELSY) / 72);
    LOGFONT lf = typeface->fLogFont;
    lf.lfHeight = -gCanonicalTextSize;
    lf.lfQuality = compute_quality(fRec);
    fFont = CreateFontIndirect(&lf);

    // if we're rotated, or want fractional widths, create a hires font
    fHiResFont = 0;
    if (needHiResMetrics(fRec.fPost2x2)) {
        lf.lfHeight = -HIRES_TEXTSIZE;
        fHiResFont = CreateFontIndirect(&lf);

        fMat22Identity.eM11 = fMat22Identity.eM22 = SkFixedToFIXED(SK_Fixed1);
        fMat22Identity.eM12 = fMat22Identity.eM21 = SkFixedToFIXED(0);

        // construct a matrix to go from HIRES logical units to our device units
        fRec.getSingleMatrix(&fHiResMatrix);
        SkScalar scale = SkScalarInvert(SkIntToScalar(HIRES_TEXTSIZE));
        fHiResMatrix.preScale(scale, scale);
    }
    fSavefont = (HFONT)SelectObject(fDDC, fFont);

    if (0 == GetTextMetrics(fDDC, &fTM)) {
        call_ensure_accessible(lf);
        if (0 == GetTextMetrics(fDDC, &fTM)) {
            fTM.tmPitchAndFamily = TMPF_TRUETYPE;
        }
    }
    // Used a logfont on a memory context, should never get a device font.
    // Therefore all TMPF_DEVICE will be PostScript fonts.

    // If TMPF_VECTOR is set, one of TMPF_TRUETYPE or TMPF_DEVICE must be set,
    // otherwise we have a vector FON, which we don't support.
    // This was determined by testing with Type1 PFM/PFB and OpenTypeCFF OTF,
    // as well as looking at Wine bugs and sources.
    SkASSERT(!(fTM.tmPitchAndFamily & TMPF_VECTOR) ||
              (fTM.tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_DEVICE)));

    XFORM xform;
    if (fTM.tmPitchAndFamily & TMPF_VECTOR) {
        // Truetype or PostScript.
        // Stroked FON also gets here (TMPF_VECTOR), but we don't handle it.
        fType = SkScalerContext_Windows::kTrueType_Type;
        fScale = fRec.fTextSize / gCanonicalTextSize;

        // fPost2x2 is column-major, left handed (y down).
        // XFORM 2x2 is row-major, left handed (y down).
        xform.eM11 = mul2float(fScale, fRec.fPost2x2[0][0]);
        xform.eM12 = mul2float(fScale, fRec.fPost2x2[1][0]);
        xform.eM21 = mul2float(fScale, fRec.fPost2x2[0][1]);
        xform.eM22 = mul2float(fScale, fRec.fPost2x2[1][1]);
        xform.eDx = 0;
        xform.eDy = 0;

        // MAT2 is row major, right handed (y up).
        fMat22.eM11 = float2FIXED(xform.eM11);
        fMat22.eM12 = float2FIXED(-xform.eM12);
        fMat22.eM21 = float2FIXED(-xform.eM21);
        fMat22.eM22 = float2FIXED(xform.eM22);

        if (needToRenderWithSkia(fRec)) {
            this->forceGenerateImageFromPath();
        }

    } else {
        // Assume bitmap
        fType = SkScalerContext_Windows::kBitmap_Type;
        fScale = SK_Scalar1;

        xform.eM11 = 1.0f;
        xform.eM12 = 0.0f;
        xform.eM21 = 0.0f;
        xform.eM22 = 1.0f;
        xform.eDx = 0.0f;
        xform.eDy = 0.0f;

        // fPost2x2 is column-major, left handed (y down).
        // MAT2 is row major, right handed (y up).
        fMat22.eM11 = SkScalarToFIXED(fRec.fPost2x2[0][0]);
        fMat22.eM12 = SkScalarToFIXED(-fRec.fPost2x2[1][0]);
        fMat22.eM21 = SkScalarToFIXED(-fRec.fPost2x2[0][1]);
        fMat22.eM22 = SkScalarToFIXED(fRec.fPost2x2[1][1]);

        lf.lfHeight = -SkScalarCeilToInt(fRec.fTextSize);
        HFONT bitmapFont = CreateFontIndirect(&lf);
        SelectObject(fDDC, bitmapFont);
        ::DeleteObject(fFont);
        fFont = bitmapFont;

        if (0 == GetTextMetrics(fDDC, &fTM)) {
            call_ensure_accessible(lf);
            //if the following fails, we'll just draw at gCanonicalTextSize.
            GetTextMetrics(fDDC, &fTM);
        }
    }

    fOffscreen.init(fFont, xform);
}

SkScalerContext_Windows::~SkScalerContext_Windows() {
    if (fDDC) {
        ::SelectObject(fDDC, fSavefont);
        ::DeleteDC(fDDC);
    }
    if (fFont) {
        ::DeleteObject(fFont);
    }
    if (fHiResFont) {
        ::DeleteObject(fHiResFont);
    }
    if (fSC) {
        ::ScriptFreeCache(&fSC);
    }
}

unsigned SkScalerContext_Windows::generateGlyphCount() {
    if (fGlyphCount < 0) {
        if (fType == SkScalerContext_Windows::kBitmap_Type) {
           return fTM.tmLastChar;
        }
        fGlyphCount = calculateOutlineGlyphCount(fDDC);
    }
    return fGlyphCount;
}

uint16_t SkScalerContext_Windows::generateCharToGlyph(SkUnichar uni) {
    uint16_t index = 0;
    WCHAR c[2];
    // TODO(ctguil): Support characters that generate more than one glyph.
    if (SkUTF16_FromUnichar(uni, (uint16_t*)c) == 1) {
        // Type1 fonts fail with uniscribe API. Use GetGlyphIndices for plane 0.
        SkAssertResult(GetGlyphIndicesW(fDDC, c, 1, &index, 0));
    } else {
        // Use uniscribe to detemine glyph index for non-BMP characters.
        // Need to add extra item to SCRIPT_ITEM to work around a bug in older
        // windows versions. https://bugzilla.mozilla.org/show_bug.cgi?id=366643
        SCRIPT_ITEM si[2 + 1];
        int items;
        SkAssertResult(
            SUCCEEDED(ScriptItemize(c, 2, 2, NULL, NULL, si, &items)));

        WORD log[2];
        SCRIPT_VISATTR vsa;
        int glyphs;
        SkAssertResult(SUCCEEDED(ScriptShape(
            fDDC, &fSC, c, 2, 1, &si[0].a, &index, log, &vsa, &glyphs)));
    }
    return index;
}

void SkScalerContext_Windows::generateAdvance(SkGlyph* glyph) {
    this->generateMetrics(glyph);
}

void SkScalerContext_Windows::generateMetrics(SkGlyph* glyph) {
    SkASSERT(fDDC);

    if (fType == SkScalerContext_Windows::kBitmap_Type) {
        SIZE size;
        WORD glyphs = glyph->getGlyphID(0);
        if (0 == GetTextExtentPointI(fDDC, &glyphs, 1, &size)) {
            glyph->fWidth = SkToS16(fTM.tmMaxCharWidth);
        } else {
            glyph->fWidth = SkToS16(size.cx);
        }
        glyph->fHeight = SkToS16(size.cy);

        glyph->fTop = SkToS16(-fTM.tmAscent);
        glyph->fLeft = SkToS16(0);
        glyph->fAdvanceX = SkIntToFixed(glyph->fWidth);
        glyph->fAdvanceY = 0;

        // Apply matrix to advance.
        glyph->fAdvanceY = SkFixedMul(SkFIXEDToFixed(fMat22.eM21), glyph->fAdvanceX);
        glyph->fAdvanceX = SkFixedMul(SkFIXEDToFixed(fMat22.eM11), glyph->fAdvanceX);

        return;
    }

    UINT glyphId = glyph->getGlyphID(0);

    GLYPHMETRICS gm;
    sk_bzero(&gm, sizeof(gm));

    DWORD status = GetGlyphOutlineW(fDDC, glyphId, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &fMat22);
    if (GDI_ERROR == status) {
        LogFontTypeface::EnsureAccessible(this->getTypeface());
        status = GetGlyphOutlineW(fDDC, glyphId, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &fMat22);
        if (GDI_ERROR == status) {
            glyph->zeroMetrics();
            return;
        }
    }

    bool empty = false;
    // The black box is either the embedded bitmap size or the outline extent.
    // It is 1x1 if nothing is to be drawn, but will also be 1x1 if something very small
    // is to be drawn, like a '.'. We need to outset '.' but do not wish to outset ' '.
    if (1 == gm.gmBlackBoxX && 1 == gm.gmBlackBoxY) {
        // If GetGlyphOutline with GGO_NATIVE returns 0, we know there was no outline.
        DWORD bufferSize = GetGlyphOutlineW(fDDC, glyphId, GGO_NATIVE | GGO_GLYPH_INDEX, &gm, 0, NULL, &fMat22);
        empty = (0 == bufferSize);
    }

    glyph->fTop = SkToS16(-gm.gmptGlyphOrigin.y);
    glyph->fLeft = SkToS16(gm.gmptGlyphOrigin.x);
    if (empty) {
        glyph->fWidth = 0;
        glyph->fHeight = 0;
    } else {
        // Outset, since the image may bleed out of the black box.
        // For embedded bitmaps the black box should be exact.
        // For outlines we need to outset by 1 in all directions for bleed.
        // For ClearType we need to outset by 2 for bleed.
        glyph->fWidth = gm.gmBlackBoxX + 4;
        glyph->fHeight = gm.gmBlackBoxY + 4;
        glyph->fTop -= 2;
        glyph->fLeft -= 2;
    }
    glyph->fAdvanceX = SkIntToFixed(gm.gmCellIncX);
    glyph->fAdvanceY = SkIntToFixed(gm.gmCellIncY);
    glyph->fRsbDelta = 0;
    glyph->fLsbDelta = 0;

    if (fHiResFont) {
        SelectObject(fDDC, fHiResFont);
        sk_bzero(&gm, sizeof(gm));
        status = GetGlyphOutlineW(fDDC, glyphId, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, NULL, &fMat22Identity);
        if (GDI_ERROR != status) {
            SkPoint advance;
            fHiResMatrix.mapXY(SkIntToScalar(gm.gmCellIncX), SkIntToScalar(gm.gmCellIncY), &advance);
            glyph->fAdvanceX = SkScalarToFixed(advance.fX);
            glyph->fAdvanceY = SkScalarToFixed(advance.fY);
        }
        SelectObject(fDDC, fFont);
    }
}

void SkScalerContext_Windows::generateFontMetrics(SkPaint::FontMetrics* mx, SkPaint::FontMetrics* my) {
// Note: This code was borrowed from generateLineHeight, which has a note
// stating that it may be incorrect.
    if (!(mx || my))
      return;

    SkASSERT(fDDC);

    if (fType == SkScalerContext_Windows::kBitmap_Type) {
        if (mx) {
            mx->fTop = SkIntToScalar(-fTM.tmAscent);
            mx->fAscent = SkIntToScalar(-fTM.tmAscent);
            mx->fDescent = -SkIntToScalar(fTM.tmDescent);
            mx->fBottom = SkIntToScalar(fTM.tmDescent);
            mx->fLeading = SkIntToScalar(fTM.tmInternalLeading
                                         + fTM.tmExternalLeading);
        }

        if (my) {
            my->fTop = SkIntToScalar(-fTM.tmAscent);
            my->fAscent = SkIntToScalar(-fTM.tmAscent);
            my->fDescent = SkIntToScalar(-fTM.tmDescent);
            my->fBottom = SkIntToScalar(fTM.tmDescent);
            my->fLeading = SkIntToScalar(fTM.tmInternalLeading
                                         + fTM.tmExternalLeading);
        }
        return;
    }

    OUTLINETEXTMETRIC otm;

    uint32_t ret = GetOutlineTextMetrics(fDDC, sizeof(otm), &otm);
    if (GDI_ERROR == ret) {
        LogFontTypeface::EnsureAccessible(this->getTypeface());
        ret = GetOutlineTextMetrics(fDDC, sizeof(otm), &otm);
    }
    if (sizeof(otm) != ret) {
        return;
    }

    if (mx) {
        mx->fTop = -fScale * otm.otmTextMetrics.tmAscent;
        mx->fAscent = -fScale * otm.otmAscent;
        mx->fDescent = -fScale * otm.otmDescent;
        mx->fBottom = fScale * otm.otmTextMetrics.tmDescent;
        mx->fLeading = fScale * (otm.otmTextMetrics.tmInternalLeading
                                 + otm.otmTextMetrics.tmExternalLeading);
    }

    if (my) {
        my->fTop = -fScale * otm.otmTextMetrics.tmAscent;
        my->fAscent = -fScale * otm.otmAscent;
        my->fDescent = -fScale * otm.otmDescent;
        my->fBottom = fScale * otm.otmTextMetrics.tmDescent;
        my->fLeading = fScale * (otm.otmTextMetrics.tmInternalLeading
                                 + otm.otmTextMetrics.tmExternalLeading);
    }
}

////////////////////////////////////////////////////////////////////////////////////////

static void build_power_table(uint8_t table[], float ee) {
    for (int i = 0; i < 256; i++) {
        float x = i / 255.f;
        x = sk_float_pow(x, ee);
        int xx = SkScalarRound(SkFloatToScalar(x * 255));
        table[i] = SkToU8(xx);
    }
}

/**
 *  This will invert the gamma applied by GDI (gray-scale antialiased), so we
 *  can get linear values.
 *
 *  GDI grayscale appears to use a hard-coded gamma of 2.3.
 *
 *  GDI grayscale appears to draw using the black and white rasterizer at four
 *  times the size and then downsamples to compute the coverage mask. As a
 *  result there are only seventeen total grays. This lack of fidelity means
 *  that shifting into other color spaces is imprecise.
 */
static const uint8_t* getInverseGammaTableGDI() {
    static bool gInited;
    static uint8_t gTableGdi[256];
    if (!gInited) {
        build_power_table(gTableGdi, 2.3f);
        gInited = true;
    }
    return gTableGdi;
}

/**
 *  This will invert the gamma applied by GDI ClearType, so we can get linear
 *  values.
 *
 *  GDI ClearType uses SPI_GETFONTSMOOTHINGCONTRAST / 1000 as the gamma value.
 *  If this value is not specified, the default is a gamma of 1.4.
 */
static const uint8_t* getInverseGammaTableClearType() {
    static bool gInited;
    static uint8_t gTableClearType[256];
    if (!gInited) {
        UINT level = 0;
        if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0, &level, 0) || !level) {
            // can't get the data, so use a default
            level = 1400;
        }
        build_power_table(gTableClearType, level / 1000.0f);
        gInited = true;
    }
    return gTableClearType;
}

#include "SkColorPriv.h"

//Cannot assume that the input rgb is gray due to possible setting of kGenA8FromLCD_Flag.
template<bool APPLY_PREBLEND>
static inline uint8_t rgb_to_a8(SkGdiRGB rgb, const uint8_t* table8) {
    U8CPU r = (rgb >> 16) & 0xFF;
    U8CPU g = (rgb >>  8) & 0xFF;
    U8CPU b = (rgb >>  0) & 0xFF;
    return sk_apply_lut_if<APPLY_PREBLEND>(SkComputeLuminance(r, g, b), table8);
}

template<bool APPLY_PREBLEND>
static inline uint16_t rgb_to_lcd16(SkGdiRGB rgb, const uint8_t* tableR,
                                                  const uint8_t* tableG,
                                                  const uint8_t* tableB) {
    U8CPU r = sk_apply_lut_if<APPLY_PREBLEND>((rgb >> 16) & 0xFF, tableR);
    U8CPU g = sk_apply_lut_if<APPLY_PREBLEND>((rgb >>  8) & 0xFF, tableG);
    U8CPU b = sk_apply_lut_if<APPLY_PREBLEND>((rgb >>  0) & 0xFF, tableB);
    return SkPack888ToRGB16(r, g, b);
}

template<bool APPLY_PREBLEND>
static inline SkPMColor rgb_to_lcd32(SkGdiRGB rgb, const uint8_t* tableR,
                                                   const uint8_t* tableG,
                                                   const uint8_t* tableB) {
    U8CPU r = sk_apply_lut_if<APPLY_PREBLEND>((rgb >> 16) & 0xFF, tableR);
    U8CPU g = sk_apply_lut_if<APPLY_PREBLEND>((rgb >>  8) & 0xFF, tableG);
    U8CPU b = sk_apply_lut_if<APPLY_PREBLEND>((rgb >>  0) & 0xFF, tableB);
    return SkPackARGB32(0xFF, r, g, b);
}

// Is this GDI color neither black nor white? If so, we have to keep this
// image as is, rather than smashing it down to a BW mask.
//
// returns int instead of bool, since we don't want/have to pay to convert
// the zero/non-zero value into a bool
static int is_not_black_or_white(SkGdiRGB c) {
    // same as (but faster than)
    //      c &= 0x00FFFFFF;
    //      return 0 == c || 0x00FFFFFF == c;
    return (c + (c & 1)) & 0x00FFFFFF;
}

static bool is_rgb_really_bw(const SkGdiRGB* src, int width, int height, int srcRB) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (is_not_black_or_white(src[x])) {
                return false;
            }
        }
        src = SkTAddByteOffset(src, srcRB);
    }
    return true;
}

// gdi's bitmap is upside-down, so we reverse dst walking in Y
// whenever we copy it into skia's buffer
static void rgb_to_bw(const SkGdiRGB* SK_RESTRICT src, size_t srcRB,
                      const SkGlyph& glyph) {
    const int width = glyph.fWidth;
    const size_t dstRB = (width + 7) >> 3;
    uint8_t* SK_RESTRICT dst = (uint8_t*)((char*)glyph.fImage + (glyph.fHeight - 1) * dstRB);

    int byteCount = width >> 3;
    int bitCount = width & 7;

    // adjust srcRB to skip the values in our byteCount loop,
    // since we increment src locally there
    srcRB -= byteCount * 8 * sizeof(SkGdiRGB);

    for (int y = 0; y < glyph.fHeight; ++y) {
        if (byteCount > 0) {
            for (int i = 0; i < byteCount; ++i) {
                unsigned byte = 0;
                byte |= src[0] & (1 << 7);
                byte |= src[1] & (1 << 6);
                byte |= src[2] & (1 << 5);
                byte |= src[3] & (1 << 4);
                byte |= src[4] & (1 << 3);
                byte |= src[5] & (1 << 2);
                byte |= src[6] & (1 << 1);
                byte |= src[7] & (1 << 0);
                dst[i] = byte;
                src += 8;
            }
        }
        if (bitCount > 0) {
            unsigned byte = 0;
            unsigned mask = 0x80;
            for (int i = 0; i < bitCount; i++) {
                byte |= src[i] & mask;
                mask >>= 1;
            }
            dst[byteCount] = byte;
        }
        src = SkTAddByteOffset(src, srcRB);
        dst -= dstRB;
    }
}

template<bool APPLY_PREBLEND>
static void rgb_to_a8(const SkGdiRGB* SK_RESTRICT src, size_t srcRB,
                      const SkGlyph& glyph, const uint8_t* table8) {
    const size_t dstRB = glyph.rowBytes();
    const int width = glyph.fWidth;
    uint8_t* SK_RESTRICT dst = (uint8_t*)((char*)glyph.fImage + (glyph.fHeight - 1) * dstRB);

    for (int y = 0; y < glyph.fHeight; y++) {
        for (int i = 0; i < width; i++) {
            dst[i] = rgb_to_a8<APPLY_PREBLEND>(src[i], table8);
        }
        src = SkTAddByteOffset(src, srcRB);
        dst -= dstRB;
    }
}

template<bool APPLY_PREBLEND>
static void rgb_to_lcd16(const SkGdiRGB* SK_RESTRICT src, size_t srcRB, const SkGlyph& glyph,
                         const uint8_t* tableR, const uint8_t* tableG, const uint8_t* tableB) {
    const size_t dstRB = glyph.rowBytes();
    const int width = glyph.fWidth;
    uint16_t* SK_RESTRICT dst = (uint16_t*)((char*)glyph.fImage + (glyph.fHeight - 1) * dstRB);

    for (int y = 0; y < glyph.fHeight; y++) {
        for (int i = 0; i < width; i++) {
            dst[i] = rgb_to_lcd16<APPLY_PREBLEND>(src[i], tableR, tableG, tableB);
        }
        src = SkTAddByteOffset(src, srcRB);
        dst = (uint16_t*)((char*)dst - dstRB);
    }
}

template<bool APPLY_PREBLEND>
static void rgb_to_lcd32(const SkGdiRGB* SK_RESTRICT src, size_t srcRB, const SkGlyph& glyph,
                         const uint8_t* tableR, const uint8_t* tableG, const uint8_t* tableB) {
    const size_t dstRB = glyph.rowBytes();
    const int width = glyph.fWidth;
    uint32_t* SK_RESTRICT dst = (uint32_t*)((char*)glyph.fImage + (glyph.fHeight - 1) * dstRB);

    for (int y = 0; y < glyph.fHeight; y++) {
        for (int i = 0; i < width; i++) {
            dst[i] = rgb_to_lcd32<APPLY_PREBLEND>(src[i], tableR, tableG, tableB);
        }
        src = SkTAddByteOffset(src, srcRB);
        dst = (uint32_t*)((char*)dst - dstRB);
    }
}

static inline unsigned clamp255(unsigned x) {
    SkASSERT(x <= 256);
    return x - (x >> 8);
}

void SkScalerContext_Windows::generateImage(const SkGlyph& glyph) {
    SkAutoMutexAcquire ac(gFTMutex);
    SkASSERT(fDDC);

    const bool isBW = SkMask::kBW_Format == fRec.fMaskFormat;
    const bool isAA = !isLCD(fRec);

    size_t srcRB;
    const void* bits = fOffscreen.draw(glyph, isBW, &srcRB);
    if (NULL == bits) {
        LogFontTypeface::EnsureAccessible(this->getTypeface());
        bits = fOffscreen.draw(glyph, isBW, &srcRB);
        if (NULL == bits) {
            sk_bzero(glyph.fImage, glyph.computeImageSize());
            return;
        }
    }

    if (!isBW) {
        const uint8_t* table;
        //The offscreen contains a GDI blit if isAA and kGenA8FromLCD_Flag is not set.
        //Otherwise the offscreen contains a ClearType blit.
        if (isAA && !(fRec.fFlags & SkScalerContext::kGenA8FromLCD_Flag)) {
            table = getInverseGammaTableGDI();
        } else {
            table = getInverseGammaTableClearType();
        }
        //Note that the following cannot really be integrated into the
        //pre-blend, since we may not be applying the pre-blend; when we aren't
        //applying the pre-blend it means that a filter wants linear anyway.
        //Other code may also be applying the pre-blend, so we'd need another
        //one with this and one without.
        SkGdiRGB* addr = (SkGdiRGB*)bits;
        for (int y = 0; y < glyph.fHeight; ++y) {
            for (int x = 0; x < glyph.fWidth; ++x) {
                int r = (addr[x] >> 16) & 0xFF;
                int g = (addr[x] >>  8) & 0xFF;
                int b = (addr[x] >>  0) & 0xFF;
                addr[x] = (table[r] << 16) | (table[g] << 8) | table[b];
            }
            addr = SkTAddByteOffset(addr, srcRB);
        }
    }

    int width = glyph.fWidth;
    size_t dstRB = glyph.rowBytes();
    if (isBW) {
        const uint8_t* src = (const uint8_t*)bits;
        uint8_t* dst = (uint8_t*)((char*)glyph.fImage + (glyph.fHeight - 1) * dstRB);
        for (int y = 0; y < glyph.fHeight; y++) {
            memcpy(dst, src, dstRB);
            src += srcRB;
            dst -= dstRB;
        }
    } else if (isAA) {
        // since the caller may require A8 for maskfilters, we can't check for BW
        // ... until we have the caller tell us that explicitly
        const SkGdiRGB* src = (const SkGdiRGB*)bits;
        if (fPreBlend.isApplicable()) {
            rgb_to_a8<true>(src, srcRB, glyph, fPreBlend.fG);
        } else {
            rgb_to_a8<false>(src, srcRB, glyph, fPreBlend.fG);
        }
    } else {    // LCD16
        const SkGdiRGB* src = (const SkGdiRGB*)bits;
        if (is_rgb_really_bw(src, width, glyph.fHeight, srcRB)) {
            rgb_to_bw(src, srcRB, glyph);
            ((SkGlyph*)&glyph)->fMaskFormat = SkMask::kBW_Format;
        } else {
            if (SkMask::kLCD16_Format == glyph.fMaskFormat) {
                if (fPreBlend.isApplicable()) {
                    rgb_to_lcd16<true>(src, srcRB, glyph,
                                       fPreBlend.fR, fPreBlend.fG, fPreBlend.fB);
                } else {
                    rgb_to_lcd16<false>(src, srcRB, glyph,
                                        fPreBlend.fR, fPreBlend.fG, fPreBlend.fB);
                }
            } else {
                SkASSERT(SkMask::kLCD32_Format == glyph.fMaskFormat);
                if (fPreBlend.isApplicable()) {
                    rgb_to_lcd32<true>(src, srcRB, glyph,
                                       fPreBlend.fR, fPreBlend.fG, fPreBlend.fB);
                } else {
                    rgb_to_lcd32<false>(src, srcRB, glyph,
                                        fPreBlend.fR, fPreBlend.fG, fPreBlend.fB);
                }
            }
        }
    }
}

void SkScalerContext_Windows::generatePath(const SkGlyph& glyph, SkPath* path) {

    SkAutoMutexAcquire ac(gFTMutex);

    SkASSERT(&glyph && path);
    SkASSERT(fDDC);

    path->reset();

    GLYPHMETRICS gm;
    uint32_t total_size = GetGlyphOutlineW(fDDC, glyph.fID, GGO_NATIVE | GGO_GLYPH_INDEX, &gm, BUFFERSIZE, glyphbuf, &fMat22);
    if (GDI_ERROR == total_size) {
        LogFontTypeface::EnsureAccessible(this->getTypeface());
        total_size = GetGlyphOutlineW(fDDC, glyph.fID, GGO_NATIVE | GGO_GLYPH_INDEX, &gm, BUFFERSIZE, glyphbuf, &fMat22);
        if (GDI_ERROR == total_size) {
            SkASSERT(false);
            return;
        }
    }

    const uint8_t* cur_glyph = glyphbuf;
    const uint8_t* end_glyph = glyphbuf + total_size;

    while (cur_glyph < end_glyph) {
        const TTPOLYGONHEADER* th = (TTPOLYGONHEADER*)cur_glyph;

        const uint8_t* end_poly = cur_glyph + th->cb;
        const uint8_t* cur_poly = cur_glyph + sizeof(TTPOLYGONHEADER);

        path->moveTo(SkFixedToScalar( SkFIXEDToFixed(th->pfxStart.x)),
                     SkFixedToScalar(-SkFIXEDToFixed(th->pfxStart.y)));

        while (cur_poly < end_poly) {
            const TTPOLYCURVE* pc = (const TTPOLYCURVE*)cur_poly;

            if (pc->wType == TT_PRIM_LINE) {
                for (uint16_t i = 0; i < pc->cpfx; i++) {
                    path->lineTo(SkFixedToScalar( SkFIXEDToFixed(pc->apfx[i].x)),
                                 SkFixedToScalar(-SkFIXEDToFixed(pc->apfx[i].y)));
                }
            }

            if (pc->wType == TT_PRIM_QSPLINE) {
                for (uint16_t u = 0; u < pc->cpfx - 1; u++) { // Walk through points in spline
                    POINTFX pnt_b = pc->apfx[u];    // B is always the current point
                    POINTFX pnt_c = pc->apfx[u+1];

                    if (u < pc->cpfx - 2) {          // If not on last spline, compute C
                        pnt_c.x = SkFixedToFIXED(SkFixedAve(SkFIXEDToFixed(pnt_b.x),
                                                            SkFIXEDToFixed(pnt_c.x)));
                        pnt_c.y = SkFixedToFIXED(SkFixedAve(SkFIXEDToFixed(pnt_b.y),
                                                            SkFIXEDToFixed(pnt_c.y)));
                    }

                    path->quadTo(SkFixedToScalar( SkFIXEDToFixed(pnt_b.x)),
                                 SkFixedToScalar(-SkFIXEDToFixed(pnt_b.y)),
                                 SkFixedToScalar( SkFIXEDToFixed(pnt_c.x)),
                                 SkFixedToScalar(-SkFIXEDToFixed(pnt_c.y)));
                }
            }
            // Advance past this TTPOLYCURVE.
            cur_poly += sizeof(WORD) * 2 + sizeof(POINTFX) * pc->cpfx;
        }
        cur_glyph += th->cb;
        path->close();
    }
}

static void logfont_for_name(const char* familyName, LOGFONT* lf) {
    sk_bzero(lf, sizeof(LOGFONT));
#ifdef UNICODE
    // Get the buffer size needed first.
    size_t str_len = ::MultiByteToWideChar(CP_UTF8, 0, familyName,
                                            -1, NULL, 0);
    // Allocate a buffer (str_len already has terminating null
    // accounted for).
    wchar_t *wideFamilyName = new wchar_t[str_len];
    // Now actually convert the string.
    ::MultiByteToWideChar(CP_UTF8, 0, familyName, -1,
                            wideFamilyName, str_len);
    ::wcsncpy(lf->lfFaceName, wideFamilyName, LF_FACESIZE - 1);
    delete [] wideFamilyName;
    lf->lfFaceName[LF_FACESIZE-1] = L'\0';
#else
    ::strncpy(lf->lfFaceName, familyName, LF_FACESIZE - 1);
    lf->lfFaceName[LF_FACESIZE - 1] = '\0';
#endif
}

void LogFontTypeface::onGetFontDescriptor(SkFontDescriptor* desc,
                                          bool* isLocalStream) const {
    // Get the actual name of the typeface. The logfont may not know this.
    HFONT font = CreateFontIndirect(&fLogFont);

    HDC deviceContext = ::CreateCompatibleDC(NULL);
    HFONT savefont = (HFONT)SelectObject(deviceContext, font);

    int fontNameLen; //length of fontName in TCHARS.
    if (0 == (fontNameLen = GetTextFace(deviceContext, 0, NULL))) {
        call_ensure_accessible(fLogFont);
        if (0 == (fontNameLen = GetTextFace(deviceContext, 0, NULL))) {
            fontNameLen = 0;
        }
    }

    SkAutoSTArray<LF_FULLFACESIZE, TCHAR> fontName(fontNameLen+1);
    if (0 == GetTextFace(deviceContext, fontNameLen, fontName.get())) {
        call_ensure_accessible(fLogFont);
        if (0 == GetTextFace(deviceContext, fontNameLen, fontName.get())) {
            fontName[0] = 0;
        }
    }

    if (deviceContext) {
        ::SelectObject(deviceContext, savefont);
        ::DeleteDC(deviceContext);
    }
    if (font) {
        ::DeleteObject(font);
    }

    SkString familyName;
    tchar_to_skstring(fontName.get(), &familyName);

    desc->setFamilyName(familyName.c_str());
    *isLocalStream = this->fSerializeAsStream;
}

static bool getWidthAdvance(HDC hdc, int gId, int16_t* advance) {
    // Initialize the MAT2 structure to the identify transformation matrix.
    static const MAT2 mat2 = {SkScalarToFIXED(1), SkScalarToFIXED(0),
                        SkScalarToFIXED(0), SkScalarToFIXED(1)};
    int flags = GGO_METRICS | GGO_GLYPH_INDEX;
    GLYPHMETRICS gm;
    if (GDI_ERROR == GetGlyphOutline(hdc, gId, flags, &gm, 0, NULL, &mat2)) {
        return false;
    }
    SkASSERT(advance);
    *advance = gm.gmCellIncX;
    return true;
}

SkAdvancedTypefaceMetrics* LogFontTypeface::onGetAdvancedTypefaceMetrics(
        SkAdvancedTypefaceMetrics::PerGlyphInfo perGlyphInfo,
        const uint32_t* glyphIDs,
        uint32_t glyphIDsCount) const {
    LOGFONT lf = fLogFont;
    SkAdvancedTypefaceMetrics* info = NULL;

    HDC hdc = CreateCompatibleDC(NULL);
    HFONT font = CreateFontIndirect(&lf);
    HFONT savefont = (HFONT)SelectObject(hdc, font);
    HFONT designFont = NULL;

    const char stem_chars[] = {'i', 'I', '!', '1'};
    int16_t min_width;
    unsigned glyphCount;

    // To request design units, create a logical font whose height is specified
    // as unitsPerEm.
    OUTLINETEXTMETRIC otm;
    unsigned int otmRet = GetOutlineTextMetrics(hdc, sizeof(otm), &otm);
    if (0 == otmRet) {
        call_ensure_accessible(lf);
        otmRet = GetOutlineTextMetrics(hdc, sizeof(otm), &otm);
    }
    if (!otmRet || !GetTextFace(hdc, LF_FACESIZE, lf.lfFaceName)) {
        goto Error;
    }
    lf.lfHeight = -SkToS32(otm.otmEMSquare);
    designFont = CreateFontIndirect(&lf);
    SelectObject(hdc, designFont);
    if (!GetOutlineTextMetrics(hdc, sizeof(otm), &otm)) {
        goto Error;
    }
    glyphCount = calculateOutlineGlyphCount(hdc);

    info = new SkAdvancedTypefaceMetrics;
    info->fEmSize = otm.otmEMSquare;
    info->fMultiMaster = false;
    info->fLastGlyphID = SkToU16(glyphCount - 1);
    info->fStyle = 0;
    tchar_to_skstring(lf.lfFaceName, &info->fFontName);

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kToUnicode_PerGlyphInfo) {
        populate_glyph_to_unicode(hdc, glyphCount, &(info->fGlyphToUnicode));
    }

    if (glyphCount > 0 &&
        (otm.otmTextMetrics.tmPitchAndFamily & TMPF_TRUETYPE)) {
        info->fType = SkAdvancedTypefaceMetrics::kTrueType_Font;
    } else {
        info->fType = SkAdvancedTypefaceMetrics::kOther_Font;
        info->fItalicAngle = 0;
        info->fAscent = 0;
        info->fDescent = 0;
        info->fStemV = 0;
        info->fCapHeight = 0;
        info->fBBox = SkIRect::MakeEmpty();
        goto ReturnInfo;
    }

    // If this bit is clear the font is a fixed pitch font.
    if (!(otm.otmTextMetrics.tmPitchAndFamily & TMPF_FIXED_PITCH)) {
        info->fStyle |= SkAdvancedTypefaceMetrics::kFixedPitch_Style;
    }
    if (otm.otmTextMetrics.tmItalic) {
        info->fStyle |= SkAdvancedTypefaceMetrics::kItalic_Style;
    }
    if (otm.otmTextMetrics.tmPitchAndFamily & FF_ROMAN) {
        info->fStyle |= SkAdvancedTypefaceMetrics::kSerif_Style;
    } else if (otm.otmTextMetrics.tmPitchAndFamily & FF_SCRIPT) {
            info->fStyle |= SkAdvancedTypefaceMetrics::kScript_Style;
    }

    // The main italic angle of the font, in tenths of a degree counterclockwise
    // from vertical.
    info->fItalicAngle = otm.otmItalicAngle / 10;
    info->fAscent = SkToS16(otm.otmTextMetrics.tmAscent);
    info->fDescent = SkToS16(-otm.otmTextMetrics.tmDescent);
    // TODO(ctguil): Use alternate cap height calculation.
    // MSDN says otmsCapEmHeight is not support but it is returning a value on
    // my Win7 box.
    info->fCapHeight = otm.otmsCapEmHeight;
    info->fBBox =
        SkIRect::MakeLTRB(otm.otmrcFontBox.left, otm.otmrcFontBox.top,
                          otm.otmrcFontBox.right, otm.otmrcFontBox.bottom);

    // Figure out a good guess for StemV - Min width of i, I, !, 1.
    // This probably isn't very good with an italic font.
    min_width = SHRT_MAX;
    info->fStemV = 0;
    for (size_t i = 0; i < SK_ARRAY_COUNT(stem_chars); i++) {
        ABC abcWidths;
        if (GetCharABCWidths(hdc, stem_chars[i], stem_chars[i], &abcWidths)) {
            int16_t width = abcWidths.abcB;
            if (width > 0 && width < min_width) {
                min_width = width;
                info->fStemV = min_width;
            }
        }
    }

    // If bit 1 is set, the font may not be embedded in a document.
    // If bit 1 is clear, the font can be embedded.
    // If bit 2 is set, the embedding is read-only.
    if (otm.otmfsType & 0x1) {
        info->fType = SkAdvancedTypefaceMetrics::kNotEmbeddable_Font;
    } else if (perGlyphInfo &
               SkAdvancedTypefaceMetrics::kHAdvance_PerGlyphInfo) {
        if (info->fStyle & SkAdvancedTypefaceMetrics::kFixedPitch_Style) {
            appendRange(&info->fGlyphWidths, 0);
            info->fGlyphWidths->fAdvance.append(1, &min_width);
            finishRange(info->fGlyphWidths.get(), 0,
                        SkAdvancedTypefaceMetrics::WidthRange::kDefault);
        } else {
            info->fGlyphWidths.reset(
                getAdvanceData(hdc,
                               glyphCount,
                               glyphIDs,
                               glyphIDsCount,
                               &getWidthAdvance));
        }
    }

Error:
ReturnInfo:
    SelectObject(hdc, savefont);
    DeleteObject(designFont);
    DeleteObject(font);
    DeleteDC(hdc);

    return info;
}

//Dummy representation of a Base64 encoded GUID from create_unique_font_name.
#define BASE64_GUID_ID "XXXXXXXXXXXXXXXXXXXXXXXX"
//Length of GUID representation from create_id, including NULL terminator.
#define BASE64_GUID_ID_LEN SK_ARRAY_COUNT(BASE64_GUID_ID)

SK_COMPILE_ASSERT(BASE64_GUID_ID_LEN < LF_FACESIZE, GUID_longer_than_facesize);

/**
   NameID 6 Postscript names cannot have the character '/'.
   It would be easier to hex encode the GUID, but that is 32 bytes,
   and many systems have issues with names longer than 28 bytes.
   The following need not be any standard base64 encoding.
   The encoded value is never decoded.
*/
static const char postscript_safe_base64_encode[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_=";

/**
   Formats a GUID into Base64 and places it into buffer.
   buffer should have space for at least BASE64_GUID_ID_LEN characters.
   The string will always be null terminated.
   XXXXXXXXXXXXXXXXXXXXXXXX0
 */
static void format_guid_b64(const GUID& guid, char* buffer, size_t bufferSize) {
    SkASSERT(bufferSize >= BASE64_GUID_ID_LEN);
    size_t written = SkBase64::Encode(&guid, sizeof(guid), buffer, postscript_safe_base64_encode);
    SkASSERT(written < LF_FACESIZE);
    buffer[written] = '\0';
}

/**
   Creates a Base64 encoded GUID and places it into buffer.
   buffer should have space for at least BASE64_GUID_ID_LEN characters.
   The string will always be null terminated.
   XXXXXXXXXXXXXXXXXXXXXXXX0
 */
static HRESULT create_unique_font_name(char* buffer, size_t bufferSize) {
    GUID guid = {};
    if (FAILED(CoCreateGuid(&guid))) {
        return E_UNEXPECTED;
    }
    format_guid_b64(guid, buffer, bufferSize);

    return S_OK;
}

/**
   Introduces a font to GDI. On failure will return NULL. The returned handle
   should eventually be passed to RemoveFontMemResourceEx.
*/
static HANDLE activate_font(SkData* fontData) {
    DWORD numFonts = 0;
    //AddFontMemResourceEx just copies the data, but does not specify const.
    HANDLE fontHandle = AddFontMemResourceEx(const_cast<void*>(fontData->data()),
                                             fontData->size(),
                                             0,
                                             &numFonts);

    if (fontHandle != NULL && numFonts < 1) {
        RemoveFontMemResourceEx(fontHandle);
        return NULL;
    }

    return fontHandle;
}

static SkTypeface* create_from_stream(SkStream* stream) {
    // Create a unique and unpredictable font name.
    // Avoids collisions and access from CSS.
    char familyName[BASE64_GUID_ID_LEN];
    const int familyNameSize = SK_ARRAY_COUNT(familyName);
    if (FAILED(create_unique_font_name(familyName, familyNameSize))) {
        return NULL;
    }

    // Change the name of the font.
    SkAutoTUnref<SkData> rewrittenFontData(SkOTUtils::RenameFont(stream, familyName, familyNameSize-1));
    if (NULL == rewrittenFontData.get()) {
        return NULL;
    }

    // Register the font with GDI.
    HANDLE fontReference = activate_font(rewrittenFontData.get());
    if (NULL == fontReference) {
        return NULL;
    }

    // Create the typeface.
    LOGFONT lf;
    logfont_for_name(familyName, &lf);

    return SkCreateFontMemResourceTypefaceFromLOGFONT(lf, fontReference);
}

SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream* stream) {
    return create_from_stream(stream);
}

SkStream* LogFontTypeface::onOpenStream(int* ttcIndex) const {
    *ttcIndex = 0;

    const DWORD kTTCTag =
        SkEndian_SwapBE32(SkSetFourByteTag('t', 't', 'c', 'f'));
    LOGFONT lf = fLogFont;

    HDC hdc = ::CreateCompatibleDC(NULL);
    HFONT font = CreateFontIndirect(&lf);
    HFONT savefont = (HFONT)SelectObject(hdc, font);

    SkMemoryStream* stream = NULL;
    DWORD tables[2] = {kTTCTag, 0};
    for (int i = 0; i < SK_ARRAY_COUNT(tables); i++) {
        size_t bufferSize = GetFontData(hdc, tables[i], 0, NULL, 0);
        if (bufferSize == GDI_ERROR) {
            call_ensure_accessible(lf);
            bufferSize = GetFontData(hdc, tables[i], 0, NULL, 0);
        }
        if (bufferSize != GDI_ERROR) {
            stream = new SkMemoryStream(bufferSize);
            if (GetFontData(hdc, tables[i], 0, (void*)stream->getMemoryBase(),
                            bufferSize)) {
                break;
            } else {
                delete stream;
                stream = NULL;
            }
        }
    }

    SelectObject(hdc, savefont);
    DeleteObject(font);
    DeleteDC(hdc);

    return stream;
}

SkScalerContext* LogFontTypeface::onCreateScalerContext(const SkDescriptor* desc) const {
    return SkNEW_ARGS(SkScalerContext_Windows, (const_cast<LogFontTypeface*>(this), desc));
}

/** Return the closest matching typeface given either an existing family
 (specified by a typeface in that family) or by a familyName, and a
 requested style.
 1) If familyFace is null, use familyName.
 2) If familyName is null, use familyFace.
 3) If both are null, return the default font that best matches style
 This MUST not return NULL.
 */

SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                       const char familyName[],
                                       SkTypeface::Style style) {
    LOGFONT lf;
    if (NULL == familyFace && NULL == familyName) {
        lf = get_default_font();
    } else if (familyFace) {
        LogFontTypeface* face = (LogFontTypeface*)familyFace;
        lf = face->fLogFont;
    } else {
        logfont_for_name(familyName, &lf);
    }
    setStyle(&lf, style);
    return SkCreateTypefaceFromLOGFONT(lf);
}

SkTypeface* SkFontHost::CreateTypefaceFromFile(const char path[]) {
    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(path));
    return stream.get() ? CreateTypefaceFromStream(stream) : NULL;
}

void LogFontTypeface::onFilterRec(SkScalerContextRec* rec) const {
    unsigned flagsWeDontSupport = SkScalerContext::kDevKernText_Flag |
                                  SkScalerContext::kAutohinting_Flag |
                                  SkScalerContext::kEmbeddedBitmapText_Flag |
                                  SkScalerContext::kEmbolden_Flag |
                                  SkScalerContext::kSubpixelPositioning_Flag |
                                  SkScalerContext::kLCD_BGROrder_Flag |
                                  SkScalerContext::kLCD_Vertical_Flag;
    rec->fFlags &= ~flagsWeDontSupport;

    SkPaint::Hinting h = rec->getHinting();

    // I think we can support no-hinting, if we get hires outlines and just
    // use skia to rasterize into a gray-scale mask...
#if 0
    switch (h) {
        case SkPaint::kNo_Hinting:
        case SkPaint::kSlight_Hinting:
            h = SkPaint::kNo_Hinting;
            break;
        case SkPaint::kNormal_Hinting:
        case SkPaint::kFull_Hinting:
            h = SkPaint::kNormal_Hinting;
            break;
        default:
            SkDEBUGFAIL("unknown hinting");
    }
#else
    h = SkPaint::kNormal_Hinting;
#endif
    rec->setHinting(h);

// turn this off since GDI might turn A8 into BW! Need a bigger fix.
#if 0
    // Disable LCD when rotated, since GDI's output is ugly
    if (isLCD(*rec) && !isAxisAligned(*rec)) {
        rec->fMaskFormat = SkMask::kA8_Format;
    }
#endif

    if (!fCanBeLCD && isLCD(*rec)) {
        rec->fMaskFormat = SkMask::kA8_Format;
        rec->fFlags &= ~SkScalerContext::kGenA8FromLCD_Flag;
    }
}

///////////////////////////////////////////////////////////////////////////////

#include "SkFontMgr.h"
#include "SkDataTable.h"

static bool valid_logfont_for_enum(const LOGFONT& lf, DWORD fontType) {
    return TRUETYPE_FONTTYPE == fontType
        && lf.lfFaceName[0]
        && lf.lfFaceName[0] != '@'
        && OUT_STROKE_PRECIS == lf.lfOutPrecision
        // without the chraset check, we got LOTS of dups of the same font
        // is there a better check (other than searching the array for
        // the same name?
        && 0 == lf.lfCharSet
        ;
}

static int CALLBACK enum_fonts_proc(const LOGFONT* lf, const TEXTMETRIC*,
                                    DWORD fontType, LPARAM builderParam) {
    if (valid_logfont_for_enum(*lf, fontType)) {
        SkTDArray<ENUMLOGFONTEX>* array = (SkTDArray<ENUMLOGFONTEX>*)builderParam;
        *array->append() = *(ENUMLOGFONTEX*)lf;
    }
    return 1; // non-zero means continue
}

static SkFontStyle compute_fontstyle(const LOGFONT& lf) {
    return SkFontStyle(lf.lfWeight, SkFontStyle::kNormal_Width,
                       lf.lfItalic ? SkFontStyle::kItalic_Slant
                                   : SkFontStyle::kUpright_Slant);
}

class SkFontStyleSetGDI : public SkFontStyleSet {
public:
    SkFontStyleSetGDI(const TCHAR familyName[]) {
        HDC hdc = ::CreateCompatibleDC(NULL);
        ::EnumFonts(hdc, familyName, enum_fonts_proc, (LPARAM)&fArray);
        ::DeleteDC(hdc);
    }

    virtual int count() SK_OVERRIDE {
        return fArray.count();
    }

    virtual void getStyle(int index, SkFontStyle* fs, SkString* styleName) SK_OVERRIDE {
        if (fs) {
            *fs = compute_fontstyle(fArray[index].elfLogFont);
        }
        if (styleName) {
            const ENUMLOGFONTEX& ref = fArray[index];
            // For some reason, ENUMLOGFONTEX and LOGFONT disagree on their type in the
            // non-unicode version.
            //      ENUMLOGFONTEX uses BYTE
            //      LOGFONT uses CHAR
            // Here we assert they that the style name is logically the same (size) as
            // a TCHAR, so we can use the same converter function.
            SkASSERT(sizeof(TCHAR) == sizeof(ref.elfStyle[0]));
            tchar_to_skstring((const TCHAR*)ref.elfStyle, styleName);
        }
    }

    virtual SkTypeface* createTypeface(int index) SK_OVERRIDE {
        return SkCreateTypefaceFromLOGFONT(fArray[index].elfLogFont);
    }

    virtual SkTypeface* matchStyle(const SkFontStyle& pattern) SK_OVERRIDE {
        // todo:
        return SkCreateTypefaceFromLOGFONT(fArray[0].elfLogFont);
    }

private:
    SkTDArray<ENUMLOGFONTEX> fArray;
};

static int CALLBACK enum_family_proc(const LOGFONT* lf, const TEXTMETRIC* tm,
                                     DWORD fontType, LPARAM builderParam) {
    if (valid_logfont_for_enum(*lf, fontType)) {
        SkTDArray<ENUMLOGFONTEX>* array = (SkTDArray<ENUMLOGFONTEX>*)builderParam;
        *array->append() = *(ENUMLOGFONTEX*)lf;
#if 0
        SkString str;
        tchar_to_skstring(lf->lfFaceName, &str);
        SkDebugf("fam:%s height:%d width:%d esc:%d orien:%d weight:%d ital:%d char:%d clip:%d qual:%d pitch:%d\n",
                 str.c_str(), lf->lfHeight, lf->lfWidth, lf->lfEscapement, lf->lfOrientation,
                 lf->lfWeight, lf->lfItalic, lf->lfCharSet, lf->lfClipPrecision, lf->lfQuality,
                 lf->lfPitchAndFamily);
#endif
    }
    return 1; // non-zero means continue
}

class SkFontMgrGDI : public SkFontMgr {
    void init() {
        if (!fLogFontArray.isEmpty()) {
            return;
        }

        LOGFONT lf;
        sk_bzero(&lf, sizeof(lf));
        lf.lfCharSet = DEFAULT_CHARSET;

        HDC hdc = ::CreateCompatibleDC(NULL);
        ::EnumFontFamiliesEx(hdc, &lf, enum_family_proc, (LPARAM)&fLogFontArray, 0);
        ::DeleteDC(hdc);
    }

public:
    SkFontMgrGDI() {}

protected:
    virtual int onCountFamilies() SK_OVERRIDE {
        this->init();
        return fLogFontArray.count();
    }

    virtual void onGetFamilyName(int index, SkString* familyName) SK_OVERRIDE {
        this->init();
        SkASSERT((unsigned)index < (unsigned)fLogFontArray.count());
        tchar_to_skstring(fLogFontArray[index].elfLogFont.lfFaceName, familyName);
    }

    virtual SkFontStyleSet* onCreateStyleSet(int index) SK_OVERRIDE {
        this->init();
        SkASSERT((unsigned)index < (unsigned)fLogFontArray.count());
        return SkNEW_ARGS(SkFontStyleSetGDI, (fLogFontArray[index].elfLogFont.lfFaceName));
    }

    virtual SkFontStyleSet* onMatchFamily(const char familyName[]) SK_OVERRIDE {
        if (NULL == familyName) {
            familyName = "";    // do we need this check???
        }
        LOGFONT lf;
        logfont_for_name(familyName, &lf);
        return SkNEW_ARGS(SkFontStyleSetGDI, (lf.lfFaceName));
    }

    virtual SkTypeface* onMatchFamilyStyle(const char familyName[],
                                           const SkFontStyle& fontstyle) SK_OVERRIDE {
        // could be in base impl
        SkAutoTUnref<SkFontStyleSet> sset(this->matchFamily(familyName));
        return sset->matchStyle(fontstyle);
    }

    virtual SkTypeface* onMatchFaceStyle(const SkTypeface* familyMember,
                                         const SkFontStyle& fontstyle) SK_OVERRIDE {
        // could be in base impl
        SkString familyName;
        ((LogFontTypeface*)familyMember)->getFamilyName(&familyName);
        return this->matchFamilyStyle(familyName.c_str(), fontstyle);
    }

    virtual SkTypeface* onCreateFromStream(SkStream* stream, int ttcIndex) SK_OVERRIDE {
        return create_from_stream(stream);
    }

    virtual SkTypeface* onCreateFromData(SkData* data, int ttcIndex) SK_OVERRIDE {
        // could be in base impl
        SkAutoTUnref<SkStream> stream(SkNEW_ARGS(SkMemoryStream, (data)));
        return this->createFromStream(stream);
    }

    virtual SkTypeface* onCreateFromFile(const char path[], int ttcIndex) SK_OVERRIDE {
        // could be in base impl
        SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(path));
        return this->createFromStream(stream);
    }

private:
    SkTDArray<ENUMLOGFONTEX> fLogFontArray;
};

SkFontMgr* SkFontMgr::Factory() {
    return SkNEW(SkFontMgrGDI);
}
