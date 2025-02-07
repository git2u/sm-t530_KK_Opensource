/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFontHost.h"
#include "SkFontHost_FreeType_common.h"
#include "SkFontDescriptor.h"
#include "SkGraphics.h"
#include "SkDescriptor.h"
#include "SkPaint.h"
#include "SkString.h"
#include "SkStream.h"
#include "SkThread.h"
#include "SkTSearch.h"
#include "SkTypeface_android.h"
#include "FontHostConfiguration_android.h"

#ifndef SK_FONT_FILE_PREFIX
    #define SK_FONT_FILE_PREFIX          "/fonts/"
#endif

#ifndef SK_DEBUG_FONTS
    #define SK_DEBUG_FONTS 0
#endif

// For test only.
static const char* gTestMainConfigFile = NULL;
static const char* gTestFallbackConfigFile = NULL;
static const char* gTestFontFilePrefix = NULL;

bool find_name_and_attributes(SkStream* stream, SkString* name,
                              SkTypeface::Style* style, bool* isFixedPitch);

static void GetFullPathForSysFonts(SkString* full, const char name[]) {
    if (gTestFontFilePrefix) {
        full->set(gTestFontFilePrefix);
    } else {
        full->set(getenv("ANDROID_ROOT"));
        full->append(SK_FONT_FILE_PREFIX);
    }
    full->append(name);
}

///////////////////////////////////////////////////////////////////////////////

struct FamilyRec;

/*  This guy holds a mapping of a name -> family, used for looking up fonts.
    Since it is stored in a stretchy array that doesn't preserve object
    semantics, we don't use constructor/destructors, but just have explicit
    helpers to manage our internal bookkeeping.
*/
struct NameFamilyPair {
    const char* fName;      // we own this
    FamilyRec*  fFamily;    // we don't own this, we just reference it

    void construct(const char name[], FamilyRec* family) {
        fName = strdup(name);
        fFamily = family;   // we don't own this, so just record the referene
    }

    void destruct() {
        free((char*)fName);
        // we don't own family, so just ignore our reference
    }
};
typedef SkTDArray<NameFamilyPair> NameFamilyPairList;

// we use atomic_inc to grow this for each typeface we create
static int32_t gUniqueFontID;

// this is the mutex that protects gFamilyHead and GetNameList()
SK_DECLARE_STATIC_MUTEX(gFamilyHeadAndNameListMutex);
static FamilyRec* gFamilyHead;

static NameFamilyPairList& GetNameList() {
    /*
     *  It is assumed that the caller has already acquired a lock on
     *  gFamilyHeadAndNameListMutex before calling this.
     */
    static NameFamilyPairList* gNameList;
    if (NULL == gNameList) {
        gNameList = SkNEW(NameFamilyPairList);
        // register a delete proc with sk_atexit(..) when available
    }
    return *gNameList;
}

struct FamilyRec {
    FamilyRec*  fNext;
    SkTypeface* fFaces[4];

    FamilyRec()
    {
        fNext = gFamilyHead;
        memset(fFaces, 0, sizeof(fFaces));
        gFamilyHead = this;
    }
};

static SkTypeface* find_best_face(const FamilyRec* family,
                                  SkTypeface::Style style) {
    SkTypeface* const* faces = family->fFaces;

    if (faces[style] != NULL) { // exact match
        return faces[style];
    }
    // look for a matching bold
    style = (SkTypeface::Style)(style ^ SkTypeface::kItalic);
    if (faces[style] != NULL) {
        return faces[style];
    }
    // look for the plain
    if (faces[SkTypeface::kNormal] != NULL) {
        return faces[SkTypeface::kNormal];
    }
    // look for anything
    for (int i = 0; i < 4; i++) {
        if (faces[i] != NULL) {
            return faces[i];
        }
    }
    // should never get here, since the faces list should not be empty
    SkDEBUGFAIL("faces list is empty");
    return NULL;
}

static FamilyRec* find_family(const SkTypeface* member) {
    FamilyRec* curr = gFamilyHead;
    while (curr != NULL) {
        for (int i = 0; i < 4; i++) {
            if (curr->fFaces[i] == member) {
                return curr;
            }
        }
        curr = curr->fNext;
    }
    return NULL;
}

//  gFamilyHeadAndNameListMutex must already be acquired
static const char* find_family_name(const SkTypeface* member) {
    FamilyRec* family = find_family(member);
    if (NULL == family) {
        return NULL;
    }

    NameFamilyPairList& namelist = GetNameList();
    NameFamilyPair* list = namelist.begin();
    int             count = namelist.count();

    for (int i = 0; i < count; i++) {
        NameFamilyPair* pair = &list[i];
        if (pair->fFamily == family) {
            return pair->fName;
        }
    }

    return NULL;
}

/*  Returns the matching typeface, or NULL. If a typeface is found, its refcnt
    is not modified.
 */
static SkTypeface* find_from_uniqueID(uint32_t uniqueID) {
    FamilyRec* curr = gFamilyHead;
    while (curr != NULL) {
        for (int i = 0; i < 4; i++) {
            SkTypeface* face = curr->fFaces[i];
            if (face != NULL && face->uniqueID() == uniqueID) {
                return face;
            }
        }
        curr = curr->fNext;
    }
    return NULL;
}

/*  Remove reference to this face from its family. If the resulting family
    is empty (has no faces), return that family, otherwise return NULL
*/
static FamilyRec* remove_from_family(const SkTypeface* face) {
    FamilyRec* family = find_family(face);
    if (family) {
        SkASSERT(family->fFaces[face->style()] == face);
        family->fFaces[face->style()] = NULL;

        for (int i = 0; i < 4; i++) {
            if (family->fFaces[i] != NULL) {    // family is non-empty
                return NULL;
            }
        }
    } else {
//        SkDebugf("remove_from_family(%p) face not found", face);
    }
    return family;  // return the empty family
}

// maybe we should make FamilyRec be doubly-linked
static void detach_and_delete_family(FamilyRec* family) {
    FamilyRec* curr = gFamilyHead;
    FamilyRec* prev = NULL;

    while (curr != NULL) {
        FamilyRec* next = curr->fNext;
        if (curr == family) {
            if (prev == NULL) {
                gFamilyHead = next;
            } else {
                prev->fNext = next;
            }
            SkDELETE(family);
            return;
        }
        prev = curr;
        curr = next;
    }
    SkASSERT(!"Yikes, couldn't find family in our list to remove/delete");
}

//  gFamilyHeadAndNameListMutex must already be acquired
static SkTypeface* find_typeface(const char name[], SkTypeface::Style style) {
    NameFamilyPairList& namelist = GetNameList();
    NameFamilyPair* list = namelist.begin();
    int             count = namelist.count();

    int index = SkStrLCSearch(&list[0].fName, count, name, sizeof(list[0]));

    if (index >= 0) {
        return find_best_face(list[index].fFamily, style);
    }
    return NULL;
}

//  gFamilyHeadAndNameListMutex must already be acquired
static SkTypeface* find_typeface(const SkTypeface* familyMember,
                                 SkTypeface::Style style) {
    const FamilyRec* family = find_family(familyMember);
    return family ? find_best_face(family, style) : NULL;
}

//  gFamilyHeadAndNameListMutex must already be acquired
static void add_name(const char name[], FamilyRec* family) {
    SkAutoAsciiToLC tolc(name);
    name = tolc.lc();

    NameFamilyPairList& namelist = GetNameList();
    NameFamilyPair* list = namelist.begin();
    int             count = namelist.count();

    int index = SkStrLCSearch(&list[0].fName, count, name, sizeof(list[0]));

    if (index < 0) {
        list = namelist.insert(~index);
        list->construct(name, family);
    }
}

//  gFamilyHeadAndNameListMutex must already be acquired
static void remove_from_names(FamilyRec* emptyFamily) {
#ifdef SK_DEBUG
    for (int i = 0; i < 4; i++) {
        SkASSERT(emptyFamily->fFaces[i] == NULL);
    }
#endif

    SkTDArray<NameFamilyPair>& list = GetNameList();

    // must go backwards when removing
    for (int i = list.count() - 1; i >= 0; --i) {
        NameFamilyPair* pair = &list[i];
        if (pair->fFamily == emptyFamily) {
            pair->destruct();
            list.remove(i);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

class FamilyTypeface : public SkTypeface_FreeType {
public:
    FamilyTypeface(Style style, bool sysFont, SkTypeface* familyMember,
                   bool isFixedPitch)
    : INHERITED(style, sk_atomic_inc(&gUniqueFontID) + 1, isFixedPitch) {
        fIsSysFont = sysFont;

        // our caller has acquired the gFamilyHeadAndNameListMutex so this is safe
        FamilyRec* rec = NULL;
        if (familyMember) {
            rec = find_family(familyMember);
            SkASSERT(rec);
        } else {
            rec = SkNEW(FamilyRec);
        }
        rec->fFaces[style] = this;
    }

    virtual ~FamilyTypeface() {
        SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);

        // remove us from our family. If the family is now empty, we return
        // that and then remove that family from the name list
        FamilyRec* family = remove_from_family(this);
        if (NULL != family) {
            remove_from_names(family);
            detach_and_delete_family(family);
        }
    }

    bool isSysFont() const { return fIsSysFont; }

    virtual const char* getUniqueString() const = 0;
    virtual const char* getFilePath() const = 0;

protected:
    virtual void onGetFontDescriptor(SkFontDescriptor*, bool*) const SK_OVERRIDE;

private:
    bool    fIsSysFont;

    typedef SkTypeface_FreeType INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class StreamTypeface : public FamilyTypeface {
public:
    StreamTypeface(Style style, bool sysFont, SkTypeface* familyMember,
                   SkStream* stream, bool isFixedPitch)
    : INHERITED(style, sysFont, familyMember, isFixedPitch) {
        SkASSERT(stream);
        stream->ref();
        fStream = stream;
    }
    virtual ~StreamTypeface() {
        fStream->unref();
    }

    virtual const char* getUniqueString() const SK_OVERRIDE { return NULL; }
    virtual const char* getFilePath() const SK_OVERRIDE { return NULL; }

protected:
    virtual SkStream* onOpenStream(int* ttcIndex) const SK_OVERRIDE {
        *ttcIndex = 0;
        // we just ref our existing stream, since the caller will call unref()
        // when they are through
        fStream->ref();
        // must rewind each time, since the caller assumes a "new" stream
        fStream->rewind();
        return fStream;
    }

private:
    SkStream* fStream;

    typedef FamilyTypeface INHERITED;
};

class FileTypeface : public FamilyTypeface {
public:
    FileTypeface(Style style, bool sysFont, SkTypeface* familyMember,
                 const char path[], bool isFixedPitch)
    : INHERITED(style, sysFont, familyMember, isFixedPitch) {
        SkString fullpath;

        if (sysFont) {
            GetFullPathForSysFonts(&fullpath, path);
            path = fullpath.c_str();
        }
        fPath.set(path);
    }

    virtual const char* getUniqueString() const SK_OVERRIDE {
        const char* str = strrchr(fPath.c_str(), '/');
        if (str) {
            str += 1;   // skip the '/'
        }
        return str;
    }

    virtual const char* getFilePath() const SK_OVERRIDE {
        return fPath.c_str();
    }

protected:
    virtual SkStream* onOpenStream(int* ttcIndex) const SK_OVERRIDE {
        *ttcIndex = 0;
        return SkStream::NewFromFile(fPath.c_str());
    }

private:
    SkString fPath;

    typedef FamilyTypeface INHERITED;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static bool get_name_and_style(const char path[], SkString* name,
                               SkTypeface::Style* style,
                               bool* isFixedPitch, bool isExpected) {
    SkString        fullpath;
    GetFullPathForSysFonts(&fullpath, path);

    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(fullpath.c_str()));
    if (stream.get()) {
        return find_name_and_attributes(stream, name, style, isFixedPitch);
    } else {
        if (isExpected) {
            SkDebugf("---- failed to open <%s> as a font", fullpath.c_str());
        }
        return false;
    }
}

// used to record our notion of the pre-existing fonts
struct FontInitRec {
    const char*         fFileName;
    const char* const*  fNames;     // null-terminated list
};

// deliberately empty, but we use the address to identify fallback fonts
static const char* gFBNames[] = { NULL };

/*  Fonts are grouped by family, with the first font in a family having the
    list of names (even if that list is empty), and the following members having
    null for the list. The names list must be NULL-terminated.
*/
static FontInitRec *gSystemFonts;
static size_t gNumSystemFonts = 0;

// these globals are assigned (once) by load_system_fonts()
static FamilyRec* gDefaultFamily;
static SkTypeface* gDefaultNormal;
static char** gDefaultNames = NULL;
static uint32_t *gFallbackFonts;

#if SK_DEBUG_FONTS
static void dump_globals() {
    SkDebugf("gDefaultNormal=%p id=%u refCnt=%d", gDefaultNormal,
             gDefaultNormal ? gDefaultNormal->uniqueID() : 0,
             gDefaultNormal ? gDefaultNormal->getRefCnt() : 0);

    if (gDefaultFamily) {
        SkDebugf("gDefaultFamily=%p fFaces={%u,%u,%u,%u} refCnt={%d,%d,%d,%d}",
                 gDefaultFamily,
                 gDefaultFamily->fFaces[0] ? gDefaultFamily->fFaces[0]->uniqueID() : 0,
                 gDefaultFamily->fFaces[1] ? gDefaultFamily->fFaces[1]->uniqueID() : 0,
                 gDefaultFamily->fFaces[2] ? gDefaultFamily->fFaces[2]->uniqueID() : 0,
                 gDefaultFamily->fFaces[3] ? gDefaultFamily->fFaces[3]->uniqueID() : 0,
                 gDefaultFamily->fFaces[0] ? gDefaultFamily->fFaces[0]->getRefCnt() : 0,
                 gDefaultFamily->fFaces[1] ? gDefaultFamily->fFaces[1]->getRefCnt() : 0,
                 gDefaultFamily->fFaces[2] ? gDefaultFamily->fFaces[2]->getRefCnt() : 0,
                 gDefaultFamily->fFaces[3] ? gDefaultFamily->fFaces[3]->getRefCnt() : 0);
    } else {
        SkDebugf("gDefaultFamily=%p", gDefaultFamily);
    }

    SkDebugf("gNumSystemFonts=%d gSystemFonts=%p gFallbackFonts=%p",
             gNumSystemFonts, gSystemFonts, gFallbackFonts);

    for (size_t i = 0; i < gNumSystemFonts; ++i) {
        SkDebugf("gSystemFonts[%d] fileName=%s", i, gSystemFonts[i].fFileName);
        size_t namesIndex = 0;
        if (gSystemFonts[i].fNames)
            for (const char* fontName = gSystemFonts[i].fNames[namesIndex];
                    fontName != 0;
                    fontName = gSystemFonts[i].fNames[++namesIndex]) {
                SkDebugf("       name[%u]=%s", namesIndex, fontName);
            }
    }

    if (gFamilyHead) {
        FamilyRec* rec = gFamilyHead;
        int i=0;
        while (rec) {
            SkDebugf("gFamilyHead[%d]=%p fFaces={%u,%u,%u,%u} refCnt={%d,%d,%d,%d}",
                     i++, rec,
                     rec->fFaces[0] ? rec->fFaces[0]->uniqueID() : 0,
                     rec->fFaces[1] ? rec->fFaces[1]->uniqueID() : 0,
                     rec->fFaces[2] ? rec->fFaces[2]->uniqueID() : 0,
                     rec->fFaces[3] ? rec->fFaces[3]->uniqueID() : 0,
                     rec->fFaces[0] ? rec->fFaces[0]->getRefCnt() : 0,
                     rec->fFaces[1] ? rec->fFaces[1]->getRefCnt() : 0,
                     rec->fFaces[2] ? rec->fFaces[2]->getRefCnt() : 0,
                     rec->fFaces[3] ? rec->fFaces[3]->getRefCnt() : 0);
            rec = rec->fNext;
        }
    } else {
        SkDebugf("gFamilyHead=%p", gFamilyHead);
    }

}
#endif


/*  Load info from a configuration file that populates the system/fallback font structures
*/
static void load_font_info() {
    SkTDArray<FontFamily*> fontFamilies;
    if (gTestMainConfigFile) {
        getTestFontFamilies(fontFamilies, gTestMainConfigFile, gTestFallbackConfigFile);
    } else {
        getFontFamilies(fontFamilies);
    }

    SkTDArray<FontInitRec> fontInfo;
    for (int i = 0; i < fontFamilies.count(); ++i) {
        FontFamily *family = fontFamilies[i];
        for (int j = 0; j < family->fFileNames.count(); ++j) {
            FontInitRec fontInfoRecord;
            fontInfoRecord.fFileName = family->fFileNames[j];
            if (j == 0) {
                if (family->fNames.count() == 0) {
                    // Fallback font
                    fontInfoRecord.fNames = (char **)gFBNames;
                } else {
                    SkTDArray<const char*> names = family->fNames;
                    const char **nameList = (const char**)
                            malloc((names.count() + 1) * sizeof(char*));
                    if (nameList == NULL) {
                        // shouldn't get here
                        SkDEBUGFAIL("Failed to allocate nameList");
                        break;
                    }
                    if (gDefaultNames == NULL) {
                        gDefaultNames = (char**) nameList;
                    }
                    for (int i = 0; i < names.count(); ++i) {
                        nameList[i] = names[i];
                    }
                    nameList[names.count()] = NULL;
                    fontInfoRecord.fNames = nameList;
                }
            } else {
                fontInfoRecord.fNames = NULL;
            }
            *fontInfo.append() = fontInfoRecord;
        }
    }
    gNumSystemFonts = fontInfo.count();
    gSystemFonts = (FontInitRec*) malloc(gNumSystemFonts * sizeof(FontInitRec));
    gFallbackFonts = (uint32_t*) malloc((gNumSystemFonts + 1) * sizeof(uint32_t));
    if (gSystemFonts == NULL) {
        // shouldn't get here
        SkDEBUGFAIL("No system fonts were found");
        gNumSystemFonts = 0;
    }

#if SK_DEBUG_FONTS
    SkDebugf("---- We have %d system fonts", gNumSystemFonts);
#endif
    for (size_t i = 0; i < gNumSystemFonts; ++i) {
        gSystemFonts[i].fFileName = fontInfo[i].fFileName;
        gSystemFonts[i].fNames = fontInfo[i].fNames;
#if SK_DEBUG_FONTS
        SkDebugf("---- gSystemFonts[%d] fileName=%s", i, fontInfo[i].fFileName);
#endif
    }
    fontFamilies.deleteAll();
}

/*
 *  Called once (ensured by the sentinel check at the beginning of our body).
 *  Initializes all the globals, and register the system fonts.
 *
 *  gFamilyHeadAndNameListMutex must already be acquired.
 */
static void init_system_fonts() {
    // check if we've already been called
    if (gDefaultNormal) {
        return;
    }

    SkASSERT(gUniqueFontID == 0);

    load_font_info();

    FontInitRec* rec = gSystemFonts;
    SkTypeface* firstInFamily = NULL;
    int fallbackCount = 0;

    for (size_t i = 0; i < gNumSystemFonts; i++) {
        // if we're the first in a new family, clear firstInFamily
        if (rec[i].fNames != NULL) {
            firstInFamily = NULL;
        }

        bool isFixedPitch;
        SkString name;
        SkTypeface::Style style;

        // we expect all the fonts, except the "fallback" fonts
        bool isExpected = (rec[i].fNames != gFBNames);
        if (!get_name_and_style(rec[i].fFileName, &name, &style,
                                &isFixedPitch, isExpected)) {
            // We need to increase gUniqueFontID here so that the unique id of
            // each font matches its index in gSystemFonts array, as expected
            // by find_uniqueID.
            sk_atomic_inc(&gUniqueFontID);
            continue;
        }

        SkTypeface* tf = SkNEW_ARGS(FileTypeface,
                                    (style,
                                     true,  // system-font (cannot delete)
                                     firstInFamily, // what family to join
                                     rec[i].fFileName,
                                     isFixedPitch) // filename
                                    );
#if SK_DEBUG_FONTS
        SkDebugf("---- SkTypeface[%d] %s fontID %d",
                  i, rec[i].fFileName, tf->uniqueID());
#endif

        if (rec[i].fNames != NULL) {
            // see if this is one of our fallback fonts
            if (rec[i].fNames == gFBNames) {
#if SK_DEBUG_FONTS
                SkDebugf("---- adding %s as fallback[%d] fontID %d",
                          rec[i].fFileName, fallbackCount, tf->uniqueID());
#endif
                gFallbackFonts[fallbackCount++] = tf->uniqueID();

                // Use the font file name as the name of the typeface.
                const char **nameList = (const char**)malloc(2 * sizeof(char*));
                if (nameList == NULL) {
                    // shouldn't get here
                    SkDEBUGFAIL("Failed to allocate nameList");
                    break;
                }
                nameList[0] = rec[i].fFileName;
                nameList[1] = NULL;
                rec[i].fNames = nameList;
            }

            firstInFamily = tf;
            FamilyRec* family = find_family(tf);
            const char* const* names = rec[i].fNames;

            // record the default family if this is it
            if (names == gDefaultNames) {
                gDefaultFamily = family;
            }
            // add the names to map to this family
            while (*names) {
                add_name(*names, family);
                names += 1;
            }
        }
    }

    // do this after all fonts are loaded. This is our default font, and it
    // acts as a sentinel so we only execute load_system_fonts() once
    gDefaultNormal = find_best_face(gDefaultFamily, SkTypeface::kNormal);
    // now terminate our fallback list with the sentinel value
    gFallbackFonts[fallbackCount] = 0;

#if SK_DEBUG_FONTS
    SkDEBUGCODE(dump_globals());
#endif
}

static size_t find_uniqueID(const char* filename) {
    // uniqueID is the index, offset by one, of the associated element in
    // gSystemFonts[] (assumes system fonts are loaded before external fonts)
    // return 0 if not found
    const FontInitRec* rec = gSystemFonts;
    for (size_t i = 0; i < gNumSystemFonts; i++) {
        if (strcmp(rec[i].fFileName, filename) == 0) {
            return i+1;
        }
    }
    return 0;
}

static void reload_fallback_fonts() {
    if (gTestFallbackConfigFile) {
        // No need to reload fallback fonts in test environment.
        return;
    }

    SkGraphics::PurgeFontCache();

    SkTDArray<FontFamily*> fallbackFamilies;
    getFallbackFontFamilies(fallbackFamilies);

    int fallbackCount = 0;
    for (int i = 0; i < fallbackFamilies.count(); ++i) {
        FontFamily *family = fallbackFamilies[i];

        for (int j = 0; j < family->fFileNames.count(); ++j) {
            if (family->fFileNames[j]) {

                // ensure the fallback font exists before adding it to the list
                bool isFixedPitch;
                SkString name;
                SkTypeface::Style style;
                if (!get_name_and_style(family->fFileNames[j], &name, &style,
                                        &isFixedPitch, false)) {
                    continue;
                }

                size_t uniqueID = find_uniqueID(family->fFileNames[j]);
                SkASSERT(uniqueID != 0);
#if SK_DEBUG_FONTS
                SkDebugf("---- reload %s as fallback[%d] fontID %d oldFontID %d",
                          family->fFileNames[j], fallbackCount, uniqueID,
                          gFallbackFonts[fallbackCount]);
#endif
                gFallbackFonts[fallbackCount++] = uniqueID;
                break;  // The fallback set contains only the first font of each family
            }
        }
    }
    // reset the sentinel the end of the newly ordered array
    gFallbackFonts[fallbackCount] = 0;
}

static void load_system_fonts() {
    static AndroidLocale prevLocale;
    AndroidLocale locale;

    getLocale(locale);

    if (!gDefaultNormal) {
        prevLocale = locale;
        init_system_fonts();
    } else if (strncmp(locale.language, prevLocale.language, 2) ||
            strncmp(locale.region, prevLocale.region, 2)) {
        prevLocale = locale;
        reload_fallback_fonts();
    }
}

///////////////////////////////////////////////////////////////////////////////

void FamilyTypeface::onGetFontDescriptor(SkFontDescriptor* desc,
                                         bool* isLocalStream) const {
    {
        SkAutoMutexAcquire ac(gFamilyHeadAndNameListMutex);
        desc->setFamilyName(find_family_name(this));
        desc->setFontFileName(this->getUniqueString());
    }
    *isLocalStream = !this->isSysFont();
}

#if 0   // do we need this different name lookup for Deserialize?
SkTypeface* SkFontHost::Deserialize(SkStream* stream) {
    ...
    if (NULL != fontFileName && 0 != *fontFileName) {
        const FontInitRec* rec = gSystemFonts;
        for (size_t i = 0; i < gNumSystemFonts; i++) {
            if (strcmp(rec[i].fFileName, fontFileName) == 0) {
                // backup until we hit the fNames
                for (int j = i; j >= 0; --j) {
                    if (rec[j].fNames != NULL) {
                        return SkFontHost::CreateTypeface(NULL,
                                    rec[j].fNames[0], style);
                    }
                }
            }
        }
    }
    ...
}
#endif

///////////////////////////////////////////////////////////////////////////////

SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                       const char familyName[],
                                       SkTypeface::Style style) {
    SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);

    load_system_fonts();

    // clip to legal style bits
    style = (SkTypeface::Style)(style & SkTypeface::kBoldItalic);

    SkTypeface* tf = NULL;

    if (NULL != familyFace) {
        tf = find_typeface(familyFace, style);
    } else if (NULL != familyName) {
//        SkDebugf("======= familyName <%s>\n", familyName);
        tf = find_typeface(familyName, style);
    }

    if (NULL == tf) {
        tf = find_best_face(gDefaultFamily, style);
    }

    // we ref(), since the semantic is to return a new instance
    tf->ref();
    return tf;
}

SkTypeface* SkAndroidNextLogicalTypeface(SkFontID currFontID,
                                         SkFontID origFontID) {
#if defined(SK_BUILD_FOR_ANDROID) && !defined(SK_BUILD_FOR_ANDROID_FRAMEWORK)
    // Skia does not support font fallback for ndk applications in order to
    // enable clients such as WebKit to customize their font selection.
    // Clients can use GetFallbackFamilyNameForChar() to get the fallback
    // font for individual characters.
    return NULL;
#else
    SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);

    load_system_fonts();

    const SkTypeface* origTypeface = find_from_uniqueID(origFontID);
    const SkTypeface* currTypeface = find_from_uniqueID(currFontID);

    SkASSERT(origTypeface != 0);
    SkASSERT(currTypeface != 0);
    SkASSERT(gFallbackFonts);

    // Our fallback list always stores the id of the plain in each fallback
    // family, so we transform currFontID to its plain equivalent.
    currFontID = find_typeface(currTypeface, SkTypeface::kNormal)->uniqueID();

    /*  First see if fontID is already one of our fallbacks. If so, return
        its successor. If fontID is not in our list, then return the first one
        in our list. Note: list is zero-terminated, and returning zero means
        we have no more fonts to use for fallbacks.
     */
    const uint32_t* list = gFallbackFonts;
    for (int i = 0; list[i] != 0; i++) {
        if (list[i] == currFontID) {
            if (list[i+1] == 0)
                return NULL;
            const SkTypeface* nextTypeface = find_from_uniqueID(list[i+1]);
            return SkRef(find_typeface(nextTypeface, origTypeface->style()));
        }
    }

    // If we get here, currFontID was not a fallback, so we start at the
    // beginning of our list. Assuming there is at least one fallback font,
    // i.e. gFallbackFonts[0] != 0.
    const SkTypeface* firstTypeface = find_from_uniqueID(list[0]);
    return SkRef(find_typeface(firstTypeface, origTypeface->style()));
#endif
}

///////////////////////////////////////////////////////////////////////////////

SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream* stream) {
    if (NULL == stream || stream->getLength() <= 0) {
        return NULL;
    }

    bool isFixedPitch;
    SkTypeface::Style style;

    if (find_name_and_attributes(stream, NULL, &style, &isFixedPitch)) {
        SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);
        // Make sure system fonts are loaded to comply with the assumption of
        // unique id offset by one in find_uniqueID.
        load_system_fonts();
        return SkNEW_ARGS(StreamTypeface, (style, false, NULL, stream, isFixedPitch));
    } else {
        return NULL;
    }
}

SkTypeface* SkFontHost::CreateTypefaceFromFile(const char path[]) {
    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(path));
    return stream.get() ? SkFontHost::CreateTypefaceFromStream(stream) : NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Function from SkTypeface_android.h
///////////////////////////////////////////////////////////////////////////////

struct FBScriptInfo {
    const FallbackScripts fScript;
    const char* fScriptID;
    const SkTypeface::Style fStyle;
    const SkUnichar fChar; // representative character for that script type
    SkFontID fFontID;
};

#define SK_DEFINE_SCRIPT_ENTRY(script, style, unichar) \
    { script, #script, style, unichar, 0 }

static FBScriptInfo gFBScriptInfo[] = {
    SK_DEFINE_SCRIPT_ENTRY(kArabic_FallbackScript,        SkTypeface::kNormal, 0x0627),
    SK_DEFINE_SCRIPT_ENTRY(kArmenian_FallbackScript,      SkTypeface::kNormal, 0x0531),
    SK_DEFINE_SCRIPT_ENTRY(kBengali_FallbackScript,       SkTypeface::kNormal, 0x0981),
    SK_DEFINE_SCRIPT_ENTRY(kDevanagari_FallbackScript,    SkTypeface::kNormal, 0x0901),
#if defined(SBROWSER_FONT_SUPPORT)
    SK_DEFINE_SCRIPT_ENTRY(kGujarati_FallbackScript,      SkTypeface::kNormal, 0x0A81),
#endif /* SBROWSER_FONT_SUPPORT */
    SK_DEFINE_SCRIPT_ENTRY(kEthiopic_FallbackScript,      SkTypeface::kNormal, 0x1200),
    SK_DEFINE_SCRIPT_ENTRY(kGeorgian_FallbackScript,      SkTypeface::kNormal, 0x10A0),
    SK_DEFINE_SCRIPT_ENTRY(kHebrewRegular_FallbackScript, SkTypeface::kNormal, 0x0591),
    SK_DEFINE_SCRIPT_ENTRY(kHebrewBold_FallbackScript,    SkTypeface::kBold,   0x0591),
    SK_DEFINE_SCRIPT_ENTRY(kKannada_FallbackScript,       SkTypeface::kNormal, 0x0C90),
    SK_DEFINE_SCRIPT_ENTRY(kMalayalam_FallbackScript,     SkTypeface::kNormal, 0x0D10),
    SK_DEFINE_SCRIPT_ENTRY(kTamilRegular_FallbackScript,  SkTypeface::kNormal, 0x0B82),
    SK_DEFINE_SCRIPT_ENTRY(kTamilBold_FallbackScript,     SkTypeface::kBold,   0x0B82),
    SK_DEFINE_SCRIPT_ENTRY(kThai_FallbackScript,          SkTypeface::kNormal, 0x0E01),
    SK_DEFINE_SCRIPT_ENTRY(kTelugu_FallbackScript,        SkTypeface::kNormal, 0x0C10),
#if defined(SBROWSER_FONT_SUPPORT)
    SK_DEFINE_SCRIPT_ENTRY(kGurumukhi_FallbackScript,     SkTypeface::kNormal, 0x0A10),
    SK_DEFINE_SCRIPT_ENTRY(kSinhala_FallbackScript,       SkTypeface::kNormal, 0x0D90),
    SK_DEFINE_SCRIPT_ENTRY(kMyanmar_FallbackScript,       SkTypeface::kNormal, 0x1000),
    SK_DEFINE_SCRIPT_ENTRY(kLao_FallbackScript,       SkTypeface::kNormal, 0x0E81),
#endif /* SBROWSER_FONT_SUPPORT */
};

static bool gFBScriptInitialized = false;
static const int gFBScriptInfoCount = sizeof(gFBScriptInfo) / sizeof(FBScriptInfo);

// ensure that if any value is added to the public enum it is also added here
SK_COMPILE_ASSERT(gFBScriptInfoCount == kFallbackScriptNumber, FBScript_count_mismatch);

// this function can't be called if the gFamilyHeadAndNameListMutex is already locked
static bool typefaceContainsChar(SkTypeface* face, SkUnichar uni) {
    SkPaint paint;
    paint.setTypeface(face);
    paint.setTextEncoding(SkPaint::kUTF32_TextEncoding);

    uint16_t glyphID;
    paint.textToGlyphs(&uni, sizeof(uni), &glyphID);
    return glyphID != 0;
}

// this function can't be called if the gFamilyHeadAndNameListMutex is already locked
static SkTypeface* findFallbackTypefaceForChar(SkUnichar uni) {
    SkASSERT(gFallbackFonts);
    const uint32_t* list = gFallbackFonts;
    for (int i = 0; list[i] != 0; i++) {
        SkTypeface* face;
        {
            SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);
            face = find_from_uniqueID(list[i]);
        }
        if (typefaceContainsChar(face, uni)) {
            return face;
        }
    }
    return 0;
}

// this function can't be called if the gFamilyHeadAndNameListMutex is already locked
static SkFontID findFallbackFontIDForChar(SkUnichar uni, SkTypeface::Style style) {
    const SkTypeface* tf = findFallbackTypefaceForChar(uni);
    if (!tf) {
        return 0;
    }
    return find_typeface(tf, style)->uniqueID();
}

// this function can't be called if the gFamilyHeadAndNameListMutex is already locked
static void initFBScriptInfo() {
    if (gFBScriptInitialized) {
        return;
    }

    // ensure the system fonts are loaded
    gFamilyHeadAndNameListMutex.acquire();
    load_system_fonts();
    gFamilyHeadAndNameListMutex.release();

    for (int i = 0; i < gFBScriptInfoCount; i++) {
        FBScriptInfo& scriptInfo = gFBScriptInfo[i];
        // selects the best available style for the desired font. However, if
        // bold is requested and no bold font exists for the typeface containing
        // the character the next best style is chosen (e.g. normal).
        scriptInfo.fFontID = findFallbackFontIDForChar(scriptInfo.fChar, scriptInfo.fStyle);
#if SK_DEBUG_FONTS
        SkDebugf("gFBScriptInfo[%s] --> %d", scriptInfo.fScriptID, scriptInfo.fFontID);
#endif
    }
    // mark the value as initialized so we don't repeat our work unnecessarily
    gFBScriptInitialized = true;
}

SkTypeface* SkCreateTypefaceForScript(FallbackScripts script) {
    if (!SkTypeface_ValidScript(script)) {
        return NULL;
    }

    // ensure that our table is populated
    initFBScriptInfo();

    FBScriptInfo& scriptInfo = gFBScriptInfo[script];

    // ensure the element with that index actually maps to the correct script
    SkASSERT(scriptInfo.fScript == script);

    // if a suitable script could not be found then return NULL
    if (scriptInfo.fFontID == 0) {
        return NULL;
    }

    SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);

    // retrieve the typeface the corresponds to this fontID
    SkTypeface* tf = find_from_uniqueID(scriptInfo.fFontID);
    // we ref(), since the semantic is to return a new instance
    tf->ref();
    return tf;
}

const char* SkGetFallbackScriptID(FallbackScripts script) {
    for (int i = 0; i < gFBScriptInfoCount; i++) {
        if (gFBScriptInfo[i].fScript == script) {
            return gFBScriptInfo[i].fScriptID;
        }
    }
    return NULL;
}

FallbackScripts SkGetFallbackScriptFromID(const char* id) {
    for (int i = 0; i < gFBScriptInfoCount; i++) {
        if (strcmp(gFBScriptInfo[i].fScriptID, id) == 0) {
            return gFBScriptInfo[i].fScript;
        }
    }
    return kFallbackScriptNumber; // Use kFallbackScriptNumber as an invalid value.
}

SkTypeface* SkCreateFallbackTypefaceForChar(SkUnichar uni,
                                            SkTypeface::Style style) {
    {
        SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);
        load_system_fonts();
    }

    SkTypeface* tf = findFallbackTypefaceForChar(uni);
    if (!tf) {
        return NULL;
    }
    SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);
    tf = find_typeface(tf, style);
    // we ref(), since the semantic is to return a new instance
    tf->ref();
    return tf;
}

bool SkGetFallbackFamilyNameForChar(SkUnichar uni, SkString* name) {
    SkASSERT(name);
    {
        SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);
        load_system_fonts();
    }

    const SkTypeface* tf = findFallbackTypefaceForChar(uni);
    if (!tf) {
        return false;
    }
    SkAutoMutexAcquire  ac(gFamilyHeadAndNameListMutex);
    name->set(find_family_name(tf));
    return true;
}

void SkUseTestFontConfigFile(const char* mainconf, const char* fallbackconf,
                             const char* fontsdir) {
    gTestMainConfigFile = mainconf;
    gTestFallbackConfigFile = fallbackconf;
    gTestFontFilePrefix = fontsdir;
    SkASSERT(gTestMainConfigFile);
    SkASSERT(gTestFallbackConfigFile);
    SkASSERT(gTestFontFilePrefix);
    SkDEBUGF(("Use Test Config File Main %s, Fallback %s, Font Dir %s",
              gTestMainConfigFile, gTestFallbackConfigFile, gTestFontFilePrefix));
}

///////////////////////////////////////////////////////////////////////////////

#include "SkFontMgr.h"

SkFontMgr* SkFontMgr::Factory() {
    // todo
    return NULL;
}
