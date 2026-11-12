/*
 * Synthetic 4D engine boundary for ownership regressions.
 *
 * Links the real plugin sources (4DPlugin-ExifTool.cpp,
 * support/4DPlugin-JSON.cpp, support/TagInfo.cpp) and the bundled,
 * unmodified 4D Plugin API (4DPluginAPI.c). Only gCall4D is replaced: it
 * models engine-side allocations (Unistrings, pictures, objects,
 * collections, handles) with malloc/free so ASan reports double frees and
 * use-after-free, and keeps a live set of every engine resource so leaks
 * are detected exactly.
 *
 * Engine semantics modelled (SDK contract; 4DPluginAPI.c documents
 * PA_GetObjectProperty with "The PA_Variable should be cleared after use",
 * and PA_GetCollectionElement uses the same EX_GET_OBJ_VALUE selector):
 *  - PA_SetObjectProperty / PA_SetCollectionElement deep-copy strings and
 *    pictures and retain objects/collections; the caller still owns its
 *    variable.
 *  - PA_GetObjectProperty / PA_GetCollectionElement return an owned copy
 *    (strings and pictures duplicated, objects/collections retained) that
 *    must be cleared; on error the result is not initialised (poisoned).
 *  - PA_ClearVariable releases what the variable owns.
 *  - PICTURE TO BLOB (692) reads its picture argument (not consumed) and
 *    fills the blob out-parameter. If the out-parameter already holds a
 *    handle (PA_CreateVariable(eVK_Blob) allocates an empty one), the bytes
 *    are written into that handle in place, so the plugin's PA_ClearVariable
 *    releases it. How the real engine treats a pre-existing out-parameter
 *    value (for PICTURE TO BLOB or BLOB TO PICTURE) is not known, and these
 *    tests make no claim about it.
 *
 * The exiftool process is never started: the plugin locates it through
 * [NSBundle bundleWithIdentifier:@"com.4D.ExifTool"], which is nil in this
 * executable, so runExifTool fails before any NSTask exists. Every command
 * test asserts that error message.
 */

#include "4DPluginAPI.h"
#include "PrivateTypes.h"
#include "EntryPoints.h"
#include "4DPlugin-JSON.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <functional>

extern "C" Call4DProcPtr gCall4D;

namespace {

typedef std::basic_string<PA_Unichar> u16;

struct Obj {
    int rc;
    bool isCollection;
    std::map<u16, PA_Variable> props;
    std::vector<PA_Variable> items;
};

struct Handle { char *data; PA_long32 len; };

std::set<void *> g_strings;
std::set<void *> g_pictures;
std::set<Obj *> g_objects;
std::set<Handle *> g_handles;
bool g_fail_get = false;
int g_get_calls = 0;
/* Stands in for the uninitialised result of a failed get: readable (so the
   test fails by assertion, not by crashing) but never engine-owned, so
   clearing it is reported as a violation. */
PA_Unichar kPoisonBuf[] = { 'B', 'A', 'D', '!', 0 };
PA_Unichar *const kPoison = kPoisonBuf;

[[noreturn]] void die(const char *msg) {
    fprintf(stderr, "ENGINE VIOLATION: %s\n", msg);
    fflush(stderr);
    abort();
}

PA_Unistring new_string(const PA_Unichar *src, size_t len) {
    PA_Unichar *buf = (PA_Unichar *)malloc((len + 1) * sizeof(PA_Unichar));
    if(len) memcpy(buf, src, len * sizeof(PA_Unichar));
    buf[len] = 0;
    g_strings.insert(buf);
    PA_Unistring s;
    s.fLength = (PA_long32)len;
    s.fString = buf;
    s.fReserved1 = 0;
    s.fReserved2 = 0;
    return s;
}

void free_string(PA_Unistring *s) {
    if(!s->fString) return;
    if(s->fString == kPoison) die("disposed a string from a failed get");
    if(!g_strings.erase(s->fString)) die("disposed a string the engine does not own");
    free(s->fString);
    s->fString = 0;
    s->fLength = 0;
}

void *new_picture() {
    void *p = malloc(16);
    g_pictures.insert(p);
    return p;
}

void free_picture(void *p) {
    if(!p) return;
    if(!g_pictures.erase(p)) die("disposed a picture the engine does not own");
    free(p);
}

Handle *new_handle(const void *data, PA_long32 len) {
    Handle *h = (Handle *)malloc(sizeof(Handle));
    h->data = (char *)malloc(len ? len : 1);
    if(data && len) memcpy(h->data, data, len);
    h->len = len;
    g_handles.insert(h);
    return h;
}

void free_handle(Handle *h) {
    if(!h) return;
    if(!g_handles.erase(h)) die("disposed an unknown handle");
    free(h->data);
    free(h);
}

void release_object(Obj *o);

void clear_var(PA_Variable *v) {
    switch(v->fType) {
        case eVK_Unistring: free_string(&v->uValue.fString); break;
        case eVK_Picture: free_picture(v->uValue.fPicture); break;
        case eVK_Blob: free_handle((Handle *)v->uValue.fBlob.fHandle); break;
        case eVK_Object: if(v->uValue.fObject) release_object((Obj *)v->uValue.fObject); break;
        case eVK_Collection: if(v->uValue.fCollection) release_object((Obj *)v->uValue.fCollection); break;
        default: break;
    }
    v->fType = eVK_Undefined;
}

PA_Variable copy_var(const PA_Variable &src) {
    PA_Variable dst = src;
    switch(src.fType) {
        case eVK_Unistring:
            dst.uValue.fString = new_string(src.uValue.fString.fString, src.uValue.fString.fLength);
            break;
        case eVK_Picture:
        {   /* PA_Variable is packed: copy fields before binding references */
            void *pic = src.uValue.fPicture;
            if(pic && !g_pictures.count(pic)) die("copied a dead picture");
        }
            dst.uValue.fPicture = new_picture();
            break;
        case eVK_Object:
        case eVK_Collection: {
            Obj *o = (Obj *)(src.fType == eVK_Object ? (void *)src.uValue.fObject : (void *)src.uValue.fCollection);
            if(o) {
                if(!g_objects.count(o)) die("copied a dead object");
                o->rc++;
            }
            break;
        }
        default: break;
    }
    return dst;
}

Obj *new_object(bool isCollection = false) {
    Obj *o = new Obj();
    o->rc = 1;
    o->isCollection = isCollection;
    g_objects.insert(o);
    return o;
}

void release_object(Obj *o) {
    if(!g_objects.count(o)) die("released a dead object");
    if(--o->rc == 0) {
        for(auto &kv : o->props) clear_var(&kv.second);
        for(auto &v : o->items) clear_var(&v);
        g_objects.erase(o);
        delete o;
    }
}

u16 key_of(const PA_Unistring &k) { return u16(k.fString, k.fLength); }

PA_Variable undefined_var() {
    PA_Variable v;
    memset(&v, 0, sizeof(v));
    v.fType = eVK_Undefined;
    return v;
}

void poison(PA_Variable *out) {
    memset(out, 0, sizeof(*out));
    out->fType = eVK_Unistring;
    out->uValue.fString.fString = kPoison;
    out->uValue.fString.fLength = 4;
}

void engine(short selector, EngineBlock *eb) {
    switch(selector) {
        case EX_GET_INFORMATION:
            eb->fParam2 = 0x00001900; /* PA_CreateObject takes the New object path */
            break;
        case EX_YIELD_ABSOLUTE:
            break;
        case EX_CREATE_UNISTRING: {
            const PA_Unichar *src = (const PA_Unichar *)eb->fHandle;
            size_t len = 0;
            while(src[len]) len++;
            eb->fUniString1 = new_string(src, len);
            break;
        }
        case EX_DISPOSE_UNISTRING:
            free_string(&eb->fUniString1);
            break;
        case EX_VARIABLE_TO_STRING: {
            PA_Variable *v = (PA_Variable *)eb->fHandle;
            if(v->fType == eVK_Unistring) eb->fUniString1 = v->uValue.fString; /* borrowed */
            break;
        }
        case EX_CREATE_PICTURE:
            eb->fPicture = new_picture();
            break;
        case EX_DISPOSE_PICTURE:
            free_picture(eb->fPicture);
            break;
        case EX_CLEAR_VARIABLE:
            clear_var((PA_Variable *)eb->fPtr1);
            break;
        case EX_SET_OBJ_VALUE: {
            Obj *o = (Obj *)eb->fPtr1;
            if(!g_objects.count(o)) die("set on a dead object");
            PA_Variable *in = (PA_Variable *)eb->fPtr2;
            if(eb->fShort == eVK_Collection) {
                if(!o->isCollection) die("collection set on an object");
                size_t i = (size_t)eb->fLongint;
                if(i >= o->items.size()) o->items.resize(i + 1, undefined_var());
                clear_var(&o->items[i]);
                o->items[i] = copy_var(*in);
            } else {
                if(o->isCollection) die("property set on a collection");
                u16 k = key_of(eb->fUniString1);
                auto it = o->props.find(k);
                if(it != o->props.end()) clear_var(&it->second);
                o->props[k] = copy_var(*in);
            }
            break;
        }
        case EX_GET_OBJ_VALUE: {
            Obj *o = (Obj *)eb->fPtr1;
            PA_Variable *out = (PA_Variable *)eb->fPtr2;
            if(!g_objects.count(o)) die("get on a dead object");
            if(eb->fShort == eVK_Collection) {
                if(!o->isCollection) die("collection get on an object");
                if(eb->fLongint == -1) { /* PA_GetCollectionLength */
                    memset(out, 0, sizeof(*out));
                    out->fType = eVK_Longint;
                    out->uValue.fLongint = (PA_long32)o->items.size();
                    break;
                }
                g_get_calls++;
                if(g_fail_get) { poison(out); eb->fError = -1; break; }
                size_t i = (size_t)eb->fLongint;
                *out = i < o->items.size() ? copy_var(o->items[i]) : undefined_var();
            } else {
                if(o->isCollection) die("property get on a collection");
                g_get_calls++;
                if(g_fail_get) { poison(out); eb->fError = -1; break; }
                auto it = o->props.find(key_of(eb->fUniString1));
                *out = it == o->props.end() ? undefined_var() : copy_var(it->second);
            }
            break;
        }
        case EX_EXECUTE_COMMAND_BY_ID: {
            PA_Variable **params = (PA_Variable **)eb->fPtr1;
            PA_Variable *ret = (PA_Variable *)eb->fPtr2;
            switch(eb->fParam1) {
                case 1231: { /* OB Is defined */
                    Obj *o = (Obj *)params[0]->uValue.fObject;
                    if(!g_objects.count(o)) die("OB Is defined on a dead object");
                    ret->fType = eVK_Boolean;
                    PA_Unistring k = params[1]->uValue.fString;
                    ret->uValue.fBoolean = o->props.count(key_of(k)) ? 1 : 0;
                    break;
                }
                case 1471: /* New object */
                    ret->fType = eVK_Object;
                    ret->uValue.fObject = (PA_ObjectRef)new_object();
                    break;
                case 1472: /* New collection */
                    ret->fType = eVK_Collection;
                    ret->uValue.fCollection = (PA_CollectionRef)new_object(true);
                    break;
                case 692: { /* PICTURE TO BLOB(picture; blob; codec) */
                    if(eb->fParam2 != 3) die("PICTURE TO BLOB arity");
                    void *pic = params[0]->uValue.fPicture;
                    if(params[0]->fType != eVK_Picture || !g_pictures.count(pic))
                        die("PICTURE TO BLOB on a picture the engine does not own");
                    static const char kBytes[] = "synthetic-picture-bytes";
                    const PA_long32 n = sizeof(kBytes) - 1;
                    Handle *h = params[1]->fType == eVK_Blob ? (Handle *)params[1]->uValue.fBlob.fHandle : 0;
                    if(h) {
                        if(!g_handles.count(h)) die("PICTURE TO BLOB into an unknown handle");
                        h->data = (char *)realloc(h->data, n);
                        memcpy(h->data, kBytes, n);
                        h->len = n;
                    } else {
                        clear_var(params[1]);
                        h = new_handle(kBytes, n);
                    }
                    params[1]->fType = eVK_Blob;
                    params[1]->uValue.fBlob.fHandle = (PA_Handle)h;
                    params[1]->uValue.fBlob.fSize = n;
                    break;
                }
                default: {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "unexpected PA_ExecuteCommandByID %d", (int)eb->fParam1);
                    die(msg);
                }
            }
            break;
        }
        case EX_CONVERT_STRING: {
            if(eb->fParam1 == 4) { /* ToUserData: length (dest NULL) or copy */
                Handle *h = (Handle *)eb->fHandle;
                if(!g_handles.count(h)) die("read an unknown handle");
                if(eb->fParam3) memcpy((void *)eb->fParam3, h->data, h->len);
                eb->fParam4 = h->len;
            } else if(eb->fParam1 == 5) { /* FromUserData */
                eb->fHandle = (PA_Handle)new_handle((const void *)eb->fParam3, (PA_long32)eb->fParam4);
            } else {
                die("unexpected convert op");
            }
            break;
        }
        case EX_HANDLE_MANAGER: {
            switch(eb->fParam1) {
                case 1: eb->fHandle = (PA_Handle)new_handle(0, (PA_long32)eb->fParam2); break;
                case 2: free_handle((Handle *)eb->fHandle); break;
                case 5: eb->fParam3 = (sLONG_PTR)((Handle *)eb->fHandle)->data; break;
                case 6: break;
                default: die("unexpected handle manager op");
            }
            break;
        }
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unmodelled engine selector %d", selector);
            die(msg);
        }
    }
}

/* ---------- test helpers ---------- */

PA_Unistring U(const char *ascii) {
    u16 s;
    for(const char *p = ascii; *p; p++) s.push_back((PA_Unichar)*p);
    return new_string(s.c_str(), s.size());
}

void put(Obj *o, const char *key, PA_Variable v) {
    PA_Unistring k = U(key);
    PA_SetObjectProperty((PA_ObjectRef)o, &k, v);
    PA_DisposeUnistring(&k);
}

PA_Variable real_var(double d) { PA_Variable v = undefined_var(); PA_SetRealVariable(&v, d); return v; }
PA_Variable bool_var(bool b) { PA_Variable v = undefined_var(); PA_SetBooleanVariable(&v, b); return v; }
PA_Variable null_var() { PA_Variable v = undefined_var(); v.fType = eVK_Null; return v; }

void put_text(Obj *o, const char *key, const char *value) {
    PA_Variable v = undefined_var();
    PA_Unistring u = U(value);
    PA_SetStringVariable(&v, &u);
    put(o, key, v);
    PA_ClearVariable(&v);
}

void put_picture(Obj *o, const char *key) {
    PA_Variable v = undefined_var();
    PA_SetPictureVariable(&v, (PA_Picture)new_picture());
    put(o, key, v);
    PA_ClearVariable(&v);
}

/* Appends o to c (the collection retains it) and drops the caller's reference. */
void append(Obj *c, Obj *o) {
    PA_Variable v = undefined_var();
    PA_SetObjectVariable(&v, (PA_ObjectRef)o);
    PA_SetCollectionElement((PA_CollectionRef)c, (PA_long32)c->items.size(), v);
    PA_ClearVariable(&v);
}

Obj *tag(const char *name) {
    Obj *o = new_object();
    if(name) put_text(o, "name", name);
    return o;
}

bool check(bool ok, const char *what) {
    if(!ok) fprintf(stderr, "  check failed: %s\n", what);
    return ok;
}

bool no_leaks() {
    bool ok = true;
    if(!g_strings.empty()) { fprintf(stderr, "  LEAK: %zu engine string(s)\n", g_strings.size()); ok = false; }
    if(!g_pictures.empty()) { fprintf(stderr, "  LEAK: %zu engine picture(s)\n", g_pictures.size()); ok = false; }
    if(!g_objects.empty()) { fprintf(stderr, "  LEAK: %zu engine object(s)/collection(s)\n", g_objects.size()); ok = false; }
    if(!g_handles.empty()) { fprintf(stderr, "  LEAK: %zu engine handle(s)\n", g_handles.size()); ok = false; }
    return ok;
}

std::string narrow(const PA_Unistring &s) {
    std::string r;
    for(PA_long32 i = 0; i < s.fLength; i++) r.push_back((char)s.fString[i]);
    return r;
}

const char *kNoExiftool = "exiftool not found in the plugin bundle";

/* Runs a command through the real PluginMain entry point and returns the
   status object it returned (the caller releases it). */
Obj *run(PA_long32 selector, Obj *tags) {
    PA_Unistring path = U("Macintosh HD:tmp:synthetic.jpg");
    PA_CollectionRef col = (PA_CollectionRef)tags;
    void *params[2] = { &path, &col };
    PA_ObjectRef result = 0;
    PluginBlock block;
    block.fParameters = params;
    block.fResult = &result;
    block.fData = 0;
    PluginMain(selector, &block);
    PA_DisposeUnistring(&path);
    return (Obj *)result;
}

/* The status must say exiftool was not found: proves the run stopped at the
   process boundary without launching anything. Releases the status. */
bool stopped_at_boundary(Obj *status) {
    if(!check(status != 0 && g_objects.count(status), "a status object was returned")) return false;
    PA_Unistring k = U("errorMessage");
    auto it = status->props.find(key_of(k));
    PA_DisposeUnistring(&k);
    std::string msg = it != status->props.end() && it->second.fType == eVK_Unistring ? narrow(it->second.uValue.fString) : "";
    release_object(status);
    return check(msg == kNoExiftool, "stopped at the stubbed process boundary (exiftool never ran)");
}

/* ExifTool Set Metadata with a one-element collection. */
bool set_one(Obj *element) {
    Obj *tags = new_object(true);
    append(tags, element);
    bool ok = stopped_at_boundary(run(2, tags));
    release_object(tags);
    return no_leaks() && ok;
}

/* ---------- tests ---------- */

typedef std::function<bool()> Test;

std::map<std::string, Test> tests() {
    std::map<std::string, Test> t;

    /* Defects reachable from ExifTool Set Metadata: owned results of
       PA_GetCollectionElement / PA_GetObjectProperty are never cleared. */
    t["defect_cmd_set_element_not_cleared"] = [] {
        return set_one(tag(nullptr)); /* no "name": only the element is fetched */
    };
    t["defect_cmd_set_text_value"] = [] {
        Obj *o = tag("Title");
        put_text(o, "value", "hello");
        return set_one(o);
    };
    t["defect_cmd_set_null_value"] = [] {
        Obj *o = tag("Title");
        put(o, "value", null_var());
        return set_one(o);
    };
    t["defect_cmd_set_picture_data"] = [] {
        Obj *o = tag("ThumbnailImage");
        put_picture(o, "data");
        return set_one(o);
    };
    t["defect_cmd_set_numeric_value"] = [] {
        Obj *o = tag("Rating");
        put(o, "value", real_var(3));
        return set_one(o);
    };

    /* Defects in the JSON getters (ob_get_a/ob_get_n are reached by the
       command; ob_get_b/ob_get_c/ob_get_o are dormant). */
    t["defect_ob_get_a_result_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "k", "hello");
        CUTF8String out;
        bool ok = check(ob_get_a((PA_ObjectRef)o, L"k", &out), "defined");
        ok = check(out == CUTF8String((const uint8_t *)"hello"), "value") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_n_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "value", "not a number");
        bool ok = check(ob_get_n((PA_ObjectRef)o, L"value") == 0.0, "wrong kind reads as 0");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_b_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "flag", "true");
        bool ok = check(ob_get_b((PA_ObjectRef)o, L"flag") == false, "wrong kind reads as false");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_c_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "c", "not a collection");
        bool ok = check(ob_get_c((PA_ObjectRef)o, L"c") == 0, "wrong kind reads as NULL");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_o_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "o", "not an object");
        bool ok = check(ob_get_o((PA_ObjectRef)o, L"o") == 0, "wrong kind reads as NULL");
        release_object(o);
        return no_leaks() && ok;
    };

    /* Guards (contract hardening): on a failed get the result is neither
       read nor cleared. */
    t["guard_get_error_result_untouched"] = [] {
        Obj *o = new_object();
        put_text(o, "k", "v");
        put(o, "n", real_var(1));
        g_fail_get = true;
        CUTF8String out;
        ob_get_a((PA_ObjectRef)o, L"k", &out);
        ob_get_n((PA_ObjectRef)o, L"n");
        ob_get_b((PA_ObjectRef)o, L"k");
        ob_get_c((PA_ObjectRef)o, L"k");
        ob_get_o((PA_ObjectRef)o, L"k");
        g_fail_get = false;
        bool ok = check(g_get_calls == 5, "every getter reached the engine");
        ok = check(out.empty(), "failed result was not read") && ok;
        release_object(o);
        return no_leaks() && ok;
    };

    /* Controls: must pass on both baseline and fixed sources. */
    t["control_cmd_set_empty_collection"] = [] {
        Obj *tags = new_object(true);
        bool ok = stopped_at_boundary(run(2, tags));
        release_object(tags);
        return no_leaks() && ok;
    };
    t["control_cmd_set_element_get_error"] = [] {
        /* a failed element get yields no object to clear */
        Obj *tags = new_object(true);
        append(tags, tag("Title"));
        g_fail_get = true;
        bool ok = stopped_at_boundary(run(2, tags));
        g_fail_get = false;
        ok = check(g_get_calls == 1, "element get reached the engine") && ok;
        release_object(tags);
        return no_leaks() && ok;
    };
    t["control_cmd_set_non_object_element"] = [] {
        Obj *tags = new_object(true);
        PA_Variable v = real_var(1);
        PA_SetCollectionElement((PA_CollectionRef)tags, 0, v);
        bool ok = stopped_at_boundary(run(2, tags));
        release_object(tags);
        return no_leaks() && ok;
    };
    t["control_cmd_get_no_exiftool"] = [] {
        bool ok = stopped_at_boundary(run(1, nullptr));
        return no_leaks() && ok;
    };
    t["control_ob_set_values"] = [] {
        Obj *o = new_object();
        ob_set_s((PA_ObjectRef)o, L"s", "text");
        ob_set_s((PA_ObjectRef)o, "s8", "text");
        ob_set_a((PA_ObjectRef)o, L"a", L"text");
        PA_Picture p = (PA_Picture)new_picture();
        ob_set_p((PA_ObjectRef)o, L"p", p); /* the helper takes the picture */
        ob_set_n((PA_ObjectRef)o, L"n", 1.5);
        ob_set_b((PA_ObjectRef)o, L"b", true);
        ob_set_0((PA_ObjectRef)o, L"z");
        bool ok = check(o->props.size() == 7, "seven properties");
        ok = check(g_pictures.size() == 1, "only the object's copy of the picture is live") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_ob_get_n_real"] = [] {
        Obj *o = new_object();
        put(o, "x", real_var(2.25));
        bool ok = check(ob_is_defined((PA_ObjectRef)o, L"x"), "defined");
        ok = check(!ob_is_defined((PA_ObjectRef)o, L"y"), "undefined") && ok;
        ok = check(ob_get_n((PA_ObjectRef)o, L"x") == 2.25, "value") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_ob_get_b_bool"] = [] {
        Obj *o = new_object();
        put(o, "f", bool_var(true));
        bool ok = check(ob_get_b((PA_ObjectRef)o, L"f") == true, "value");
        release_object(o);
        return no_leaks() && ok;
    };
    /* ob_get_o / ob_get_c hand the caller one reference that stays valid
       after the source object is gone; the caller releases it. */
    t["control_ob_get_o_reference_valid"] = [] {
        Obj *o = new_object();
        Obj *child = new_object();
        put_text(child, "k", "v");
        PA_Variable v = undefined_var();
        PA_SetObjectVariable(&v, (PA_ObjectRef)child);
        put(o, "child", v);
        PA_ClearVariable(&v);
        Obj *got = (Obj *)ob_get_o((PA_ObjectRef)o, L"child");
        bool ok = check(got == child, "same object");
        release_object(o);
        ok = check(g_objects.count(child) && child->props.size() == 1, "still valid for the caller") && ok;
        if(g_objects.count(child)) release_object(child);
        return no_leaks() && ok;
    };
    t["control_ob_get_c_reference_valid"] = [] {
        Obj *o = new_object();
        Obj *c = new_object(true);
        append(c, new_object());
        PA_Variable v = undefined_var();
        PA_SetCollectionVariable(&v, (PA_CollectionRef)c);
        put(o, "c", v);
        PA_ClearVariable(&v);
        Obj *got = (Obj *)ob_get_c((PA_ObjectRef)o, L"c");
        bool ok = check(got == c, "same collection");
        release_object(o);
        ok = check(g_objects.count(c) && c->items.size() == 1, "still valid for the caller") && ok;
        if(g_objects.count(c)) release_object(c);
        return no_leaks() && ok;
    };
    return t;
}

} // namespace

int main(int argc, char **argv) {
    gCall4D = engine;
    std::map<std::string, Test> all = tests();
    if(argc == 2 && !strcmp(argv[1], "--list")) {
        for(auto &kv : all) printf("%s\n", kv.first.c_str());
        return 0;
    }
    if(argc != 2 || !all.count(argv[1])) {
        fprintf(stderr, "usage: %s --list | <test>\n", argv[0]);
        return 2;
    }
    return all[argv[1]]() ? 0 : 1;
}
