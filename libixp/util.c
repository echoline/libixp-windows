/* Written by Kris Maglione <fbsdaemon at gmail dot com> */
/* Public domain */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef __WIN32
	#include <pwd.h>
#endif
#include "ixp_local.h"

char*
ixp_smprint(const char *fmt, ...) {
	va_list ap;
	char *s;

	va_start(ap, fmt);
	s = ixp_vsmprint(fmt, ap);
	va_end(ap);
	if(s == nil)
		ixp_werrstr("no memory");
	return s;
}

static char*
_user(void) {
	static char *user;
#ifndef __WIN32
	struct passwd *pw;

	if(user == nil) {
		pw = getpwuid(getuid());
		if(pw)
			user = strdup(pw->pw_name);
	}
#endif
	if(user == nil)
		user = "none";
	return user;
}

static int
rmkdir(char *path, int mode) {
	char *p;
	int ret;
	char c;

	for(p = path+1; ; p++) {
		c = *p;
		if((c == '/') || (c == '\0')) {
			*p = '\0';
#ifdef __WIN32
			ret = mkdir(path);
#else
			ret = mkdir(path, mode);
#endif
			if((ret == -1) && (errno != EEXIST)) {
				ixp_werrstr("Can't create path '%s': %s", path, ixp_errbuf());
				return 0;
			}
			*p = c;
		}
		if(c == '\0')
			break;
	}
	return 1;
}

static char*
ns_display(void) {
	char *path, *disp;
	struct stat st;

	disp = getenv("DISPLAY");
	if(disp == nil || disp[0] == '\0') {
		ixp_werrstr("$DISPLAY is unset");
		return nil;
	}

	disp = estrdup(disp);
	path = &disp[strlen(disp) - 2];
	if(path > disp && !strcmp(path, ".0"))
		*path = '\0';

	path = ixp_smprint("/tmp/ns.%s.%s", _user(), disp);
	free(disp);

	if(!rmkdir(path, 0700))
		;
	else if(stat(path, &st))
		ixp_werrstr("Can't stat ns_path '%s': %s", path, ixp_errbuf());
#ifndef __WIN32
	else if(getuid() != st.st_uid)
		ixp_werrstr("ns_path '%s' exists but is not owned by you", path);
#endif
	else if((st.st_mode & 077) && chmod(path, st.st_mode & ~077))
		ixp_werrstr("Namespace path '%s' exists, but has wrong permissions: %s", path, ixp_errbuf());
	else
		return path;
	free(path);
	return nil;
}

/**
 * Function: ixp_namespace
 *
 * Returns the path of the canonical 9p namespace directory.
 * Either the value of $NAMESPACE, if it's set, or, roughly,
 * /tmp/ns.${USER}.${DISPLAY:%.0=%}. In the latter case, the
 * directory is created if it doesn't exist, and it is
 * ensured to be owned by the current user, with no group or
 * other permissions.
 *
 * Returns:
 *   A statically allocated string which must not be freed
 * or altered by the caller. The same value is returned
 * upon successive calls.
 */
/* Not especially threadsafe. */
char*
ixp_namespace(void) {
	static char *namespace;

	if(namespace == nil)
		namespace = getenv("NAMESPACE");
	if(namespace == nil)
		namespace = ns_display();
	return namespace;
}

void
eprint(const char *fmt, ...) {
	va_list ap;
	int err;

	err = errno;
	fprintf(stderr, "libixp: fatal: ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if(fmt[strlen(fmt)-1] == ':')
		fprintf(stderr, " %s\n", strerror(err));
	else
		fprintf(stderr, "\n");

	exit(1);
}

/* Can't malloc */
static void
mfatal(char *name, uint size) {
	const char
		couldnot[] = "libixp: fatal: Could not ",
		paren[] = "() ",
		bytes[] = " bytes\n";
	char sizestr[8];
	int i;
	
	i = sizeof sizestr;
	do {
		sizestr[--i] = '0' + (size%10);
		size /= 10;
	} while(size > 0);

	write(1, couldnot, sizeof(couldnot)-1);
	write(1, name, strlen(name));
	write(1, paren, sizeof(paren)-1);
	write(1, sizestr+i, sizeof(sizestr)-i);
	write(1, bytes, sizeof(bytes)-1);

	exit(1);
}

void*
emalloc(uint size) {
	void *ret = malloc(size);
	if(!ret)
		mfatal("malloc", size);
	return ret;
}

void*
emallocz(uint size) {
	void *ret = emalloc(size);
	memset(ret, 0, size);
	return ret;
}

void*
erealloc(void *ptr, uint size) {
	void *ret = realloc(ptr, size);
	if(!ret)
		mfatal("realloc", size);
	return ret;
}

char*
estrdup(const char *str) {
	void *ret = malloc(strlen(str) + 1);
	if(!ret)
		mfatal("strdup", strlen(str));
	strcpy(ret, str);
	return ret;
}

uint
tokenize(char *res[], uint reslen, char *str, char delim) {
	char *s;
	uint i;

	i = 0;
	s = str;
	while(i < reslen && *s) {
		while(*s == delim)
			*(s++) = '\0';
		if(*s)
			res[i++] = s;
		while(*s && *s != delim)
			s++;
	}
	return i;
}

uint
strlcat(char *dst, const char *src, uint size) {
	const char *s;
	char *d;
	int n, len;

	d = dst;
	s = src;
	n = size;
	while(n-- > 0 && *d != '\0')
		d++;
	len = n;

	while(*s != '\0' && n-- > 0)
		*d++ = *s++;
	while(*s++ != '\0')
		n--;
	if(len > 0)
		*d = '\0';
	return size - n - 1;
}

