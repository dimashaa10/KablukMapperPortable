/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#include <common/cmdlib.h>
#include <common/log.h>
#include <common/threads.h>

#include <sys/types.h>
#include <sys/stat.h>

/* MSVC on x64 defines _WIN32 but not WIN32, and has no POSIX getwd() */
#if defined(WIN32) || defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h> /* getcwd(): the old getwd() is deprecated everywhere */
#endif

#ifdef NeXT
#include <libc.h>
#endif

/* Kabluk Mapper: the plain file calls LoadFile() uses. Windows spells the same
 * calls with a leading underscore and needs the binary flag to keep CR from
 * being translated away. */
#if defined(WIN32) || defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#define KMP_OPEN_FLAGS (_O_RDONLY | _O_BINARY)
#define kmp_open _open
#define kmp_read _read
#define kmp_lseek _lseek
#define kmp_close _close
#else
#include <fcntl.h>
#include <unistd.h>
#define KMP_OPEN_FLAGS O_RDONLY
#define kmp_open open
#define kmp_read read
#define kmp_lseek lseek
#define kmp_close close
#endif

#ifdef LINUX
#include <string.h>
#endif

#define PATHSEPERATOR '/'

/* set these before calling CheckParm */
int myargc;
char **myargv;

char com_token[1024];
qboolean com_eof;

qboolean archive;
char archivedir[1024];

/*
 * =================
 * Error
 * For abnormal program terminations
 * =================
 */
void
Error(const char *error, ...)
{
    va_list argptr;

    /* Using lockless prints so we can error out while holding the lock */
    InterruptThreadProgress__();
    logprint_locked__("************ ERROR ************\n");

    va_start(argptr, error);
    logvprint(error, argptr);
    va_end(argptr);
    logprint_locked__("\n");
    tyr_fatal_exit();
}


/*
 * qdir will hold the path up to the base directory (basedir), including the
 * slash:
 *	c:\quake\
 *	/usr/local/games/quake/
 *
 * gamedir will hold qdir + the game directory (id1, hipnotic, etc).
 * SetQdirFromPath requires an input containing both the basedir and the
 * gamedir:
 *	c:\quake\id1\somefile.dat
 *	/usr/local/games/quake/id1/somefile.dat
 * or similar partials:
 *	id1\somefile.dat
 *	quake/id1/somefile.dat
 *
 */

const char basedir[] = "quake";
char qdir[1024];
char gamedir[1024];

void
SetQdirFromPath(char *path)
{
    char temp[1024];
    char *c, *mark;

    if (!(path[0] == '/' || path[0] == '\\' || path[1] == ':')) {
	/* path is partial */
	Q_getwd(temp);
	strcat(temp, path);
	path = temp;
    }

    c = path + strlen(path);
    while (c > path && *c != '/' && *c != '\\')
	c--;
    if (c == path)
	goto out_err;
    mark = c + 1;
    c--;

    /* search for the basedir in path */
    while (c != path) {
	if (!Q_strncasecmp(c, basedir, sizeof(basedir) - 1)) {
	    strncpy (qdir, path, c + sizeof(basedir) - path);
	    logprint("qdir: %s\n", qdir);
	    /* now search for a gamedir in path */
	    c += sizeof(basedir);
	    while (c < mark) {
		if (*c == '/' || *c == '\\') {
		    strncpy(gamedir, path, c + 1 - path);
		    logprint("gamedir: %s\n", gamedir);
		    return;
		}
		c++;
	    }
	    Error("No gamedir in %s", path);
	}
	c--;
    }
out_err:
    Error("%s: no '%s' in %s", __func__, basedir, path);
}

char *
ExpandPath(char *path)
{
    static char full[1024];

    if (!qdir[0])
	Error("ExpandPath called without qdir set");
    if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
	return path;
    sprintf(full, "%s%s", qdir, path);
    return full;
}

char *
ExpandPathAndArchive(char *path)
{
    char *expanded;
    char archivename[1024];

    expanded = ExpandPath(path);

    if (archive) {
	sprintf(archivename, "%s/%s", archivedir, path);
	CopyFile(expanded, archivename);
    }
    return expanded;
}

char *
copystring(const char *s)
{
    char *b;

    b = malloc(strlen(s) + 1);
    strcpy(b, s);
    return b;
}


/*
 * ================
 * I_FloatTime
 * ================
 */
double
I_FloatTime(void)
{
    time_t t;

    time(&t);

    return t;
}

void
Q_getwd(char *out)
{
#if defined(WIN32) || defined(_WIN32)
    _getcwd(out, 256);
    strcat(out, "\\");
#else
#ifdef LINUX

    char *pwd;
    int len;

    pwd = getenv("PWD");
    if (!pwd)
	Error("Couldn't get working directory - "
	      "PWD not set in environment\n");
    len = strlen(pwd);
    if (len > 255)
	Error("Not enough space to hold working dir\n");
    strcpy(out, pwd);

#else
    if (getcwd(out, 256) == NULL)
	Error("Couldn't get working directory: %s", strerror(errno));
    strcat(out, "/");
#endif
#endif
}

void
Q_mkdir(const char *path)
{
#if defined(WIN32) || defined(_WIN32)
    if (_mkdir(path) != -1)
	return;
#else
    if (mkdir(path, 0777) != -1)
	return;
#endif
    if (errno != EEXIST)
	Error("mkdir %s: %s", path, strerror(errno));
}

/*
 * ============
 * FileTime
 * returns -1 if not present
 * ============
 */
int
FileTime(const char *path)
{
    struct stat buf;

    if (stat(path, &buf) == -1)
	return -1;

    return buf.st_mtime;
}

/*
 * ==============
 * COM_Parse
 * Parse a token out of a string
 * ==============
 */
char *
COM_Parse(char *data)
{
    int c;
    int len;

    len = 0;
    com_token[0] = 0;

    if (!data)
	return NULL;

    /* skip whitespace */
  skipwhite:
    while ((c = *data) <= ' ') {
	if (c == 0) {
	    com_eof = true;
	    return NULL;	/* end of file; */
	}
	data++;
    }

    /* skip // (double forward slash) comments */
    if (c == '/' && data[1] == '/') {
	while (*data && *data != '\n')
	    data++;
	goto skipwhite;
    }

    /* skip C-style comments */
    if (c == '/' && data[1] == '*') {
	data += 2;
	while (*data && !(*data == '*' && data[1] == '/'))
	    data++;
	if (*data)
	    data += 2;
	goto skipwhite;
    }

    /* handle quoted strings specially */
    if (c == '\"') {
	data++;
	do {
	    c = *data;
	    if (c)
		data++;
	    if (c == '\"') {
		com_token[len] = 0;
		return data;
	    }
	    com_token[len] = c;
	    len++;
	} while (1);
    }

    /* parse single characters */
    if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':') {
	com_token[len] = c;
	len++;
	com_token[len] = 0;
	return data + 1;
    }

    /* parse a regular word */
    do {
	com_token[len] = c;
	data++;
	len++;
	c = *data;
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\''
	    || c == ':')
	    break;
    } while (c > 32);

    com_token[len] = 0;
    return data;
}

int
Q_strncasecmp(const char *s1, const char *s2, int n)
{
    int c1, c2;

    while (1) {
	c1 = *s1++;
	c2 = *s2++;

	if (!n--)
	    return 0;		/* strings are equal until end point */

	if (c1 != c2) {
	    if (c1 >= 'a' && c1 <= 'z')
		c1 -= ('a' - 'A');
	    if (c2 >= 'a' && c2 <= 'z')
		c2 -= ('a' - 'A');
	    if (c1 != c2)
		return -1;	/* strings not equal */
	}
	if (!c1)
	    return 0;		/* strings are equal */
    }

    return -1;
}

int
Q_strcasecmp(const char *s1, const char *s2)
{
    return Q_strncasecmp(s1, s2, 99999);
}

#if !defined(_MSC_VER)
char *
strupr(char *start)
{
    char *in;

    in = start;
    while (*in) {
	*in = toupper(*in);
	in++;
    }
    return start;
}

char *
strlower(char *start)
{
    char *in;

    in = start;
    while (*in) {
	*in = tolower(*in);
	in++;
    }
    return start;
}
#endif /* !_MSC_VER */


/* ============================================================================
 *                                 MISC FUNCTIONS
 * ============================================================================
 */

/*
 * =================
 * CheckParm
 * Checks for the given parameter in the program's command line arguments
 * Returns the argument number (1 to argc-1) or 0 if not present
 * =================
 */
int
CheckParm(const char *check)
{
    int i;

    for (i = 1; i < myargc; i++) {
	if (!Q_strcasecmp(check, myargv[i]))
	    return i;
    }

    return 0;
}


/*
 * ===========
 * filelength
 * ===========
 */
int
Sys_filelength(FILE *f)
{
    int pos;
    int end;

    pos = ftell(f);
    if (pos < 0)
	pos = 0;
    if (fseek(f, 0, SEEK_END) != 0)
	Error("%s: cannot seek to the end of the file", __func__);
    end = ftell(f);
    fseek(f, pos, SEEK_SET);

    /* Kabluk Mapper: a failed ftell returns -1, and length -1 turned into
     * malloc(0) plus a fread() of (size_t)-1 bytes - on the PSP that is an
     * exception, not a message in the log. */
    if (end < 0)
	Error("%s: cannot size the file", __func__);

    return end;
}

FILE *
SafeOpenWrite(const char *filename)
{
    FILE *f;

    f = fopen(filename, "wb");

    if (!f)
	Error("Error opening %s: %s", filename, strerror(errno));

    return f;
}

FILE *
SafeOpenRead(const char *filename)
{
    FILE *f;

    f = fopen(filename, "rb");

    if (!f)
	Error("Error opening %s: %s", filename, strerror(errno));

    return f;
}

void
SafeRead(FILE *f, void *buffer, int count)
{
    /* Kabluk Mapper: (size_t)-1 would be a four gigabyte read. */
    if (count < 0)
	Error("File read failure: negative length");
    if (fread(buffer, 1, (size_t)count, f) != (size_t) count)
	Error("File read failure");
}

void
SafeWrite(FILE *f, const void *buffer, int count)
{
    if (count < 0)
	Error("File write failure: negative length");
    if (fwrite(buffer, 1, (size_t)count, f) != (size_t) count)
	Error("File write failure");
}

/*
 * ==============
 * LoadFile
 * ==============
 */
/* Kabluk Mapper: the size of the file LoadFile() read last, so LoadBSPFile()
 * can tell a cut-off lump from an intact one instead of memcpy()ing past the
 * end of the buffer. -1 when unknown. */
static int loaded_file_size = -1;

int
Sys_LoadedFileSize(void)
{
    return loaded_file_size;
}

/*
 * Kabluk Mapper: the tools used to read the file with fopen/fread/fclose. On
 * the PSP the chain stopped on the first big .bsp with the log ending right
 * after the read and before "BSP is version", so it died in the tail of that
 * sequence - the close, or the free of the stream buffer inside it. Plain
 * open/lseek/read/close have no stream to set up and no buffer to give back,
 * so the read cannot fail that way at all, and each call leaves a line of its
 * own behind in case one of them does.
 */
/* Kabluk Mapper: a step the phase file keeps, for the loads big enough to be
 * worth it. The log says a line at a time; this says the same thing through a
 * file that is opened, appended and closed on its own, so the two disagree
 * when the death is inside the logging. */
static void load_phase(const char *step)
{
    if (tyr_phase_hook != NULL)
	tyr_phase_hook(step);
}

int
LoadFile(const char *filename, void *destptr)
{
    byte **bufferptr = destptr;
    byte *buffer;
    int handle;
    int length;
    int offset;

    load_phase("load: open");
    handle = kmp_open(filename, KMP_OPEN_FLAGS);
    if (handle < 0)
	Error("%s: cannot open %s", __func__, filename);
    length = (int)kmp_lseek(handle, 0, SEEK_END);
    if (length < 0) {
	kmp_close(handle);
	Error("%s: cannot size %s", __func__, filename);
    }
    if (length == 0) {
	kmp_close(handle);
	Error("%s: %s is empty", __func__, filename);
    }
    (void)kmp_lseek(handle, 0, SEEK_SET);
    loaded_file_size = length;
    /* Kabluk Mapper: a load that kills the console leaves its last line in the
     * log, so the size, the free heap and each of the calls that can die
     * (allocate, read, close) say so before they run. */
    logprint("%s is %d bytes, %d KB free\n", filename, length, tyr_free_kb());
    logprint("---- allocating %d bytes ----\n", length);
    load_phase("load: alloc");
    buffer = *bufferptr = malloc((size_t)length + 1);
    if (!buffer) {
	kmp_close(handle);
	Error("%s: allocation of %i bytes failed.", __func__, length);
    }
    logprint("---- allocated, reading ----\n");

    /* Kabluk Mapper: a single read() can come back short on the memory stick,
     * and the old fread() check was the only thing that noticed. */
    load_phase("load: read");
    offset = 0;
    while (offset < length) {
	int got = (int)kmp_read(handle, buffer + offset, (size_t)(length - offset));

	if (got <= 0)
	    break;
	offset += got;
    }
    logprint("---- read %d of %d bytes (%d KB free) ----\n", offset, length,
	     tyr_free_kb());
    load_phase("load: read ok");
    if (offset != length) {
	kmp_close(handle);
	*bufferptr = NULL;
	free(buffer);
	logprint("---- short read, closed ----\n");
	Error("%s: %s stopped at %d of %d bytes", __func__, filename, offset,
	      length);
    }
    load_phase("load: close");
    kmp_close(handle);
    load_phase("load: closed");
    logprint("---- closed ----\n");
    load_phase("load: closed logged");
    buffer[length] = 0;
    load_phase("load: terminated");
    logprint("---- loaded ----\n");
    load_phase("load: done");

    return length;
}

/*
 * ==============
 * SaveFile
 * ==============
 */
void
SaveFile(const char *filename, const void *buffer, int count)
{
    FILE *f;

    f = SafeOpenWrite(filename);
    SafeWrite(f, buffer, count);
    fclose(f);
}

void
DefaultExtension(char *path, const char *extension)
{
    char *src;

    /* if path doesn't have a .EXT, append extension */
    /* (extension should include the .)              */
    src = path + strlen(path) - 1;

    while (*src != PATHSEPERATOR && src != path) {
	if (*src == '.')
	    return;		/* it has an extension */
	src--;
    }

    strcat(path, extension);
}

void
DefaultPath(char *path, const char *basepath)
{
    char temp[128];

    if (path[0] == PATHSEPERATOR)
	return;			/* absolute path location */
    strcpy(temp, path);
    strcpy(path, basepath);
    strcat(path, temp);
}

void
StripFilename(char *path)
{
    int length;

    length = strlen(path) - 1;
    while (length > 0 && path[length] != PATHSEPERATOR)
	length--;
    path[length] = '\0';
}

void
StripExtension(char *path)
{
    int length;

    length = strlen(path) - 1;
    while (length > 0 && path[length] != '.') {
	length--;
	if (path[length] == '/')
	    return;		/* no extension */
    }
    if (length)
	path[length] = 0;
}


/*
 * ====================
 * Extract file parts
 * ====================
 */
void
ExtractFilePath(char *path, char *dest)
{
    char *src;

    src = path + strlen(path) - 1;

    /* back up until a \ or the start */
    while (src != path && *(src - 1) != PATHSEPERATOR)
	src--;

    memcpy(dest, path, src - path);
    dest[src - path] = 0;
}

void
ExtractFileBase(char *path, char *dest)
{
    char *src;

    src = path + strlen(path) - 1;

    /* back up until a \ or the start */
    while (src != path && *(src - 1) != PATHSEPERATOR)
	src--;

    while (*src && *src != '.') {
	*dest++ = *src++;
    }
    *dest = 0;
}

void
ExtractFileExtension(char *path, char *dest)
{
    char *src;

    src = path + strlen(path) - 1;

    /* back up until a . or the start */
    while (src != path && *(src - 1) != '.')
	src--;
    if (src == path) {
	*dest = 0;		/* no extension */
	return;
    }

    strcpy(dest, src);
}

/*
 * ==============
 * ParseNum / ParseHex
 * ==============
 */
int
ParseHex(char *hex)
{
    char *str;
    int num;

    num = 0;
    str = hex;

    while (*str) {
	num <<= 4;
	if (*str >= '0' && *str <= '9')
	    num += *str - '0';
	else if (*str >= 'a' && *str <= 'f')
	    num += 10 + *str - 'a';
	else if (*str >= 'A' && *str <= 'F')
	    num += 10 + *str - 'A';
	else
	    Error("Bad hex number: %s", hex);
	str++;
    }

    return num;
}

int
ParseNum(char *str)
{
    if (str[0] == '$')
	return ParseHex(str + 1);
    if (str[0] == '0' && str[1] == 'x')
	return ParseHex(str + 2);
    return atol(str);
}


/*
 * ============================================================================
 *                            BYTE ORDER FUNCTIONS
 * ============================================================================
 */

#ifdef _SGI_SOURCE
#define __BIG_ENDIAN__
#endif

#ifdef __BIG_ENDIAN__

short
LittleShort(short l)
{
    byte b1, b2;

    b1 = l & 255;
    b2 = (l >> 8) & 255;

    return (b1 << 8) + b2;
}

short
BigShort(short l)
{
    return l;
}


int
LittleLong(int l)
{
    byte b1, b2, b3, b4;

    b1 = l & 255;
    b2 = (l >> 8) & 255;
    b3 = (l >> 16) & 255;
    b4 = (l >> 24) & 255;

    return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int
BigLong(int l)
{
    return l;
}

float
LittleFloat(float l)
{
    union {
	byte b[4];
	float f;
    } in , out;

    in.f = l;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];

    return out.f;
}

float
BigFloat(float l)
{
    return l;
}


#else /* must be little endian */


short
BigShort(short l)
{
    byte b1, b2;

    b1 = l & 255;
    b2 = (l >> 8) & 255;

    return (b1 << 8) + b2;
}

short
LittleShort(short l)
{
    return l;
}

int
BigLong(int l)
{
    byte b1, b2, b3, b4;

    b1 = l & 255;
    b2 = (l >> 8) & 255;
    b3 = (l >> 16) & 255;
    b4 = (l >> 24) & 255;

    return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int
LittleLong(int l)
{
    return l;
}

float
BigFloat(float l)
{
    union {
	byte b[4];
	float f;
    } in , out;

    in.f = l;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];

    return out.f;
}

float
LittleFloat(float l)
{
    return l;
}


#endif

/* ========================================================================= */


/*
 * FIXME: byte swap?
 *
 * this is a 16 bit, non-reflected CRC using the polynomial 0x1021
 * and the initial and final xor values shown below...  in other words, the
 * CCITT standard CRC used by XMODEM
 */

#define CRC_INIT_VALUE    0xffff
#define CRC_XOR_VALUE    0x0000

static unsigned short crctable[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

void
CRC_Init(unsigned short *crcvalue)
{
    *crcvalue = CRC_INIT_VALUE;
}

void
CRC_ProcessByte(unsigned short *crcvalue, byte data)
{
    *crcvalue = (*crcvalue << 8) ^ crctable[(*crcvalue >> 8) ^ data];
}

unsigned short
CRC_Value(unsigned short crcvalue)
{
    return crcvalue ^ CRC_XOR_VALUE;
}

unsigned short CRC_Block (const unsigned char *start, int count)
{
    unsigned short crc;
    CRC_Init (&crc);
    while (count--)
	crc = (crc << 8) ^ crctable[(crc >> 8) ^ *start++];
    return crc;
}

/* ========================================================================= */

/*
 * ============
 * CreatePath
 * ============
 */
void
CreatePath(char *path)
{
    char *ofs, c;

    for (ofs = path + 1; *ofs; ofs++) {
	c = *ofs;
	if (c == '/' || c == '\\') {	/* create the directory */
	    *ofs = 0;
	    Q_mkdir(path);
	    *ofs = c;
	}
    }
}


/*
 * ============
 * CopyFile
 * Used to archive source files
 *============
 */
void
CopyFile(const char *from, char *to)
{
    void *buffer;
    int length;

    length = LoadFile(from, &buffer);
    CreatePath(to);
    SaveFile(to, buffer, length);
    free(buffer);
}
