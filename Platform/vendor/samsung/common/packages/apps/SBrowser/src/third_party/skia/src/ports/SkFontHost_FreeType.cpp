
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkDescriptor.h"
#include "SkFDot6.h"
#include "SkFloatingPoint.h"
#include "SkFontHost.h"
#include "SkFontHost_FreeType_common.h"
#include "SkGlyph.h"
#include "SkMask.h"
#include "SkMaskGamma.h"
#include "SkAdvancedTypefaceMetrics.h"
#include "SkScalerContext.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTemplates.h"
#include "SkThread.h"

#if defined(SK_CAN_USE_DLOPEN)
#include <dlfcn.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_TRUETYPE_TABLES_H
#include FT_TYPE1_TABLES_H
#include FT_BITMAP_H
// In the past, FT_GlyphSlot_Own_Bitmap was defined in this header file.
#include FT_SYNTHESIS_H
#include FT_XFREE86_H
#ifdef FT_LCD_FILTER_H
#include FT_LCD_FILTER_H
#endif

#ifdef   FT_ADVANCES_H
#include FT_ADVANCES_H
#endif

#if 0
// Also include the files by name for build tools which require this.
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftsizes.h>
#include <freetype/tttables.h>
#include <freetype/ftadvanc.h>
#include <freetype/ftlcdfil.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftsynth.h>
#endif

//#define ENABLE_GLYPH_SPEW     // for tracing calls
//#define DUMP_STRIKE_CREATION

//#define SK_GAMMA_APPLY_TO_A8

using namespace skia_advanced_typeface_metrics_utils;

static bool isLCD(const SkScalerContext::Rec& rec) {
    switch (rec.fMaskFormat) {
        case SkMask::kLCD16_Format:
        case SkMask::kLCD32_Format:
            return true;
        default:
            return false;
    }
}

//////////////////////////////////////////////////////////////////////////

struct SkFaceRec;

SK_DECLARE_STATIC_MUTEX(gFTMutex);
static int          gFTCount;
static FT_Library   gFTLibrary;
static SkFaceRec*   gFaceRecHead;
static bool         gLCDSupportValid;  // true iff |gLCDSupport| has been set.
static bool         gLCDSupport;  // true iff LCD is supported by the runtime.
static int          gLCDExtra;  // number of extra pixels for filtering.

/////////////////////////////////////////////////////////////////////////

// FT_Library_SetLcdFilterWeights was introduced in FreeType 2.4.0.
// The following platforms provide FreeType of at least 2.4.0.
// Ubuntu >= 11.04 (previous deprecated April 2013)
// Debian >= 6.0 (good)
// OpenSuse >= 11.4 (previous deprecated January 2012 / Nov 2013 for Evergreen 11.2)
// Fedora >= 14 (good)
// Android >= Gingerbread (good)
typedef FT_Error (*FT_Library_SetLcdFilterWeightsProc)(FT_Library, unsigned char*);

// Caller must lock gFTMutex before calling this function.
static bool InitFreetype() {
    FT_Error err = FT_Init_FreeType(&gFTLibrary);
    if (err) {
        return false;
    }

    // Setup LCD filtering. This reduces color fringes for LCD smoothed glyphs.
#ifdef FT_LCD_FILTER_H
    // Use default { 0x10, 0x40, 0x70, 0x40, 0x10 }, as it adds up to 0x110, simulating ink spread.
    // SetLcdFilter must be called before SetLcdFilterWeights.
    err = FT_Library_SetLcdFilter(gFTLibrary, FT_LCD_FILTER_DEFAULT);
    if (0 == err) {
        gLCDSupport = true;
        gLCDExtra = 2; //Using a filter adds one full pixel to each side.

#ifdef SK_FONTHOST_FREETYPE_USE_NORMAL_LCD_FILTER
        // This also adds to 0x110 simulating ink spread, but provides better results than default.
        static unsigned char gGaussianLikeHeavyWeights[] = { 0x1A, 0x43, 0x56, 0x43, 0x1A, };

#if defined(SK_FONTHOST_FREETYPE_RUNTIME_VERSION) && \
            SK_FONTHOST_FREETYPE_RUNTIME_VERSION > 0x020400
        err = FT_Library_SetLcdFilterWeights(gFTLibrary, gGaussianLikeHeavyWeights);
#elif defined(SK_CAN_USE_DLOPEN) && SK_CAN_USE_DLOPEN == 1
        //The FreeType library is already loaded, so symbols are available in process.
        void* self = dlopen(NULL, RTLD_LAZY);
        if (NULL != self) {
            FT_Library_SetLcdFilterWeightsProc setLcdFilterWeights;
            //The following cast is non-standard, but safe for POSIX.
            *reinterpret_cast<void**>(&setLcdFilterWeights) = dlsym(self, "FT_Library_SetLcdFilterWeights");
            dlclose(self);

            if (NULL != setLcdFilterWeights) {
                err = setLcdFilterWeights(gFTLibrary, gGaussianLikeHeavyWeights);
            }
        }
#endif
#endif
    }
#else
    gLCDSupport = false;
#endif
    gLCDSupportValid = true;

    return true;
}

// Lazy, once, wrapper to ask the FreeType Library if it can support LCD text
static bool is_lcd_supported() {
    if (!gLCDSupportValid) {
        SkAutoMutexAcquire  ac(gFTMutex);

        if (!gLCDSupportValid) {
            InitFreetype();
            FT_Done_FreeType(gFTLibrary);
        }
    }
    return gLCDSupport;
}

class SkScalerContext_FreeType : public SkScalerContext_FreeType_Base {
public:
    SkScalerContext_FreeType(SkTypeface*, const SkDescriptor* desc);
    virtual ~SkScalerContext_FreeType();

    bool success() const {
        return fFaceRec != NULL &&
               fFTSize != NULL &&
               fFace != NULL;
    }

protected:
    virtual unsigned generateGlyphCount() SK_OVERRIDE;
    virtual uint16_t generateCharToGlyph(SkUnichar uni) SK_OVERRIDE;
    virtual void generateAdvance(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateMetrics(SkGlyph* glyph) SK_OVERRIDE;
    virtual void generateImage(const SkGlyph& glyph) SK_OVERRIDE;
    virtual void generatePath(const SkGlyph& glyph, SkPath* path) SK_OVERRIDE;
    virtual void generateFontMetrics(SkPaint::FontMetrics* mx,
                                     SkPaint::FontMetrics* my) SK_OVERRIDE;
    virtual SkUnichar generateGlyphToChar(uint16_t glyph) SK_OVERRIDE;

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
    virtual bool isColoredBitmap();
#endif

private:
    SkFaceRec*  fFaceRec;
    FT_Face     fFace;              // reference to shared face in gFaceRecHead
    FT_Size     fFTSize;            // our own copy
    SkFixed     fScaleX, fScaleY;
    FT_Matrix   fMatrix22;
    uint32_t    fLoadGlyphFlags;
    bool        fDoLinearMetrics;
    bool        fLCDIsVert;

    // Need scalar versions for generateFontMetrics
    SkVector    fScale;
    SkMatrix    fMatrix22Scalar;

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
    FT_Bitmap_Size  ftBitmap;
    FT_Int      fStrikeIndex;
#endif

    FT_Error setupSize();
    void getBBoxForCurrentGlyph(SkGlyph* glyph, FT_BBox* bbox,
                                bool snapToPixelBoundary = false);
    // Caller must lock gFTMutex before calling this function.
    void updateGlyphIfLCD(SkGlyph* glyph);
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

#include "SkStream.h"

struct SkFaceRec {
    SkFaceRec*      fNext;
    FT_Face         fFace;
    FT_StreamRec    fFTStream;
    SkStream*       fSkStream;
    uint32_t        fRefCnt;
    uint32_t        fFontID;

    // assumes ownership of the stream, will call unref() when its done
    SkFaceRec(SkStream* strm, uint32_t fontID);
    ~SkFaceRec() {
        fSkStream->unref();
    }
};

extern "C" {
    static unsigned long sk_stream_read(FT_Stream       stream,
                                        unsigned long   offset,
                                        unsigned char*  buffer,
                                        unsigned long   count ) {
        SkStream* str = (SkStream*)stream->descriptor.pointer;

        if (count) {
            if (!str->rewind()) {
                return 0;
            } else {
                unsigned long ret;
                if (offset) {
                    ret = str->read(NULL, offset);
                    if (ret != offset) {
                        return 0;
                    }
                }
                ret = str->read(buffer, count);
                if (ret != count) {
                    return 0;
                }
                count = ret;
            }
        }
        return count;
    }

    static void sk_stream_close(FT_Stream) {}
}

SkFaceRec::SkFaceRec(SkStream* strm, uint32_t fontID)
        : fNext(NULL), fSkStream(strm), fRefCnt(1), fFontID(fontID) {
//    SkDEBUGF(("SkFaceRec: opening %s (%p)\n", key.c_str(), strm));

    sk_bzero(&fFTStream, sizeof(fFTStream));
    fFTStream.size = fSkStream->getLength();
    fFTStream.descriptor.pointer = fSkStream;
    fFTStream.read  = sk_stream_read;
    fFTStream.close = sk_stream_close;
}

// Will return 0 on failure
// Caller must lock gFTMutex before calling this function.
static SkFaceRec* ref_ft_face(const SkTypeface* typeface) {
    const SkFontID fontID = typeface->uniqueID();
    SkFaceRec* rec = gFaceRecHead;
    while (rec) {
        if (rec->fFontID == fontID) {
            SkASSERT(rec->fFace);
            rec->fRefCnt += 1;
            return rec;
        }
        rec = rec->fNext;
    }

    int face_index;
    SkStream* strm = typeface->openStream(&face_index);
    if (NULL == strm) {
        return NULL;
    }

    // this passes ownership of strm to the rec
    rec = SkNEW_ARGS(SkFaceRec, (strm, fontID));

    FT_Open_Args    args;
    memset(&args, 0, sizeof(args));
    const void* memoryBase = strm->getMemoryBase();

    if (NULL != memoryBase) {
//printf("mmap(%s)\n", keyString.c_str());
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)memoryBase;
        args.memory_size = strm->getLength();
    } else {
//printf("fopen(%s)\n", keyString.c_str());
        args.flags = FT_OPEN_STREAM;
        args.stream = &rec->fFTStream;
    }

    FT_Error err = FT_Open_Face(gFTLibrary, &args, face_index, &rec->fFace);
    if (err) {    // bad filename, try the default font
        fprintf(stderr, "ERROR: unable to open font '%x'\n", fontID);
        SkDELETE(rec);
        return NULL;
    } else {
        SkASSERT(rec->fFace);
        //fprintf(stderr, "Opened font '%s'\n", filename.c_str());
        rec->fNext = gFaceRecHead;
        gFaceRecHead = rec;
        return rec;
    }
}

// Caller must lock gFTMutex before calling this function.
static void unref_ft_face(FT_Face face) {
    SkFaceRec*  rec = gFaceRecHead;
    SkFaceRec*  prev = NULL;
    while (rec) {
        SkFaceRec* next = rec->fNext;
        if (rec->fFace == face) {
            if (--rec->fRefCnt == 0) {
                if (prev) {
                    prev->fNext = next;
                } else {
                    gFaceRecHead = next;
                }
                FT_Done_Face(face);
                SkDELETE(rec);
            }
            return;
        }
        prev = rec;
        rec = next;
    }
    SkDEBUGFAIL("shouldn't get here, face not in list");
}

///////////////////////////////////////////////////////////////////////////

// Work around for old versions of freetype.
static FT_Error getAdvances(FT_Face face, FT_UInt start, FT_UInt count,
                           FT_Int32 loadFlags, FT_Fixed* advances) {
#ifdef FT_ADVANCES_H
    return FT_Get_Advances(face, start, count, loadFlags, advances);
#else
    if (!face || start >= face->num_glyphs ||
            start + count > face->num_glyphs || loadFlags != FT_LOAD_NO_SCALE) {
        return 6;  // "Invalid argument."
    }
    if (count == 0)
        return 0;

    for (int i = 0; i < count; i++) {
        FT_Error err = FT_Load_Glyph(face, start + i, FT_LOAD_NO_SCALE);
        if (err)
            return err;
        advances[i] = face->glyph->advance.x;
    }

    return 0;
#endif
}

static bool canEmbed(FT_Face face) {
#ifdef FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING
    FT_UShort fsType = FT_Get_FSType_Flags(face);
    return (fsType & (FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING |
                      FT_FSTYPE_BITMAP_EMBEDDING_ONLY)) == 0;
#else
    // No embedding is 0x2 and bitmap embedding only is 0x200.
    TT_OS2* os2_table;
    if ((os2_table = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2)) != NULL) {
        return (os2_table->fsType & 0x202) == 0;
    }
    return false;  // We tried, fail safe.
#endif
}

static bool GetLetterCBox(FT_Face face, char letter, FT_BBox* bbox) {
    const FT_UInt glyph_id = FT_Get_Char_Index(face, letter);
    if (!glyph_id)
        return false;
    FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE);
    FT_Outline_Get_CBox(&face->glyph->outline, bbox);
    return true;
}

static bool getWidthAdvance(FT_Face face, int gId, int16_t* data) {
    FT_Fixed advance = 0;
    if (getAdvances(face, gId, 1, FT_LOAD_NO_SCALE, &advance)) {
        return false;
    }
    SkASSERT(data);
    *data = advance;
    return true;
}

static void populate_glyph_to_unicode(FT_Face& face,
                                      SkTDArray<SkUnichar>* glyphToUnicode) {
    // Check and see if we have Unicode cmaps.
    for (int i = 0; i < face->num_charmaps; ++i) {
        // CMaps known to support Unicode:
        // Platform ID   Encoding ID   Name
        // -----------   -----------   -----------------------------------
        // 0             0,1           Apple Unicode
        // 0             3             Apple Unicode 2.0 (preferred)
        // 3             1             Microsoft Unicode UCS-2
        // 3             10            Microsoft Unicode UCS-4 (preferred)
        //
        // See Apple TrueType Reference Manual
        // http://developer.apple.com/fonts/TTRefMan/RM06/Chap6cmap.html
        // http://developer.apple.com/fonts/TTRefMan/RM06/Chap6name.html#ID
        // Microsoft OpenType Specification
        // http://www.microsoft.com/typography/otspec/cmap.htm

        FT_UShort platformId = face->charmaps[i]->platform_id;
        FT_UShort encodingId = face->charmaps[i]->encoding_id;

        if (platformId != 0 && platformId != 3) {
            continue;
        }
        if (platformId == 3 && encodingId != 1 && encodingId != 10) {
            continue;
        }
        bool preferredMap = ((platformId == 3 && encodingId == 10) ||
                             (platformId == 0 && encodingId == 3));

        FT_Set_Charmap(face, face->charmaps[i]);
        if (glyphToUnicode->isEmpty()) {
            glyphToUnicode->setCount(face->num_glyphs);
            memset(glyphToUnicode->begin(), 0,
                   sizeof(SkUnichar) * face->num_glyphs);
        }

        // Iterate through each cmap entry.
        FT_UInt glyphIndex;
        for (SkUnichar charCode = FT_Get_First_Char(face, &glyphIndex);
             glyphIndex != 0;
             charCode = FT_Get_Next_Char(face, charCode, &glyphIndex)) {
            if (charCode &&
                    ((*glyphToUnicode)[glyphIndex] == 0 || preferredMap)) {
                (*glyphToUnicode)[glyphIndex] = charCode;
            }
        }
    }
}

SkAdvancedTypefaceMetrics* SkTypeface_FreeType::onGetAdvancedTypefaceMetrics(
        SkAdvancedTypefaceMetrics::PerGlyphInfo perGlyphInfo,
        const uint32_t* glyphIDs,
        uint32_t glyphIDsCount) const {
#if defined(SK_BUILD_FOR_MAC)
    return NULL;
#else
    SkAutoMutexAcquire ac(gFTMutex);
    FT_Library libInit = NULL;
    if (gFTCount == 0) {
        if (!InitFreetype())
            sk_throw();
        libInit = gFTLibrary;
    }
    SkAutoTCallIProc<struct FT_LibraryRec_, FT_Done_FreeType> ftLib(libInit);
    SkFaceRec* rec = ref_ft_face(this);
    if (NULL == rec)
        return NULL;
    FT_Face face = rec->fFace;

    SkAdvancedTypefaceMetrics* info = new SkAdvancedTypefaceMetrics;
    info->fFontName.set(FT_Get_Postscript_Name(face));
    info->fMultiMaster = FT_HAS_MULTIPLE_MASTERS(face);
    info->fLastGlyphID = face->num_glyphs - 1;
    info->fEmSize = 1000;

    bool cid = false;
    const char* fontType = FT_Get_X11_Font_Format(face);
    if (strcmp(fontType, "Type 1") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kType1_Font;
    } else if (strcmp(fontType, "CID Type 1") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kType1CID_Font;
        cid = true;
    } else if (strcmp(fontType, "CFF") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kCFF_Font;
    } else if (strcmp(fontType, "TrueType") == 0) {
        info->fType = SkAdvancedTypefaceMetrics::kTrueType_Font;
        cid = true;
        TT_Header* ttHeader;
        if ((ttHeader = (TT_Header*)FT_Get_Sfnt_Table(face,
                                                      ft_sfnt_head)) != NULL) {
            info->fEmSize = ttHeader->Units_Per_EM;
        }
    }

    info->fStyle = 0;
    if (FT_IS_FIXED_WIDTH(face))
        info->fStyle |= SkAdvancedTypefaceMetrics::kFixedPitch_Style;
    if (face->style_flags & FT_STYLE_FLAG_ITALIC)
        info->fStyle |= SkAdvancedTypefaceMetrics::kItalic_Style;

    PS_FontInfoRec ps_info;
    TT_Postscript* tt_info;
    if (FT_Get_PS_Font_Info(face, &ps_info) == 0) {
        info->fItalicAngle = ps_info.italic_angle;
    } else if ((tt_info =
                (TT_Postscript*)FT_Get_Sfnt_Table(face,
                                                  ft_sfnt_post)) != NULL) {
        info->fItalicAngle = SkFixedToScalar(tt_info->italicAngle);
    } else {
        info->fItalicAngle = 0;
    }

    info->fAscent = face->ascender;
    info->fDescent = face->descender;

    // Figure out a good guess for StemV - Min width of i, I, !, 1.
    // This probably isn't very good with an italic font.
    int16_t min_width = SHRT_MAX;
    info->fStemV = 0;
    char stem_chars[] = {'i', 'I', '!', '1'};
    for (size_t i = 0; i < SK_ARRAY_COUNT(stem_chars); i++) {
        FT_BBox bbox;
        if (GetLetterCBox(face, stem_chars[i], &bbox)) {
            int16_t width = bbox.xMax - bbox.xMin;
            if (width > 0 && width < min_width) {
                min_width = width;
                info->fStemV = min_width;
            }
        }
    }

    TT_PCLT* pclt_info;
    TT_OS2* os2_table;
    if ((pclt_info = (TT_PCLT*)FT_Get_Sfnt_Table(face, ft_sfnt_pclt)) != NULL) {
        info->fCapHeight = pclt_info->CapHeight;
        uint8_t serif_style = pclt_info->SerifStyle & 0x3F;
        if (serif_style >= 2 && serif_style <= 6)
            info->fStyle |= SkAdvancedTypefaceMetrics::kSerif_Style;
        else if (serif_style >= 9 && serif_style <= 12)
            info->fStyle |= SkAdvancedTypefaceMetrics::kScript_Style;
    } else if ((os2_table =
                (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2)) != NULL) {
        info->fCapHeight = os2_table->sCapHeight;
    } else {
        // Figure out a good guess for CapHeight: average the height of M and X.
        FT_BBox m_bbox, x_bbox;
        bool got_m, got_x;
        got_m = GetLetterCBox(face, 'M', &m_bbox);
        got_x = GetLetterCBox(face, 'X', &x_bbox);
        if (got_m && got_x) {
            info->fCapHeight = (m_bbox.yMax - m_bbox.yMin + x_bbox.yMax -
                    x_bbox.yMin) / 2;
        } else if (got_m && !got_x) {
            info->fCapHeight = m_bbox.yMax - m_bbox.yMin;
        } else if (!got_m && got_x) {
            info->fCapHeight = x_bbox.yMax - x_bbox.yMin;
        }
    }

    info->fBBox = SkIRect::MakeLTRB(face->bbox.xMin, face->bbox.yMax,
                                    face->bbox.xMax, face->bbox.yMin);

    if (!canEmbed(face) || !FT_IS_SCALABLE(face) ||
            info->fType == SkAdvancedTypefaceMetrics::kOther_Font) {
        perGlyphInfo = SkAdvancedTypefaceMetrics::kNo_PerGlyphInfo;
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kHAdvance_PerGlyphInfo) {
        if (FT_IS_FIXED_WIDTH(face)) {
            appendRange(&info->fGlyphWidths, 0);
            int16_t advance = face->max_advance_width;
            info->fGlyphWidths->fAdvance.append(1, &advance);
            finishRange(info->fGlyphWidths.get(), 0,
                        SkAdvancedTypefaceMetrics::WidthRange::kDefault);
        } else if (!cid) {
            appendRange(&info->fGlyphWidths, 0);
            // So as to not blow out the stack, get advances in batches.
            for (int gID = 0; gID < face->num_glyphs; gID += 128) {
                FT_Fixed advances[128];
                int advanceCount = 128;
                if (gID + advanceCount > face->num_glyphs)
                    advanceCount = face->num_glyphs - gID + 1;
                getAdvances(face, gID, advanceCount, FT_LOAD_NO_SCALE,
                            advances);
                for (int i = 0; i < advanceCount; i++) {
                    int16_t advance = advances[i];
                    info->fGlyphWidths->fAdvance.append(1, &advance);
                }
            }
            finishRange(info->fGlyphWidths.get(), face->num_glyphs - 1,
                        SkAdvancedTypefaceMetrics::WidthRange::kRange);
        } else {
            info->fGlyphWidths.reset(
                getAdvanceData(face,
                               face->num_glyphs,
                               glyphIDs,
                               glyphIDsCount,
                               &getWidthAdvance));
        }
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kVAdvance_PerGlyphInfo &&
            FT_HAS_VERTICAL(face)) {
        SkASSERT(false);  // Not implemented yet.
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kGlyphNames_PerGlyphInfo &&
            info->fType == SkAdvancedTypefaceMetrics::kType1_Font) {
        // Postscript fonts may contain more than 255 glyphs, so we end up
        // using multiple font descriptions with a glyph ordering.  Record
        // the name of each glyph.
        info->fGlyphNames.reset(
                new SkAutoTArray<SkString>(face->num_glyphs));
        for (int gID = 0; gID < face->num_glyphs; gID++) {
            char glyphName[128];  // PS limit for names is 127 bytes.
            FT_Get_Glyph_Name(face, gID, glyphName, 128);
            info->fGlyphNames->get()[gID].set(glyphName);
        }
    }

    if (perGlyphInfo & SkAdvancedTypefaceMetrics::kToUnicode_PerGlyphInfo &&
           info->fType != SkAdvancedTypefaceMetrics::kType1_Font &&
           face->num_charmaps) {
        populate_glyph_to_unicode(face, &(info->fGlyphToUnicode));
    }

    if (!canEmbed(face))
        info->fType = SkAdvancedTypefaceMetrics::kNotEmbeddable_Font;

    unref_ft_face(face);
    return info;
#endif
}

///////////////////////////////////////////////////////////////////////////

#define BLACK_LUMINANCE_LIMIT   0x40
#define WHITE_LUMINANCE_LIMIT   0xA0

static bool bothZero(SkScalar a, SkScalar b) {
    return 0 == a && 0 == b;
}

// returns false if there is any non-90-rotation or skew
static bool isAxisAligned(const SkScalerContext::Rec& rec) {
    return 0 == rec.fPreSkewX &&
           (bothZero(rec.fPost2x2[0][1], rec.fPost2x2[1][0]) ||
            bothZero(rec.fPost2x2[0][0], rec.fPost2x2[1][1]));
}

SkScalerContext* SkTypeface_FreeType::onCreateScalerContext(
                                               const SkDescriptor* desc) const {
    SkScalerContext_FreeType* c = SkNEW_ARGS(SkScalerContext_FreeType,
                                        (const_cast<SkTypeface_FreeType*>(this),
                                         desc));
    if (!c->success()) {
        SkDELETE(c);
        c = NULL;
    }
    return c;
}

void SkTypeface_FreeType::onFilterRec(SkScalerContextRec* rec) const {
    //BOGUS: http://code.google.com/p/chromium/issues/detail?id=121119
    //Cap the requested size as larger sizes give bogus values.
    //Remove when http://code.google.com/p/skia/issues/detail?id=554 is fixed.
    if (rec->fTextSize > SkIntToScalar(1 << 14)) {
        rec->fTextSize = SkIntToScalar(1 << 14);
    }

    if (!is_lcd_supported() && isLCD(*rec)) {
        // If the runtime Freetype library doesn't support LCD mode, we disable
        // it here.
        rec->fMaskFormat = SkMask::kA8_Format;
    }

    SkPaint::Hinting h = rec->getHinting();
    if (SkPaint::kFull_Hinting == h && !isLCD(*rec)) {
        // collapse full->normal hinting if we're not doing LCD
        h = SkPaint::kNormal_Hinting;
    }
    if ((rec->fFlags & SkScalerContext::kSubpixelPositioning_Flag)) {
        if (SkPaint::kNo_Hinting != h) {
            h = SkPaint::kSlight_Hinting;
        }
    }

    // rotated text looks bad with hinting, so we disable it as needed
    if (!isAxisAligned(*rec)) {
        h = SkPaint::kNo_Hinting;
    }
    rec->setHinting(h);

#ifndef SK_GAMMA_APPLY_TO_A8
    if (!isLCD(*rec)) {
      rec->ignorePreBlend();
    }
#endif
}

int SkTypeface_FreeType::onGetUPEM() const {
    SkAutoMutexAcquire ac(gFTMutex);
    FT_Library libInit = NULL;
    if (gFTCount == 0) {
        if (!InitFreetype())
            sk_throw();
        libInit = gFTLibrary;
    }
    SkAutoTCallIProc<struct FT_LibraryRec_, FT_Done_FreeType> ftLib(libInit);
    SkFaceRec *rec = ref_ft_face(this);
    int unitsPerEm = 0;

    if (rec != NULL && rec->fFace != NULL) {
        unitsPerEm = rec->fFace->units_per_EM;
        unref_ft_face(rec->fFace);
    }

    return unitsPerEm;
}

static int getAbsoluteDiff(int x, int y)
{
    if(x > y)
        return (x - y);

    return (y - x);
}

SkScalerContext_FreeType::SkScalerContext_FreeType(SkTypeface* typeface,
                                                   const SkDescriptor* desc)
        : SkScalerContext_FreeType_Base(typeface, desc) {
    SkAutoMutexAcquire  ac(gFTMutex);

    if (gFTCount == 0) {
        if (!InitFreetype()) {
            sk_throw();
        }
    }
    ++gFTCount;

    // load the font file
    fFTSize = NULL;
    fFace = NULL;
#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
    fStrikeIndex = -1;
#endif
    fFaceRec = ref_ft_face(typeface);
    if (NULL == fFaceRec) {
        return;
    }
    fFace = fFaceRec->fFace;

    // compute our factors from the record

    SkMatrix    m;

    fRec.getSingleMatrix(&m);

#ifdef DUMP_STRIKE_CREATION
    SkString     keyString;
    SkFontHost::GetDescriptorKeyString(desc, &keyString);
    printf("========== strike [%g %g %g] [%g %g %g %g] hints %d format %d %s\n", SkScalarToFloat(fRec.fTextSize),
           SkScalarToFloat(fRec.fPreScaleX), SkScalarToFloat(fRec.fPreSkewX),
           SkScalarToFloat(fRec.fPost2x2[0][0]), SkScalarToFloat(fRec.fPost2x2[0][1]),
           SkScalarToFloat(fRec.fPost2x2[1][0]), SkScalarToFloat(fRec.fPost2x2[1][1]),
           fRec.getHinting(), fRec.fMaskFormat, keyString.c_str());
#endif

    //  now compute our scale factors
    SkScalar    sx = m.getScaleX();
    SkScalar    sy = m.getScaleY();

    fMatrix22Scalar.reset();

    if (m.getSkewX() || m.getSkewY() || sx < 0 || sy < 0) {
        // sort of give up on hinting
        sx = SkMaxScalar(SkScalarAbs(sx), SkScalarAbs(m.getSkewX()));
        sy = SkMaxScalar(SkScalarAbs(m.getSkewY()), SkScalarAbs(sy));
        sx = sy = SkScalarAve(sx, sy);

        SkScalar inv = SkScalarInvert(sx);

        // flip the skew elements to go from our Y-down system to FreeType's
        fMatrix22.xx = SkScalarToFixed(SkScalarMul(m.getScaleX(), inv));
        fMatrix22.xy = -SkScalarToFixed(SkScalarMul(m.getSkewX(), inv));
        fMatrix22.yx = -SkScalarToFixed(SkScalarMul(m.getSkewY(), inv));
        fMatrix22.yy = SkScalarToFixed(SkScalarMul(m.getScaleY(), inv));
        
        fMatrix22Scalar.setScaleX(SkScalarMul(m.getScaleX(), inv));
        fMatrix22Scalar.setSkewX(-SkScalarMul(m.getSkewX(), inv));
        fMatrix22Scalar.setSkewY(-SkScalarMul(m.getSkewY(), inv));
        fMatrix22Scalar.setScaleY(SkScalarMul(m.getScaleY(), inv));
    } else {
        fMatrix22.xx = fMatrix22.yy = SK_Fixed1;
        fMatrix22.xy = fMatrix22.yx = 0;
    }

#ifdef SK_SUPPORT_HINTING_SCALE_FACTOR
    if (fRec.getHinting() == SkPaint::kNo_Hinting) {
        fScale.set(sx, sy);
        fScaleX = SkScalarToFixed(sx);
        fScaleY = SkScalarToFixed(sy);
    } else {
        SkScalar hintingScaleFactor = fRec.fHintingScaleFactor;

        fScale.set(sx / hintingScaleFactor, sy / hintingScaleFactor);
        fScaleX = SkScalarToFixed(fScale.fX);
        fScaleY = SkScalarToFixed(fScale.fY);

        fMatrix22.xx *= hintingScaleFactor;
        fMatrix22.xy *= hintingScaleFactor;
        fMatrix22.yx *= hintingScaleFactor;
        fMatrix22.yy *= hintingScaleFactor;

        fMatrix22Scalar.setScaleX(fMatrix22Scalar.getScaleX() * hintingScaleFactor);
        fMatrix22Scalar.setSkewX(fMatrix22Scalar..getSkewX() * hintingScaleFactor);
        fMatrix22Scalar.setSkewY(fMatrix22Scalar..getSkewY() * hintingScaleFactor);
        fMatrix22Scalar.setScaleY(fMatrix22Scalar..getScaleY() * hintingScaleFactor);
    }
#else
    fScale.set(sx, sy);
    fScaleX = SkScalarToFixed(sx);
    fScaleY = SkScalarToFixed(sy);
#endif

    fLCDIsVert = SkToBool(fRec.fFlags & SkScalerContext::kLCD_Vertical_Flag);

    // compute the flags we send to Load_Glyph
    {
        FT_Int32 loadFlags = FT_LOAD_DEFAULT;
        bool linearMetrics = SkToBool(fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag);

        if (SkMask::kBW_Format == fRec.fMaskFormat) {
            // See http://code.google.com/p/chromium/issues/detail?id=43252#c24
            loadFlags = FT_LOAD_TARGET_MONO;
            if (fRec.getHinting() == SkPaint::kNo_Hinting) {
                loadFlags = FT_LOAD_NO_HINTING;
                linearMetrics = true;
            }
        } else {
            switch (fRec.getHinting()) {
            case SkPaint::kNo_Hinting:
                loadFlags = FT_LOAD_NO_HINTING;
                linearMetrics = true;
                break;
            case SkPaint::kSlight_Hinting:
                loadFlags = FT_LOAD_TARGET_LIGHT;  // This implies FORCE_AUTOHINT
                break;
            case SkPaint::kNormal_Hinting:
                if (fRec.fFlags & SkScalerContext::kAutohinting_Flag)
                    loadFlags = FT_LOAD_FORCE_AUTOHINT;
                else
                    loadFlags = FT_LOAD_NO_AUTOHINT;
                break;
            case SkPaint::kFull_Hinting:
                if (fRec.fFlags & SkScalerContext::kAutohinting_Flag) {
                    loadFlags = FT_LOAD_FORCE_AUTOHINT;
                    break;
                }
                loadFlags = FT_LOAD_TARGET_NORMAL;
                if (isLCD(fRec)) {
                    if (fLCDIsVert) {
                        loadFlags = FT_LOAD_TARGET_LCD_V;
                    } else {
                        loadFlags = FT_LOAD_TARGET_LCD;
                    }
                }
                break;
            default:
                SkDebugf("---------- UNKNOWN hinting %d\n", fRec.getHinting());
                break;
            }
        }

        if ((fRec.fFlags & SkScalerContext::kEmbeddedBitmapText_Flag) == 0) {
            loadFlags |= FT_LOAD_NO_BITMAP;
        }

        // Always using FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH to get correct
        // advances, as fontconfig and cairo do.
        // See http://code.google.com/p/skia/issues/detail?id=222.
        loadFlags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;

        // Use vertical layout if requested.
        if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
            loadFlags |= FT_LOAD_VERTICAL_LAYOUT;
        }

        fLoadGlyphFlags = loadFlags;
        fDoLinearMetrics = linearMetrics;
    }

    // now create the FT_Size

    {
        FT_Error    err;

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
       FT_ULong emojiTag1 = FT_MAKE_TAG('C', 'B', 'D', 'T');
       FT_ULong emojiTag2 = FT_MAKE_TAG('C', 'B', 'L', 'C');
       FT_ULong emojiLength = 0x00;
       FT_Error err1, err2;
       FT_Error emojiErr = 0x01;

       fStrikeIndex = -1;
       err1 = FT_Load_Sfnt_Table(fFace, emojiTag1, 0x00, NULL, &emojiLength);
       emojiLength = 0x00;
       err2 = FT_Load_Sfnt_Table(fFace, emojiTag2, 0x00, NULL, &emojiLength);

       if((err1 == 0x00) && (err2 == 0x00)) {
            emojiErr = 0x00;
            fLoadGlyphFlags |= FT_LOAD_COLOR;
            fLoadGlyphFlags &= ~FT_LOAD_NO_BITMAP;
            if(FT_HAS_FIXED_SIZES(fFace))
                fDoLinearMetrics = false;
        }
#endif /* #if defined(SBROWSER_EMOJI_FONT_SUPPORT) */

        err = FT_New_Size(fFace, &fFTSize);
        if (err != 0) {
            SkDEBUGF(("SkScalerContext_FreeType::FT_New_Size(%x): FT_Set_Char_Size(0x%x, 0x%x) returned 0x%x\n",
                        fFaceRec->fFontID, fScaleX, fScaleY, err));
            fFace = NULL;
            return;
        }

        err = FT_Activate_Size(fFTSize);
        if (err != 0) {
            SkDEBUGF(("SkScalerContext_FreeType::FT_Activate_Size(%x, 0x%x, 0x%x) returned 0x%x\n",
                        fFaceRec->fFontID, fScaleX, fScaleY, err));
            fFTSize = NULL;
        }

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
        if((emojiErr == 0x00) && (FT_HAS_FIXED_SIZES(fFace))) {
            int selectedBitmap = fScaleY >> 16;
            int diff = 0xFFFF;
            int currDiff = 0x00;
            for(int ii = 0; ii < fFace->num_fixed_sizes; ii++)
            {
                currDiff = getAbsoluteDiff(fScaleY >> 16 , fFace->available_sizes[ii].size >> 6);
                if(diff > currDiff)
                {
                    diff = currDiff;
                    selectedBitmap = fFace->available_sizes[ii].size >> 6;
                    fStrikeIndex = ii;
                    ftBitmap = fFace->available_sizes[ii];
                }
            }/* end for */
        }/* end if */

        if(fStrikeIndex != -1)
        {
            err = FT_Select_Size(fFace, fStrikeIndex);
            if(err)
            {
               fStrikeIndex = -1;
            }
        }/* end if */

        if(fStrikeIndex == -1)
#endif
        {
        err = FT_Set_Char_Size( fFace,
                                SkFixedToFDot6(fScaleX), SkFixedToFDot6(fScaleY),
                                72, 72);
        if (err != 0) {
            SkDEBUGF(("SkScalerContext_FreeType::FT_Set_Char_Size(%x, 0x%x, 0x%x) returned 0x%x\n",
                        fFaceRec->fFontID, fScaleX, fScaleY, err));
            fFace = NULL;
            return;
        }

        FT_Set_Transform( fFace, &fMatrix22, NULL);
    }
}
}

SkScalerContext_FreeType::~SkScalerContext_FreeType() {
    SkAutoMutexAcquire  ac(gFTMutex);

    if (fFTSize != NULL) {
        FT_Done_Size(fFTSize);
    }

    if (fFace != NULL) {
        unref_ft_face(fFace);
    }
    if (--gFTCount == 0) {
//        SkDEBUGF(("FT_Done_FreeType\n"));
        FT_Done_FreeType(gFTLibrary);
        SkDEBUGCODE(gFTLibrary = NULL;)
    }
}

/*  We call this before each use of the fFace, since we may be sharing
    this face with other context (at different sizes).
*/
FT_Error SkScalerContext_FreeType::setupSize() {
    FT_Error    err = FT_Activate_Size(fFTSize);

    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::FT_Activate_Size(%x, 0x%x, 0x%x) returned 0x%x\n",
                    fFaceRec->fFontID, fScaleX, fScaleY, err));
        fFTSize = NULL;
    } else {
        // seems we need to reset this every time (not sure why, but without it
        // I get random italics from some other fFTSize)
        FT_Set_Transform( fFace, &fMatrix22, NULL);
    }
    return err;
}

unsigned SkScalerContext_FreeType::generateGlyphCount() {
    return fFace->num_glyphs;
}

uint16_t SkScalerContext_FreeType::generateCharToGlyph(SkUnichar uni) {
    return SkToU16(FT_Get_Char_Index( fFace, uni ));
}

SkUnichar SkScalerContext_FreeType::generateGlyphToChar(uint16_t glyph) {
    // iterate through each cmap entry, looking for matching glyph indices
    FT_UInt glyphIndex;
    SkUnichar charCode = FT_Get_First_Char( fFace, &glyphIndex );

    while (glyphIndex != 0) {
        if (glyphIndex == glyph) {
            return charCode;
        }
        charCode = FT_Get_Next_Char( fFace, charCode, &glyphIndex );
    }

    return 0;
}

void SkScalerContext_FreeType::generateAdvance(SkGlyph* glyph) {
#ifdef FT_ADVANCES_H
   /* unhinted and light hinted text have linearly scaled advances
    * which are very cheap to compute with some font formats...
    */
    if (fDoLinearMetrics) {
        SkAutoMutexAcquire  ac(gFTMutex);

        if (this->setupSize()) {
            glyph->zeroMetrics();
            return;
        }

        FT_Error    error;
        FT_Fixed    advance;

        error = FT_Get_Advance( fFace, glyph->getGlyphID(fBaseGlyphCount),
                                fLoadGlyphFlags | FT_ADVANCE_FLAG_FAST_ONLY,
                                &advance );
        if (0 == error) {
            glyph->fRsbDelta = 0;
            glyph->fLsbDelta = 0;
            glyph->fAdvanceX = SkFixedMul(fMatrix22.xx, advance);
            glyph->fAdvanceY = - SkFixedMul(fMatrix22.yx, advance);
            return;
        }
    }
#endif /* FT_ADVANCES_H */
    /* otherwise, we need to load/hint the glyph, which is slower */
    this->generateMetrics(glyph);
    return;
}

void SkScalerContext_FreeType::getBBoxForCurrentGlyph(SkGlyph* glyph,
                                                      FT_BBox* bbox,
                                                      bool snapToPixelBoundary) {

    FT_Outline_Get_CBox(&fFace->glyph->outline, bbox);

    if (fRec.fFlags & SkScalerContext::kSubpixelPositioning_Flag) {
        int dx = SkFixedToFDot6(glyph->getSubXFixed());
        int dy = SkFixedToFDot6(glyph->getSubYFixed());
        // negate dy since freetype-y-goes-up and skia-y-goes-down
        bbox->xMin += dx;
        bbox->yMin -= dy;
        bbox->xMax += dx;
        bbox->yMax -= dy;
    }

    // outset the box to integral boundaries
    if (snapToPixelBoundary) {
        bbox->xMin &= ~63;
        bbox->yMin &= ~63;
        bbox->xMax  = (bbox->xMax + 63) & ~63;
        bbox->yMax  = (bbox->yMax + 63) & ~63;
    }

    // Must come after snapToPixelBoundary so that the width and height are
    // consistent. Otherwise asserts will fire later on when generating the
    // glyph image.
    if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
        FT_Vector vector;
        vector.x = fFace->glyph->metrics.vertBearingX - fFace->glyph->metrics.horiBearingX;
        vector.y = -fFace->glyph->metrics.vertBearingY - fFace->glyph->metrics.horiBearingY;
        FT_Vector_Transform(&vector, &fMatrix22);
        bbox->xMin += vector.x;
        bbox->xMax += vector.x;
        bbox->yMin += vector.y;
        bbox->yMax += vector.y;
    }
}

void SkScalerContext_FreeType::updateGlyphIfLCD(SkGlyph* glyph) {
    if (isLCD(fRec)) {
        if (fLCDIsVert) {
            glyph->fHeight += gLCDExtra;
            glyph->fTop -= gLCDExtra >> 1;
        } else {
            glyph->fWidth += gLCDExtra;
            glyph->fLeft -= gLCDExtra >> 1;
        }
    }
}

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
inline void scaleGlyphMetrics(SkGlyph& glyph, SkScalar scale) {
    glyph.fWidth = SkToU16(SkScalarRoundToInt(SkScalarMul(SkIntToScalar(glyph.fWidth), scale)));
    glyph.fHeight = SkToU16(SkScalarRoundToInt(SkScalarMul(SkIntToScalar(glyph.fHeight), scale)));

    glyph.fTop = SkToS16(SkScalarRoundToInt(SkScalarMul(SkIntToScalar(glyph.fTop), scale)));
    glyph.fLeft = SkToS16(SkScalarRoundToInt(SkScalarMul(SkIntToScalar(glyph.fLeft), scale)));

    SkFixed fixedScale = SkScalarToFixed(scale);
    glyph.fAdvanceX = SkFixedMul(glyph.fAdvanceX, fixedScale);
    glyph.fAdvanceY = SkFixedMul(glyph.fAdvanceY, fixedScale);
}

bool SkScalerContext_FreeType::isColoredBitmap()
{
    return getColoredGlyph();
}

#endif /* #if defined(SBROWSER_EMOJI_FONT_SUPPORT) */

void SkScalerContext_FreeType::generateMetrics(SkGlyph* glyph) {
    SkAutoMutexAcquire  ac(gFTMutex);

    glyph->fRsbDelta = 0;
    glyph->fLsbDelta = 0;

    FT_Error    err;

    if (this->setupSize()) {
        goto ERROR;
    }

    err = FT_Load_Glyph( fFace, glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags );
    if (err != 0) {
#if 0
        SkDEBUGF(("SkScalerContext_FreeType::generateMetrics(%x): FT_Load_Glyph(glyph:%d flags:%x) returned 0x%x\n",
                    fFaceRec->fFontID, glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags, err));
#endif
    ERROR:
        glyph->zeroMetrics();
        return;
    }

    switch ( fFace->glyph->format ) {
      case FT_GLYPH_FORMAT_OUTLINE: {
        FT_BBox bbox;

        if (0 == fFace->glyph->outline.n_contours) {
            glyph->fWidth = 0;
            glyph->fHeight = 0;
            glyph->fTop = 0;
            glyph->fLeft = 0;
            break;
        }

        if(!FT_HAS_FIXED_SIZES(fFace))
        {
        if (fRec.fFlags & kEmbolden_Flag) {
            emboldenOutline(fFace, &fFace->glyph->outline);
        }
        }

        getBBoxForCurrentGlyph(glyph, &bbox, true);

        glyph->fWidth   = SkToU16(SkFDot6Floor(bbox.xMax - bbox.xMin));
        glyph->fHeight  = SkToU16(SkFDot6Floor(bbox.yMax - bbox.yMin));
        glyph->fTop     = -SkToS16(SkFDot6Floor(bbox.yMax));
        glyph->fLeft    = SkToS16(SkFDot6Floor(bbox.xMin));

        updateGlyphIfLCD(glyph);

        break;
      }

      case FT_GLYPH_FORMAT_BITMAP:
        if (fRec.fFlags & kEmbolden_Flag) {
            FT_GlyphSlot_Own_Bitmap(fFace->glyph);
            FT_Bitmap_Embolden(gFTLibrary, &fFace->glyph->bitmap, kBitmapEmboldenStrength, 0);
        }

        if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
            FT_Vector vector;
            vector.x = fFace->glyph->metrics.vertBearingX - fFace->glyph->metrics.horiBearingX;
            vector.y = -fFace->glyph->metrics.vertBearingY - fFace->glyph->metrics.horiBearingY;
            FT_Vector_Transform(&vector, &fMatrix22);
            fFace->glyph->bitmap_left += SkFDot6Floor(vector.x);
            fFace->glyph->bitmap_top  += SkFDot6Floor(vector.y);
        }

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
        if (fFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA){
            glyph->fMaskFormat = SkMask::kARGB32_Format;
            setColoredGlyph(true);
        }
        else {
            setColoredGlyph(false);
        }
#endif

        glyph->fWidth   = SkToU16(fFace->glyph->bitmap.width);
        glyph->fHeight  = SkToU16(fFace->glyph->bitmap.rows);
        glyph->fTop     = -SkToS16(fFace->glyph->bitmap_top);
        glyph->fLeft    = SkToS16(fFace->glyph->bitmap_left);
        break;

      default:
        SkDEBUGFAIL("unknown glyph format");
        goto ERROR;
    }

    if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
        if (fDoLinearMetrics) {
            glyph->fAdvanceX = -SkFixedMul(fMatrix22.xy, fFace->glyph->linearVertAdvance);
            glyph->fAdvanceY = SkFixedMul(fMatrix22.yy, fFace->glyph->linearVertAdvance);
        } else {
            glyph->fAdvanceX = -SkFDot6ToFixed(fFace->glyph->advance.x);
            glyph->fAdvanceY = SkFDot6ToFixed(fFace->glyph->advance.y);
        }
    } else {
        if (fDoLinearMetrics) {
            glyph->fAdvanceX = SkFixedMul(fMatrix22.xx, fFace->glyph->linearHoriAdvance);
            glyph->fAdvanceY = -SkFixedMul(fMatrix22.yx, fFace->glyph->linearHoriAdvance);
        } else {
            glyph->fAdvanceX = SkFDot6ToFixed(fFace->glyph->advance.x);
            glyph->fAdvanceY = -SkFDot6ToFixed(fFace->glyph->advance.y);

            if (fRec.fFlags & kDevKernText_Flag) {
                glyph->fRsbDelta = SkToS8(fFace->glyph->rsb_delta);
                glyph->fLsbDelta = SkToS8(fFace->glyph->lsb_delta);
            }
        }
    }

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
    if (fFace->glyph->format == FT_GLYPH_FORMAT_BITMAP && fScaleY && fFace->size->metrics.y_ppem) {
        scaleGlyphMetrics(*glyph, SkScalarDiv(SkFixedToScalar(fScaleY),
                                              SkIntToScalar(fFace->size->metrics.y_ppem)));
    }
#endif

#ifdef ENABLE_GLYPH_SPEW
    SkDEBUGF(("FT_Set_Char_Size(this:%p sx:%x sy:%x ", this, fScaleX, fScaleY));
    SkDEBUGF(("Metrics(glyph:%d flags:0x%x) w:%d\n", glyph->getGlyphID(fBaseGlyphCount), fLoadGlyphFlags, glyph->fWidth));
#endif
}


void SkScalerContext_FreeType::generateImage(const SkGlyph& glyph) {
    SkAutoMutexAcquire  ac(gFTMutex);

    FT_Error    err;

    if (this->setupSize()) {
        goto ERROR;
    }

    err = FT_Load_Glyph( fFace, glyph.getGlyphID(fBaseGlyphCount), fLoadGlyphFlags);
    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generateImage: FT_Load_Glyph(glyph:%d width:%d height:%d rb:%d flags:%d) returned 0x%x\n",
                    glyph.getGlyphID(fBaseGlyphCount), glyph.fWidth, glyph.fHeight, glyph.rowBytes(), fLoadGlyphFlags, err));
    ERROR:
        memset(glyph.fImage, 0, glyph.rowBytes() * glyph.fHeight);
        return;
    }

    generateGlyphImage(fFace, glyph);
}


void SkScalerContext_FreeType::generatePath(const SkGlyph& glyph,
                                            SkPath* path) {
    SkAutoMutexAcquire  ac(gFTMutex);

    SkASSERT(&glyph && path);

    if (this->setupSize()) {
        path->reset();
        return;
    }

    uint32_t flags = fLoadGlyphFlags;
    flags |= FT_LOAD_NO_BITMAP; // ignore embedded bitmaps so we're sure to get the outline
    flags &= ~FT_LOAD_RENDER;   // don't scan convert (we just want the outline)

    FT_Error err = FT_Load_Glyph( fFace, glyph.getGlyphID(fBaseGlyphCount), flags);

    if (err != 0) {
        SkDEBUGF(("SkScalerContext_FreeType::generatePath: FT_Load_Glyph(glyph:%d flags:%d) returned 0x%x\n",
                    glyph.getGlyphID(fBaseGlyphCount), flags, err));
        path->reset();
        return;
    }

    generateGlyphPath(fFace, path);

    // The path's origin from FreeType is always the horizontal layout origin.
    // Offset the path so that it is relative to the vertical origin if needed.
    if (fRec.fFlags & SkScalerContext::kVertical_Flag) {
        FT_Vector vector;
        vector.x = fFace->glyph->metrics.vertBearingX - fFace->glyph->metrics.horiBearingX;
        vector.y = -fFace->glyph->metrics.vertBearingY - fFace->glyph->metrics.horiBearingY;
        FT_Vector_Transform(&vector, &fMatrix22);
        path->offset(SkFDot6ToScalar(vector.x), -SkFDot6ToScalar(vector.y));
    }
}

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
void SkScalerContext_FreeType::generateFontMetrics(SkPaint::FontMetrics* mx,
                                                   SkPaint::FontMetrics* my) {
    if (NULL == mx && NULL == my) {
        return;
    }

    SkAutoMutexAcquire  ac(gFTMutex);

    if (this->setupSize()) {
        ERROR:
        if (mx) {
            sk_bzero(mx, sizeof(SkPaint::FontMetrics));
        }
        if (my) {
            sk_bzero(my, sizeof(SkPaint::FontMetrics));
        }
        return;
    }

    FT_Face face = fFace;
    SkScalar scaleX = fScale.x();
    SkScalar scaleY = fScale.y();
    SkScalar mxy = fMatrix22Scalar.getSkewX() * scaleY;
    SkScalar myy = fMatrix22Scalar.getScaleY() * scaleY;

    SkScalar upem = SkIntToScalar(face->units_per_EM);
    if (!upem) {
        TT_Header* ttHeader = (TT_Header*)FT_Get_Sfnt_Table(face, ft_sfnt_head);
        if (ttHeader) {
            upem = SkIntToScalar(ttHeader->Units_Per_EM);
        }
    }

    SkScalar x_height = 0.0f;
    SkScalar avgCharWidth = 0.0f;
    TT_OS2* os2 = (TT_OS2*) FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    if (os2) {
        x_height = scaleX * SkIntToScalar(os2->sxHeight) / upem;
        avgCharWidth = SkIntToScalar(os2->xAvgCharWidth) / upem;
    }

    // pull from format-specific metrics as needed
    SkScalar ascent, descent, leading, xmin, xmax, ymin, ymax;
#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
    if (fStrikeIndex == -1) {
#else
    if (face->face_flags & FT_FACE_FLAG_SCALABLE) {
#endif
        ascent = -SkIntToScalar(face->ascender) / upem;
        descent = -SkIntToScalar(face->descender) / upem;
        leading = SkIntToScalar(face->height + (face->descender - face->ascender)) / upem;
        xmin = SkIntToScalar(face->bbox.xMin) / upem;
        xmax = SkIntToScalar(face->bbox.xMax) / upem;
        ymin = -SkIntToScalar(face->bbox.yMin) / upem;
        ymax = -SkIntToScalar(face->bbox.yMax) / upem;
        // we may be able to synthesize x_height from outline
        if (!x_height) {
            const FT_UInt x_glyph = FT_Get_Char_Index(fFace, 'x');
            if (x_glyph) {
                FT_BBox bbox;
                FT_Load_Glyph(fFace, x_glyph, fLoadGlyphFlags);
                if ((fRec.fFlags & kEmbolden_Flag) && !(fFace->style_flags & FT_STYLE_FLAG_BOLD)) {
                    emboldenOutline(fFace, &fFace->glyph->outline);
                }
                FT_Outline_Get_CBox(&fFace->glyph->outline, &bbox);
                x_height = SkIntToScalar(bbox.yMax) / 64.0f;
            }
        }
    } 
#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
    else if (fStrikeIndex != -1) {
        SkScalar xppem = SkIntToScalar(face->size->metrics.x_ppem);
        SkScalar yppem = SkIntToScalar(face->size->metrics.y_ppem);
        ascent = -SkIntToScalar(face->size->metrics.ascender) / (yppem * 64.0f);
        descent = -SkIntToScalar(face->size->metrics.descender) / (yppem * 64.0f);
        leading = (SkIntToScalar(face->size->metrics.height) / (yppem * 64.0f))
                + ascent - descent;
        xmin = 0.0f;
        xmax = SkIntToScalar(face->available_sizes[fStrikeIndex].width) / xppem;
        ymin = descent + leading;
        ymax = ascent - descent;
        if (!x_height) {
            x_height = -ascent;
        }
        if (!avgCharWidth) {
            avgCharWidth = xmax - xmin;
        }
    } else {
        goto ERROR;
    }
#endif

    // disallow negative linespacing
    if (leading < 0.0f) {
        leading = 0.0f;
    }

    if (mx) {
        mx->fTop = ymax * mxy;
        mx->fAscent = ascent * mxy;
        mx->fDescent = descent * mxy;
        mx->fBottom = ymin * mxy;
        mx->fLeading = leading * mxy;
        mx->fAvgCharWidth = avgCharWidth * mxy;
        mx->fXMin = xmin;
        mx->fXMax = xmax;
        mx->fXHeight = x_height;
    }
    if (my) {
        my->fTop = ymax * myy;
        my->fAscent = ascent * myy;
        my->fDescent = descent * myy;
        my->fBottom = ymin * myy;
        my->fLeading = leading * myy;
        my->fAvgCharWidth = avgCharWidth * myy;
        my->fXMin = xmin;
        my->fXMax = xmax;
        my->fXHeight = x_height;
    }
}
#else
void SkScalerContext_FreeType::generateFontMetrics(SkPaint::FontMetrics* mx,
                                                   SkPaint::FontMetrics* my) {
    if (NULL == mx && NULL == my) {
        return;
    }

    SkAutoMutexAcquire  ac(gFTMutex);

    if (this->setupSize()) {
        ERROR:
        if (mx) {
            sk_bzero(mx, sizeof(SkPaint::FontMetrics));
        }
        if (my) {
            sk_bzero(my, sizeof(SkPaint::FontMetrics));
        }
        return;
    }

    FT_Face face = fFace;
    int upem = face->units_per_EM;

#if defined(SBROWSER_EMOJI_FONT_SUPPORT)
    FT_ULong emojiTag1 = FT_MAKE_TAG('C', 'B', 'D', 'T');
    FT_ULong emojiTag2 = FT_MAKE_TAG('C', 'B', 'L', 'C');
    FT_ULong emojiLength = 0x00;
    FT_Error err1, err2;
    err1 = FT_Load_Sfnt_Table(face, emojiTag1, 0x00, NULL, &emojiLength);
    emojiLength = 0x00;
    err2 = FT_Load_Sfnt_Table(face, emojiTag2, 0x00, NULL, &emojiLength);
    
    if((FT_HAS_FIXED_SIZES(fFace)) && (err1 == 0x00) && (err2 == 0x00)) {
        TT_OS2* fontOS2 = (TT_OS2*) FT_Get_Sfnt_Table(face, ft_sfnt_os2);
        if(fontOS2 != NULL) {
            upem = face->units_per_EM = 2048;
            face->bbox.xMin = 0;
            face->bbox.xMax = ftBitmap.width << 6;
            face->bbox.yMax = face->ascender = face->height = fontOS2->sTypoAscender;
            face->bbox.yMin = face->descender = fontOS2->sTypoDescender;
        }
    }
#endif
        
    if (upem <= 0) {
        goto ERROR;
    }

    SkPoint pts[6];
    SkFixed ys[6];
    SkScalar scaleY = fScale.y();
    SkScalar mxy = fMatrix22Scalar.getSkewX();
    SkScalar myy = fMatrix22Scalar.getScaleY();
    SkScalar xmin = SkIntToScalar(face->bbox.xMin) / upem;
    SkScalar xmax = SkIntToScalar(face->bbox.xMax) / upem;

    int leading = face->height - (face->ascender + -face->descender);
    if (leading < 0) {
        leading = 0;
    }

    // Try to get the OS/2 table from the font. This contains the specific
    // average font width metrics which Windows uses.
    TT_OS2* os2 = (TT_OS2*) FT_Get_Sfnt_Table(face, ft_sfnt_os2);

    ys[0] = -face->bbox.yMax;
    ys[1] = -face->ascender;
    ys[2] = -face->descender;
    ys[3] = -face->bbox.yMin;
    ys[4] = leading;
    ys[5] = os2 ? os2->xAvgCharWidth : 0;

    SkScalar x_height;
    if (os2 && os2->sxHeight) {
        x_height = fScale.x() * os2->sxHeight / upem;
    } else {
        const FT_UInt x_glyph = FT_Get_Char_Index(fFace, 'x');
        if (x_glyph) {
            FT_BBox bbox;
            FT_Load_Glyph(fFace, x_glyph, fLoadGlyphFlags);
            if (fRec.fFlags & kEmbolden_Flag) {
                emboldenOutline(fFace, &fFace->glyph->outline);
            }
            FT_Outline_Get_CBox(&fFace->glyph->outline, &bbox);
            x_height = bbox.yMax / 64.0f;
        } else {
            x_height = 0;
        }
    }

    // convert upem-y values into scalar points
    for (int i = 0; i < 6; i++) {
        SkScalar y = scaleY * ys[i] / upem;
        pts[i].set(y * mxy, y * myy);
    }

    if (mx) {
        mx->fTop = pts[0].fX;
        mx->fAscent = pts[1].fX;
        mx->fDescent = pts[2].fX;
        mx->fBottom = pts[3].fX;
        mx->fLeading = pts[4].fX;
        mx->fAvgCharWidth = pts[5].fX;
        mx->fXMin = xmin;
        mx->fXMax = xmax;
        mx->fXHeight = x_height;
    }
    if (my) {
        my->fTop = pts[0].fY;
        my->fAscent = pts[1].fY;
        my->fDescent = pts[2].fY;
        my->fBottom = pts[3].fY;
        my->fLeading = pts[4].fY;
        my->fAvgCharWidth = pts[5].fY;
        my->fXMin = xmin;
        my->fXMax = xmax;
        my->fXHeight = x_height;
    }
}
#endif

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

/*  Export this so that other parts of our FonttHost port can make use of our
    ability to extract the name+style from a stream, using FreeType's api.
*/
bool find_name_and_attributes(SkStream* stream, SkString* name,
                              SkTypeface::Style* style, bool* isFixedPitch) {
    FT_Library  library;
    if (FT_Init_FreeType(&library)) {
        return false;
    }

    FT_Open_Args    args;
    memset(&args, 0, sizeof(args));

    const void* memoryBase = stream->getMemoryBase();
    FT_StreamRec    streamRec;

    if (NULL != memoryBase) {
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = (const FT_Byte*)memoryBase;
        args.memory_size = stream->getLength();
    } else {
        memset(&streamRec, 0, sizeof(streamRec));
        streamRec.size = stream->getLength();
        streamRec.descriptor.pointer = stream;
        streamRec.read  = sk_stream_read;
        streamRec.close = sk_stream_close;

        args.flags = FT_OPEN_STREAM;
        args.stream = &streamRec;
    }

    FT_Face face;
    if (FT_Open_Face(library, &args, 0, &face)) {
        FT_Done_FreeType(library);
        return false;
    }

    int tempStyle = SkTypeface::kNormal;
    if (face->style_flags & FT_STYLE_FLAG_BOLD) {
        tempStyle |= SkTypeface::kBold;
    }
    if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
        tempStyle |= SkTypeface::kItalic;
    }

    if (name) {
        name->set(face->family_name);
    }
    if (style) {
        *style = (SkTypeface::Style) tempStyle;
    }
    if (isFixedPitch) {
        *isFixedPitch = FT_IS_FIXED_WIDTH(face);
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return true;
}
