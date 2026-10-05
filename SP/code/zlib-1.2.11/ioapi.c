/* ioapi.h -- IO base function header for compress/uncompress .zip
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

         For more info read MiniZip_info.txt

*/

#if defined(_WIN32) && (!(defined(_CRT_SECURE_NO_WARNINGS)))
        #define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(__APPLE__) || defined(IOAPI_NO_64)
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif


#include "ioapi.h"
#ifdef __SWITCH__
#include <string.h>
#endif

voidpf call_zopen64 (const zlib_filefunc64_32_def* pfilefunc,const void*filename,int mode)
{
    if (pfilefunc->zfile_func64.zopen64_file != NULL)
        return (*(pfilefunc->zfile_func64.zopen64_file)) (pfilefunc->zfile_func64.opaque,filename,mode);
    else
    {
        return (*(pfilefunc->zopen32_file))(pfilefunc->zfile_func64.opaque,(const char*)filename,mode);
    }
}

long call_zseek64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream, ZPOS64_T offset, int origin)
{
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.zseek64_file)) (pfilefunc->zfile_func64.opaque,filestream,offset,origin);
    else
    {
        uLong offsetTruncated = (uLong)offset;
        if (offsetTruncated != offset)
            return -1;
        else
            return (*(pfilefunc->zseek32_file))(pfilefunc->zfile_func64.opaque,filestream,offsetTruncated,origin);
    }
}

ZPOS64_T call_ztell64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream)
{
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.ztell64_file)) (pfilefunc->zfile_func64.opaque,filestream);
    else
    {
        uLong tell_uLong = (*(pfilefunc->ztell32_file))(pfilefunc->zfile_func64.opaque,filestream);
        if ((tell_uLong) == MAXU32)
            return (ZPOS64_T)-1;
        else
            return tell_uLong;
    }
}

void fill_zlib_filefunc64_32_def_from_filefunc32(zlib_filefunc64_32_def* p_filefunc64_32,const zlib_filefunc_def* p_filefunc32)
{
    p_filefunc64_32->zfile_func64.zopen64_file = NULL;
    p_filefunc64_32->zopen32_file = p_filefunc32->zopen_file;
    p_filefunc64_32->zfile_func64.zerror_file = p_filefunc32->zerror_file;
    p_filefunc64_32->zfile_func64.zread_file = p_filefunc32->zread_file;
    p_filefunc64_32->zfile_func64.zwrite_file = p_filefunc32->zwrite_file;
    p_filefunc64_32->zfile_func64.ztell64_file = NULL;
    p_filefunc64_32->zfile_func64.zseek64_file = NULL;
    p_filefunc64_32->zfile_func64.zclose_file = p_filefunc32->zclose_file;
    p_filefunc64_32->zfile_func64.zerror_file = p_filefunc32->zerror_file;
    p_filefunc64_32->zfile_func64.opaque = p_filefunc32->opaque;
    p_filefunc64_32->zseek32_file = p_filefunc32->zseek_file;
    p_filefunc64_32->ztell32_file = p_filefunc32->ztell_file;
}



static voidpf  ZCALLBACK fopen_file_func OF((voidpf opaque, const char* filename, int mode));
static uLong   ZCALLBACK fread_file_func OF((voidpf opaque, voidpf stream, void* buf, uLong size));
static uLong   ZCALLBACK fwrite_file_func OF((voidpf opaque, voidpf stream, const void* buf,uLong size));
static ZPOS64_T ZCALLBACK ftell64_file_func OF((voidpf opaque, voidpf stream));
static long    ZCALLBACK fseek64_file_func OF((voidpf opaque, voidpf stream, ZPOS64_T offset, int origin));
static int     ZCALLBACK fclose_file_func OF((voidpf opaque, voidpf stream));
static int     ZCALLBACK ferror_file_func OF((voidpf opaque, voidpf stream));

static voidpf ZCALLBACK fopen_file_func (voidpf opaque, const char* filename, int mode)
{
    FILE* file = NULL;
    const char* mode_fopen = NULL;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL) && (mode_fopen != NULL))
        file = fopen(filename, mode_fopen);
    return file;
}

static voidpf ZCALLBACK fopen64_file_func (voidpf opaque, const void* filename, int mode)
{
    FILE* file = NULL;
    const char* mode_fopen = NULL;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL) && (mode_fopen != NULL))
        file = FOPEN_FUNC((const char*)filename, mode_fopen);
    return file;
}


static uLong ZCALLBACK fread_file_func (voidpf opaque, voidpf stream, void* buf, uLong size)
{
    uLong ret;
    ret = (uLong)fread(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
}

static uLong ZCALLBACK fwrite_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size)
{
    uLong ret;
    ret = (uLong)fwrite(buf, 1, (size_t)size, (FILE *)stream);
    return ret;
}

static long ZCALLBACK ftell_file_func (voidpf opaque, voidpf stream)
{
    long ret;
    ret = ftell((FILE *)stream);
    return ret;
}


static ZPOS64_T ZCALLBACK ftell64_file_func (voidpf opaque, voidpf stream)
{
    ZPOS64_T ret;
    ret = FTELLO_FUNC((FILE *)stream);
    return ret;
}

static long ZCALLBACK fseek_file_func (voidpf  opaque, voidpf stream, uLong offset, int origin)
{
    int fseek_origin=0;
    long ret;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
    }
    ret = 0;
    if (fseek((FILE *)stream, offset, fseek_origin) != 0)
        ret = -1;
    return ret;
}

static long ZCALLBACK fseek64_file_func (voidpf  opaque, voidpf stream, ZPOS64_T offset, int origin)
{
    int fseek_origin=0;
    long ret;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = SEEK_SET;
        break;
    default: return -1;
    }
    ret = 0;

    if(FSEEKO_FUNC((FILE *)stream, offset, fseek_origin) != 0)
                        ret = -1;

    return ret;
}


static int ZCALLBACK fclose_file_func (voidpf opaque, voidpf stream)
{
    int ret;
    ret = fclose((FILE *)stream);
    return ret;
}

static int ZCALLBACK ferror_file_func (voidpf opaque, voidpf stream)
{
    int ret;
    ret = ferror((FILE *)stream);
    return ret;
}

#ifdef __SWITCH__
/* The Switch's: a 4 KB read window kept across seeks. The zip code reads a
   pak's directory a byte at a time (unzip.c's unz64local_getByte), seeking
   between fields, and each newlib fread costs microseconds; reads of 4 KB or
   more go straight to the file, in one call to the console's file service,
   not through newlib's 1 KB buffer */
#define SWITCH_ZWINDOW 4096

typedef struct {
    FILE *file;
    ZPOS64_T pos;       /* the reader's */
    ZPOS64_T start;     /* the window's */
    uLong length;
    unsigned char window[SWITCH_ZWINDOW];
} switch_zfile;

static voidpf ZCALLBACK switch_open64_file_func (voidpf opaque, const void* filename, int mode)
{
    FILE *file = (FILE *)fopen64_file_func(opaque, filename, mode);
    switch_zfile *z;

    if (file == NULL)
        return NULL;
    z = (switch_zfile *)malloc(sizeof(*z));
    if (z == NULL) {
        fclose(file);
        return NULL;
    }
    setvbuf(file, NULL, _IONBF, 0);
    z->file = file;
    z->pos = z->start = 0;
    z->length = 0;
    return z;
}

static uLong ZCALLBACK switch_read_file_func (voidpf opaque, voidpf stream, void* buf, uLong size)
{
    switch_zfile *z = (switch_zfile *)stream;
    unsigned char *out = (unsigned char *)buf;
    uLong done = 0;

    while (done < size) {
        if (z->pos >= z->start && z->pos < z->start + z->length) {
            uLong offset = (uLong)(z->pos - z->start);
            uLong n = z->length - offset;

            if (n > size - done)
                n = size - done;
            memcpy(out + done, z->window + offset, n);
            done += n;
            z->pos += n;
            continue;
        }
        if (FSEEKO_FUNC(z->file, z->pos, SEEK_SET) != 0)
            break;
        if (size - done >= SWITCH_ZWINDOW) {
            uLong n = (uLong)fread(out + done, 1, (size_t)(size - done), z->file);

            done += n;
            z->pos += n;
            break;
        }
        z->start = z->pos;
        z->length = (uLong)fread(z->window, 1, SWITCH_ZWINDOW, z->file);
        if (z->length == 0)
            break;
    }
    return done;
}

static ZPOS64_T ZCALLBACK switch_tell64_file_func (voidpf opaque, voidpf stream)
{
    return ((switch_zfile *)stream)->pos;
}

static long ZCALLBACK switch_seek64_file_func (voidpf opaque, voidpf stream, ZPOS64_T offset, int origin)
{
    switch_zfile *z = (switch_zfile *)stream;

    switch (origin) {
    case ZLIB_FILEFUNC_SEEK_SET:
        z->pos = offset;
        return 0;
    case ZLIB_FILEFUNC_SEEK_CUR:
        z->pos += offset;
        return 0;
    case ZLIB_FILEFUNC_SEEK_END:
        if (FSEEKO_FUNC(z->file, 0, SEEK_END) != 0)
            return -1;
        z->pos = (ZPOS64_T)FTELLO_FUNC(z->file) + offset;
        return 0;
    default:
        return -1;
    }
}

static int ZCALLBACK switch_close_file_func (voidpf opaque, voidpf stream)
{
    switch_zfile *z = (switch_zfile *)stream;
    int ret = fclose(z->file);

    free(z);
    return ret;
}

static int ZCALLBACK switch_error_file_func (voidpf opaque, voidpf stream)
{
    return ferror(((switch_zfile *)stream)->file);
}
#endif

void fill_fopen_filefunc (pzlib_filefunc_def)
  zlib_filefunc_def* pzlib_filefunc_def;
{
    pzlib_filefunc_def->zopen_file = fopen_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell_file = ftell_file_func;
    pzlib_filefunc_def->zseek_file = fseek_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_fopen64_filefunc (zlib_filefunc64_def*  pzlib_filefunc_def)
{
#ifdef __SWITCH__
    pzlib_filefunc_def->zopen64_file = switch_open64_file_func;
    pzlib_filefunc_def->zread_file = switch_read_file_func;
    pzlib_filefunc_def->zwrite_file = NULL;  /* (the paks are only read) */
    pzlib_filefunc_def->ztell64_file = switch_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = switch_seek64_file_func;
    pzlib_filefunc_def->zclose_file = switch_close_file_func;
    pzlib_filefunc_def->zerror_file = switch_error_file_func;
#else
    pzlib_filefunc_def->zopen64_file = fopen64_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell64_file = ftell64_file_func;
    pzlib_filefunc_def->zseek64_file = fseek64_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
#endif
    pzlib_filefunc_def->opaque = NULL;
}
