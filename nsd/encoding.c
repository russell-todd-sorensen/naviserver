/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://mozilla.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is AOLserver Code and related documentation
 * distributed by AOL.
 *
 * The Initial Developer of the Original Code is America Online,
 * Inc. Portions created by AOL are Copyright (C) 1999 America Online,
 * Inc. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under either the License or the GPL.
 */

/*
 * encoding.c --
 *
 *      Defines standard default charset to encoding mappings.
 */

#include "nsd.h"

/*
 * Local functions defined in this file.
 */

static void AddCharset(const char *charset, const char *name)
    NS_GNUC_NONNULL(1) NS_GNUC_NONNULL(2);

static void AddExtension(const char *ext, const char *name)
    NS_GNUC_NONNULL(1) NS_GNUC_NONNULL(2);

static Tcl_Encoding LoadEncoding(const char *name)
    NS_GNUC_NONNULL(1);

static Ns_ServerInitProc ConfigServerEncodings;

/*
 * Local variables defined in this file.
 */

static Tcl_HashTable  extensions;   /* Maps file extensions to charsets. */
static Tcl_HashTable  charsets;     /* Maps Internet charset names to Tcl encoding names */
static Tcl_HashTable  encnames;     /* Maps Tcl encoding names to Internet charset names. */
static Tcl_HashTable  encodings;    /* Cache of loaded Tcl encodings */

static Ns_Mutex       lock;         /* Lock around encodings. */
static Ns_Cond        cond;

Tcl_Encoding          NS_utf8Encoding = NULL; /* Cached pointer to utf-8 encoding. */

#define EncodingLocked ((Tcl_Encoding) (-1))

/*
 * The default table maps file extensions to Tcl encodings.
 * That is, the encoding used to read the files from disk (mainly ADP).
 */

static const struct {
    const char  *extension;
    const char  *name;
} builtinExt[] = {
    {".txt",    "ascii"},
    {".htm",    "utf-8"},
    {".html",   "utf-8"},
    {".adp",    "utf-8"},
    {NULL, NULL}
};

/*
 * The following table provides HTTP charset aliases for Tcl encodings names.
 */

static const struct {
    const char  *charset;
    const char  *name;
} builtinChar[] = {
    { "iso-2022-jp",        "iso2022-jp" },
    { "iso-2022-kr",        "iso2022-kr" },
    { "iso-8859-1",         "iso8859-1" },
    { "iso-8859-2",         "iso8859-2" },
    { "iso-8859-3",         "iso8859-3" },
    { "iso-8859-4",         "iso8859-4" },
    { "iso-8859-5",         "iso8859-5" },
    { "iso-8859-6",         "iso8859-6" },
    { "iso-8859-7",         "iso8859-7" },
    { "iso-8859-8",         "iso8859-8" },
    { "iso-8859-9",         "iso8859-9" },
    { "korean",             "ksc5601" },
    { "ksc_5601",           "ksc5601" },
    { "mac",                "macRoman" },
    { "mac-centeuro",       "macCentEuro" },
    { "mac-centraleupore",  "macCentEuro" },
    { "mac-croatian",       "macCroatian" },
    { "mac-cyrillic",       "macCyrillic" },
    { "mac-greek",          "macGreek" },
    { "mac-iceland",        "macIceland" },
    { "mac-japan",          "macJapan" },
    { "mac-roman",          "macRoman" },
    { "mac-romania",        "macRomania" },
    { "mac-thai",           "macThai" },
    { "mac-turkish",        "macTurkish" },
    { "mac-ukraine",        "macUkraine" },
    { "maccenteuro",        "macCentEuro" },
    { "maccentraleupore",   "macCentEuro" },
    { "maccroatian",        "macCroatian" },
    { "maccyrillic",        "macCyrillic" },
    { "macgreek",           "macGreek" },
    { "maciceland",         "macIceland" },
    { "macintosh",          "macRoman" },
    { "macjapan",           "macJapan" },
    { "macroman",           "macRoman" },
    { "macromania",         "macRomania" },
    { "macthai",            "macThai" },
    { "macturkish",         "macTurkish" },
    { "macukraine",         "macUkraine" },
    { "shift_jis",          "shiftjis" },
    { "us-ascii",           "ascii" },
    { "windows-1250",       "cp1250" },
    { "windows-1251",       "cp1251" },
    { "windows-1252",       "cp1252" },
    { "windows-1253",       "cp1253" },
    { "windows-1254",       "cp1254" },
    { "windows-1255",       "cp1255" },
    { "windows-1256",       "cp1256" },
    { "windows-1257",       "cp1257" },
    { "windows-1258",       "cp1258" },
    { "x-mac",              "macRoman" },
    { "x-mac-centeuro",      "macCentEuro" },
    { "x-mac-centraleupore", "macCentEuro" },
    { "x-mac-croatian",      "macCroatian" },
    { "x-mac-cyrillic",      "macCyrillic" },
    { "x-mac-greek",         "macGreek" },
    { "x-mac-iceland",       "macIceland" },
    { "x-mac-japan",         "macJapan" },
    { "x-mac-roman",         "macRoman" },
    { "x-mac-romania",       "macRomania" },
    { "x-mac-thai",          "macThai" },
    { "x-mac-turkish",       "macTurkish" },
    { "x-mac-ukraine",       "macUkraine" },
    { "x-macintosh",         "macRoman" },
    { NULL, NULL }
};


/*
 *----------------------------------------------------------------------
 *
 * NsConfigEncodings --
 *
 *      Configure charset aliases and file extension mappings.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
NsConfigEncodings(void)
{
    const Ns_Set *set;
    size_t        i;

    Ns_MutexSetName(&lock, "ns:encodings");
    Tcl_InitHashTable(&extensions, TCL_STRING_KEYS);
    Tcl_InitHashTable(&charsets, TCL_STRING_KEYS);
    Tcl_InitHashTable(&encnames, TCL_STRING_KEYS);
    Tcl_InitHashTable(&encodings, TCL_STRING_KEYS);
    NS_utf8Encoding = Ns_GetCharsetEncoding("utf-8");

    /*
     * Add default charsets and file mappings.
     */

    for (i = 0u; builtinChar[i].charset != NULL; ++i) {
        AddCharset(builtinChar[i].charset, builtinChar[i].name);
    }
    for (i = 0u; builtinExt[i].extension != NULL; ++i) {
        AddExtension(builtinExt[i].extension, builtinExt[i].name);
    }

    /*
     * Add configured charsets and file mappings.
     */

    set = Ns_ConfigGetSection("ns/charsets");
    for (i = 0u; set != NULL && i < Ns_SetSize(set); ++i) {
        AddCharset(Ns_SetKey(set, i), Ns_SetValue(set, i));
    }
    set = Ns_ConfigGetSection("ns/encodings");
    for (i = 0u; set != NULL && i < Ns_SetSize(set); ++i) {
        AddExtension(Ns_SetKey(set, i), Ns_SetValue(set, i));
    }

    NsRegisterServerInit(ConfigServerEncodings);
}

static Ns_ReturnCode
ConfigServerEncodings(const char *server)
{
    NsServer     *servPtr = NsGetServer(server);
    Ns_ReturnCode result;

    if (unlikely(servPtr == NULL)) {
        Ns_Log(Warning, "Could not set encoding, server '%s' unknown", server);
        result = NS_ERROR;

    } else {
        const char *path;

        /*
         * Configure the encoding used in the request URL.
         */

        path = Ns_ConfigSectionPath(NULL, server, NULL, (char *)0L);

        servPtr->encoding.urlCharset =
            Ns_ConfigString(path, "urlCharset", "utf-8");

        servPtr->encoding.urlEncoding =
            Ns_GetCharsetEncoding(servPtr->encoding.urlCharset);
        if (servPtr->encoding.urlEncoding == NULL) {
            Ns_Log(Warning, "no encoding found for charset \"%s\" from config",
                   servPtr->encoding.urlCharset);
        }

        /*
         * Configure the encoding used for Tcl/ADP output.
         */

        servPtr->encoding.outputCharset =
            Ns_ConfigString(path, "outputCharset", "utf-8");

        servPtr->encoding.outputEncoding =
            Ns_GetCharsetEncoding(servPtr->encoding.outputCharset);
        if (servPtr->encoding.outputEncoding == NULL) {
            Ns_Fatal("could not find encoding for default output charset \"%s\"",
                     servPtr->encoding.outputCharset);
        }

        /*
         * Force charset into content-type header for dynamic responses.
         */

        servPtr->encoding.hackContentTypeP =
            Ns_ConfigBool(path, "HackContentType", NS_TRUE);

        result = NS_OK;

    }
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_GetFileEncoding --
 *
 *      Return the Tcl_Encoding that should be used to read a file from disk
 *      according to its extension.
 *
 *      Note this may not be the same as the encoding for the charset of the
 *      file's mimetype.
 *
 * Results:
 *      Tcl_Encoding or NULL if not found.
 *
 * Side effects:
 *      See Ns_GetCharsetEncoding().
 *
 *----------------------------------------------------------------------
 */

Tcl_Encoding
Ns_GetFileEncoding(const char *file)
{
    const char    *ext;
    Tcl_Encoding   encoding = NULL;

    NS_NONNULL_ASSERT(file != NULL);

    ext = strrchr(file, INTCHAR('.'));
    if (ext != NULL) {
        const Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&extensions, ext);

        if (hPtr != NULL) {
            const char *name = Tcl_GetHashValue(hPtr);
            encoding = Ns_GetCharsetEncoding(name);
        }
    }
    return encoding;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_GetTypeEncoding --
 *
 *      Return the Tcl_Encoding for the given Content-type header,
 *      e.g., "text/html; charset=iso-8859-1" returns Tcl_Encoding
 *      for iso8859-1.
 *
 *      This function will utilize the ns/parameters/OutputCharset
 *      config parameter if given a content-type "text/<anything>" with
 *      no charset.
 *
 *      When no OutputCharset defined, the fall-back behavior is to
 *      return NULL.
 *
 * Results:
 *      Tcl_Encoding or NULL if not found.
 *
 * Side effects:
 *      See LoadEncoding().
 *
 *----------------------------------------------------------------------
 */

Tcl_Encoding
Ns_GetTypeEncoding(const char *mimeType)
{
    const char *charset;
    size_t      len;

    NS_NONNULL_ASSERT(mimeType != NULL);

    charset = NsFindCharset(mimeType, &len);
    return (charset != NULL) ? Ns_GetCharsetEncodingEx(charset, (int)len) : NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_GetCharsetEncoding, Ns_GetCharsetEncodingEx --
 *
 *      Return the Tcl_Encoding for the given charset, e.g.,
 *      "iso-8859-1" returns Tcl_Encoding for iso8859-1.
 *
 * Results:
 *      Tcl_Encoding or NULL if not found.
 *
 * Side effects:
 *      See LoadEncoding().
 *
 *----------------------------------------------------------------------
 */

Tcl_Encoding
Ns_GetCharsetEncoding(const char *charset)
{
    NS_NONNULL_ASSERT(charset != NULL);

    return Ns_GetCharsetEncodingEx(charset, -1);
}

Tcl_Encoding
Ns_GetCharsetEncodingEx(const char *charset, int len)
{
    const Tcl_HashEntry *hPtr;
    Tcl_Encoding         encoding;
    Ns_DString           ds;

    NS_NONNULL_ASSERT(charset != NULL);

    /*
     * Cleanup the charset name and check for an
     * alias (e.g., iso-8859-1 = iso8859-1) before
     * assuming the charset and Tcl encoding names
     * match (e.g., big5).
     */

    Ns_DStringInit(&ds);
    Ns_DStringNAppend(&ds, charset, len);
    charset = Ns_StrTrim(Ns_StrToLower(ds.string));
    hPtr = Tcl_FindHashEntry(&charsets, charset);
    if (hPtr != NULL) {
        charset = Tcl_GetHashValue(hPtr);
    }
    encoding = LoadEncoding(charset);
    Ns_DStringFree(&ds);

    return encoding;
}

Tcl_Encoding
Ns_GetEncoding(const char *name)
{
    /* Deprecated, use Ns_GetCharsetEncodingEx(). */
    return LoadEncoding(name);
}


/*
 *----------------------------------------------------------------------
 *
 * Ns_GetEncodingCharset --
 *
 *      Return the charset name for the given Tcl_Encoding.
 *
 * Results:
 *      Charset name, or encoding name if no alias.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
Ns_GetEncodingCharset(Tcl_Encoding encoding)
{
    const char          *encname, *charset = NULL;
    const Tcl_HashEntry *hPtr;

    NS_NONNULL_ASSERT(encoding != NULL);

    encname = Tcl_GetEncodingName(encoding);
    hPtr = Tcl_FindHashEntry(&encnames, encname);
    if (hPtr != NULL) {
        charset = Tcl_GetHashValue(hPtr);
    }
    return (charset != NULL) ? charset : encname;
}


/*
 *----------------------------------------------------------------------
 *
 * NsFindCharset --
 *
 *      Find start of charset within a mime-type string.
 *
 * Results:
 *      Pointer to start of charset or NULL on no charset.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
NsFindCharset(const char *mimetype, size_t *lenPtr)
{
    const char *start, *charset = NULL;

    NS_NONNULL_ASSERT(mimetype != NULL);
    NS_NONNULL_ASSERT(lenPtr != NULL);

    start = Ns_StrCaseFind(mimetype, "charset");
    if (start != NULL) {
        start += 7;
        start += strspn(start, " ");
        if (*start++ == '=') {
            const char *end;

            start += strspn(start, " ");
            end = start;
            while (*end != '\0' && CHARTYPE(space, *end) == 0) {
                ++end;
            }
            *lenPtr = (size_t)(end - start);
            charset = start;
        }
    }
    return charset;
}


/*
 *----------------------------------------------------------------------
 *
 * NsTclCharsetsObjCmd --
 *
 *      Implements "ns_charsets". The command returns the list of charsets for
 *      which encodings are defined.
 *
 * Results:
 *      TCL_OK
 *
 * Side effects:
 *      Sets Tcl interpreter result.
 *
 *----------------------------------------------------------------------
 */

int
NsTclCharsetsObjCmd(ClientData UNUSED(clientData), Tcl_Interp *interp,
                    int UNUSED(objc), Tcl_Obj *const* UNUSED(objv))
{
    const Tcl_HashEntry *hPtr;
    Tcl_HashSearch       search;
    Tcl_Obj             *listObj = Tcl_NewListObj(0, NULL);

    for (hPtr = Tcl_FirstHashEntry(&charsets, &search);
         hPtr != NULL;
         hPtr = Tcl_NextHashEntry(&search)
         ) {
        const char *key = Tcl_GetHashKey(&charsets, hPtr);
        Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(key, -1));
    }
    Tcl_SetObjResult(interp, listObj);

    return TCL_OK;
}



/*
 *----------------------------------------------------------------------
 *
 * NsTclEncodingForCharsetObjCmd --
 *
 *      Implements "ns_encodingforcharset". The command returns the name of
 *      the encoding for the specified charset.
 *
 * Results:
 *      Tcl result contains an encoding name or "".
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
NsTclEncodingForCharsetObjCmd(ClientData UNUSED(clientData), Tcl_Interp *interp, int objc, Tcl_Obj *const* objv)
{
    int result = TCL_OK;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "charset");
        result =  TCL_ERROR;
    } else {
        int          encodingNameLen;
        const char  *encodingName = Tcl_GetStringFromObj(objv[1], &encodingNameLen);
        Tcl_Encoding encoding = Ns_GetCharsetEncodingEx(encodingName, encodingNameLen);

        if (encoding != NULL) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_GetEncodingName(encoding), -1));
        }
    }

    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * NsEncodingIsUtf8 --
 *
 *      Is the given encoding the utf-8 encoding?
 *
 * Results:
 *      Boolean.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

bool
NsEncodingIsUtf8(const Tcl_Encoding encoding)
{
    return (encoding == NS_utf8Encoding);
}


/*
 *----------------------------------------------------------------------
 *
 * LoadEncoding --
 *
 *      Return the Tcl_Encoding for the given charset.
 *
 * Results:
 *      Tcl_Encoding or NULL if not found.
 *
 * Side effects:
 *      Will load encoding from disk on first access.
 *      May wait for other thread to load encoding from disk.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Encoding
LoadEncoding(const char *name)
{
    Tcl_HashEntry *hPtr;
    Tcl_Encoding   encoding;
    int            isNew;

    NS_NONNULL_ASSERT(name != NULL);

    Ns_MutexLock(&lock);
    hPtr = Tcl_CreateHashEntry(&encodings, name, &isNew);
    if (isNew == 0) {
        while ((encoding = Tcl_GetHashValue(hPtr)) == EncodingLocked) {
            Ns_CondWait(&cond, &lock);
        }
    } else {
        Tcl_SetHashValue(hPtr, INT2PTR(EncodingLocked));
        Ns_MutexUnlock(&lock);
        encoding = Tcl_GetEncoding(NULL, name);
        if (encoding == NULL) {
            Ns_Log(Warning, "encoding: could not load: '%s'", name);
        } else {
            Ns_Log(Debug, "encoding: loaded: %s", name);
        }
        Ns_MutexLock(&lock);
        Tcl_SetHashValue(hPtr, encoding);
        Ns_CondBroadcast(&cond);
    }
    Ns_MutexUnlock(&lock);

    return encoding;
}


/*
 *----------------------------------------------------------------------
 *
 * AddCharset, AddExtension --
 *
 *      Add extensiont to encoding mapping and charset aliases.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
AddExtension(const char *ext, const char *name)
{
    Tcl_HashEntry  *hPtr;
    int             isNew;

    NS_NONNULL_ASSERT(ext != NULL);
    NS_NONNULL_ASSERT(name != NULL);

    hPtr = Tcl_CreateHashEntry(&extensions, ext, &isNew);
    Tcl_SetHashValue(hPtr, name);
}

static void
AddCharset(const char *charset, const char *name)
{
    Tcl_HashEntry  *hPtr;
    Ns_DString      ds;
    int             isNew;

    NS_NONNULL_ASSERT(charset != NULL);
    NS_NONNULL_ASSERT(name != NULL);

    Ns_DStringInit(&ds);
    charset = Ns_StrToLower(Ns_DStringAppend(&ds, charset));

    /*
     * Map in the forward direction: charsets to encodings.
     */

    hPtr = Tcl_CreateHashEntry(&charsets, charset, &isNew);
    Tcl_SetHashValue(hPtr, name);

    /*
     * Map in the reverse direction: encodings to charsets.
     * Nb: Ignore duplicate mappings.
     */

    hPtr = Tcl_CreateHashEntry(&encnames, name, &isNew);
    if (isNew != 0) {
        Tcl_SetHashValue(hPtr, ns_strdup(charset));
    }

    Ns_DStringFree(&ds);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * indent-tabs-mode: nil
 * End:
 */
