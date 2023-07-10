/* An implementation of the redo build system
   in portable C with zero dependencies

   http://cr.yp.to/redo.html
   https://jdebp.info/FGA/introduction-to-redo.html
   https://github.com/apenwarr/redo/blob/master/README.md
   http://news.dieweltistgarnichtso.net/bin/redo-sh.html

   To the extent possible under law,
   Leah Neukirchen <leah@vuxu.org>
   has waived all copyright and related or neighboring rights to this work.

   http://creativecommons.org/publicdomain/zero/1.0/
*/

/************************************************************
This redo-c version can be found at:

https://github.com/AndreyDobrovolskyOdessa/redo-c/tree/lualog

which is the fork of:

https://github.com/leahneukirchen/redo-c


Features:

1. Optimized recipes envocations.

2. Dependency loops detected during parallel builds.

3. Build log is Lua table.


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>
********************************************************/


#define _GNU_SOURCE 1

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
/* from musl/src/crypt/crypt_sha256.c */

/* public domain sha256 implementation based on fips180-3 */

struct sha256 {
	uint64_t len;    /* processed message length */
	uint32_t h[8];   /* hash state */
	uint8_t buf[64]; /* message block buffer */
};

static uint32_t ror(uint32_t n, int k) { return (n >> k) | (n << (32-k)); }
#define Ch(x,y,z)  (z ^ (x & (y ^ z)))
#define Maj(x,y,z) ((x & y) | (z & (x | y)))
#define S0(x)	   (ror(x,2) ^ ror(x,13) ^ ror(x,22))
#define S1(x)	   (ror(x,6) ^ ror(x,11) ^ ror(x,25))
#define R0(x)	   (ror(x,7) ^ ror(x,18) ^ (x>>3))
#define R1(x)	   (ror(x,17) ^ ror(x,19) ^ (x>>10))

static const uint32_t K[64] = {
0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void processblock(struct sha256 *s, const uint8_t *buf)
{
	uint32_t W[64], t1, t2, a, b, c, d, e, f, g, h;
	int i;

	for (i = 0; i < 16; i++) {
		W[i] = (uint32_t)buf[4*i]<<24;
		W[i] |= (uint32_t)buf[4*i+1]<<16;
		W[i] |= (uint32_t)buf[4*i+2]<<8;
		W[i] |= buf[4*i+3];
	}
	for (; i < 64; i++)
		W[i] = R1(W[i-2]) + W[i-7] + R0(W[i-15]) + W[i-16];
	a = s->h[0];
	b = s->h[1];
	c = s->h[2];
	d = s->h[3];
	e = s->h[4];
	f = s->h[5];
	g = s->h[6];
	h = s->h[7];
	for (i = 0; i < 64; i++) {
		t1 = h + S1(e) + Ch(e, f, g) + K[i] + W[i];
		t2 = S0(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	s->h[0] += a;
	s->h[1] += b;
	s->h[2] += c;
	s->h[3] += d;
	s->h[4] += e;
	s->h[5] += f;
	s->h[6] += g;
	s->h[7] += h;
}

static void pad(struct sha256 *s)
{
	unsigned r = s->len % 64;

	s->buf[r++] = 0x80;
	if (r > 56) {
		memset(s->buf + r, 0, 64 - r);
		r = 0;
		processblock(s, s->buf);
	}
	memset(s->buf + r, 0, 56 - r);
	s->len *= 8;
	s->buf[56] = s->len >> 56;
	s->buf[57] = s->len >> 48;
	s->buf[58] = s->len >> 40;
	s->buf[59] = s->len >> 32;
	s->buf[60] = s->len >> 24;
	s->buf[61] = s->len >> 16;
	s->buf[62] = s->len >> 8;
	s->buf[63] = s->len;
	processblock(s, s->buf);
}

static void sha256_init(struct sha256 *s)
{
	s->len = 0;
	s->h[0] = 0x6a09e667;
	s->h[1] = 0xbb67ae85;
	s->h[2] = 0x3c6ef372;
	s->h[3] = 0xa54ff53a;
	s->h[4] = 0x510e527f;
	s->h[5] = 0x9b05688c;
	s->h[6] = 0x1f83d9ab;
	s->h[7] = 0x5be0cd19;
}

static void sha256_sum(struct sha256 *s, uint8_t *md)
{
	int i;

	pad(s);
	for (i = 0; i < 8; i++) {
		md[4*i] = s->h[i] >> 24;
		md[4*i+1] = s->h[i] >> 16;
		md[4*i+2] = s->h[i] >> 8;
		md[4*i+3] = s->h[i];
	}
}

static void sha256_update(struct sha256 *s, const void *m, unsigned long len)
{
	const uint8_t *p = m;
	unsigned r = s->len % 64;

	s->len += len;
	if (r) {
		if (len < 64 - r) {
			memcpy(s->buf + r, p, len);
			return;
		}
		memcpy(s->buf + r, p, 64 - r);
		len -= 64 - r;
		p += 64 - r;
		processblock(s, s->buf);
	}
	for (; len >= 64; len -= 64, p += 64)
		processblock(s, p);
	memcpy(s->buf, p, len);
}

/* ------------------------------------------------------------------------- */


/***************** Globals ******************/

int dflag, xflag, wflag, fflag, log_fd, level;

/********************************************/


static void
open_comment(void)
{
	dprintf(log_fd, "--[====================================================================[\n");
}


static void
close_comment(void)
{
	dprintf(log_fd, "--]====================================================================]\n");
}


#define start_msg() if ((log_fd > 0) && (log_fd < 3)) open_comment()
#define end_msg()   if ((log_fd > 0) && (log_fd < 3)) close_comment()


static void
msg(const char *name, const char *error)
{
	start_msg();
	dprintf(2, "%s : %s\n", name, error);
	end_msg();
}


static void
pperror(const char *s)
{
	msg(s, strerror(errno));
}


static const char redo_suffix[] =	".do";
static const char trickpoint[]  =	".do.";
static const char redo_prefix[] =	".do..";
static const char lock_prefix[] =	".do...do..";
static const char target_prefix[] =	".do...do...do..";

static const char dirup[] = "../";


#define TRACK_DELIMITER ':'

static char *
track(const char *target, int track_op)
{
	static char *track_buf = 0;		/* this buffer will hold initial REDO_TRACK
						stored once during the very first envocation
						followed by target cwd and target basename
						varying for consequent envocations */

	static size_t track_buf_size = 0;	/* the whole track_buf size, sufficient for
						holding initial REDO_TRACK and current target
						full path */

	size_t target_len, track_engaged, target_wd_offset;
	char *target_wd, *ptr;

	if (track_buf_size == 0) {				/* the very first invocation */
		track_buf_size = PATH_MAX;
		if (target)					/* suppose getenv("REDO_TRACK") */
			track_buf_size += strlen(target);
		track_buf = malloc(track_buf_size);
		if (!track_buf) {
			perror("malloc");
			exit (-1);
		}
		*track_buf = 0;
		if (target)
			strcpy(track_buf, target);
		track_op = 0;				/* enough for the first time */
	}

	if (!track_op)
		return track_buf;

	if (!target) {					/* strip last path */
		ptr = strrchr(track_buf, TRACK_DELIMITER);
		if (ptr)
			*ptr = 0;
		return ptr;
	}

	/* store cwd in the track_buf */

	target_wd_offset = strlen(track_buf) + 1;
	track_engaged = target_wd_offset + strlen(target) + sizeof lock_prefix + 2;

	while (1) {
		if (track_buf_size > track_engaged) {
			target_wd = getcwd(track_buf + target_wd_offset, track_buf_size - track_engaged);
			if (target_wd)		/* getcwd successful */
				break;
		} else
			errno = ERANGE;

		if (errno == ERANGE) {  /* track_buf_size is not sufficient */
			track_buf_size += PATH_MAX;
			track_buf = realloc(track_buf, track_buf_size);
			if (!track_buf) {
				pperror("realloc");
				return 0;
			}
		} else {
			pperror ("getcwd");
			return 0;
		}
	}

	ptr = strchr(target_wd, '\0');		/* construct target full path */
	*ptr++ = '/';
	strcpy(ptr, target);

	*--target_wd = TRACK_DELIMITER;		/* join target full path with the track_buf */

	/* searching for target full path inside track_buf */

	ptr = track_buf;
	target_len = strlen(target_wd);

	do {
		ptr = strstr(ptr, target_wd);
		if (ptr == target_wd)
			return target_wd + 1;
		ptr += target_len;
	} while (*ptr != TRACK_DELIMITER);

	return 0;
}


static char *
base_name(char *name, int uprel)
{
	char *ptr = strchr(name, '\0');

	do {
		while ((ptr != name) && (*--ptr != '/'));
	} while (uprel--);

	if (*ptr == '/')
		ptr++;

	return ptr;
}


static int
setenvfd(const char *name, int i)
{
	char buf[16];

	snprintf(buf, sizeof buf, "%d", i);

	return setenv(name, buf, 1);
}

#define HASH_LEN 32
#define HEXHASH_LEN (2 * HASH_LEN)
#define HEXDATE_LEN 16


static char redoline[HEXHASH_LEN + 1 + HEXDATE_LEN + 1 + PATH_MAX + 1];

static char * const hexhash = redoline;
static char * const hexdate = redoline + HEXHASH_LEN + 1;
static char * const namebuf = redoline + HEXHASH_LEN + 1 + HEXDATE_LEN + 1;


static char *
hashfile(int fd)
{
	static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	struct sha256 ctx;
	char buf[4096];
	char *a;
	unsigned char hash[HASH_LEN];
	int i;
	ssize_t r;

	sha256_init(&ctx);

	while ((r = read(fd, buf, sizeof buf)) > 0) {
		sha256_update(&ctx, buf, r);
	}

	sha256_sum(&ctx, hash);

	for (i = 0, a = hexhash; i < HASH_LEN; i++) {
		*a++ = hex[hash[i] / sizeof hex];
		*a++ = hex[hash[i] % sizeof hex];
	}

	return hexhash;
}


#define stringize(s) stringyze(s)
#define stringyze(s) #s

static const char *
datestat(struct stat *st)
{
	snprintf(hexdate, HEXDATE_LEN + 1, "%0" stringize(HEXDATE_LEN) PRIx64, (uint64_t)st->st_ctime);

	return hexdate;
}


static const char *
datefile(int fd)
{
	struct stat st = {0};

	fstat(fd, &st);

	return datestat(&st);
}


static const char *
datefilename(const char *name)
{
	struct stat st = {0};

	stat(name, &st);

	return datestat(&st);
}


static const char *
datebuild()
{
	static char build_date[HEXDATE_LEN + 1] = {'\0'};
	const char *dateptr;

	FILE *f;

	if (build_date[0])
		return build_date;

	build_date[HEXDATE_LEN] = '\0';

	dateptr = getenv("REDO_BUILD_DATE");
	if (dateptr)
		return memcpy(build_date, dateptr, HEXDATE_LEN);

	f = tmpfile();
	memcpy(build_date, datefile(fileno(f)), HEXDATE_LEN);
	fclose(f);

	setenv("REDO_BUILD_DATE", build_date, 1);

	return build_date;
}


static char *
file_chdir(int *fd, char *name)
{
	int fd_new;
	char *slash = strrchr(name, '/');

	if (!slash)
		return name;

	*slash = 0;
	fd_new = openat(*fd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	*slash = '/';

	if (fd_new < 0) {
		pperror("openat dir");
		return 0;
	}

	if (fchdir(fd_new) < 0) {
		pperror("chdir");
		return 0;
	}

	*fd = fd_new;

	return slash + 1;
}


#define SUFFIX_LEN	(sizeof redo_suffix - 1)

#define reserve(space)	if (dofile_free < (space)) return -1; dofile_free -= (space)


static int
find_dofile(char *dep, char *dofile_rel, size_t dofile_free, const char *slash)
{
	char *dofile = dofile_rel;

	char *end  = strchr(dep, '\0');
	char *tail = end;
	char *ext, *dep_trickpoint;

	size_t doname_size = (end - dep) + sizeof redo_suffix;

	int uprel, first_name_len;


	/* rewind .do tail inside dependency name */

	for (ext = end - SUFFIX_LEN; ext >= dep; ext -= SUFFIX_LEN) {
		if (strncmp(ext, redo_suffix, SUFFIX_LEN))
			break;
		if (!dflag)		/* skip dofile? */
			return -1;
		tail = ext;
	}

	reserve(doname_size);

	dep_trickpoint = strstr(dep, trickpoint);
	if (dep_trickpoint == tail)
		dep_trickpoint = 0;

	strcpy(end, redo_suffix);

	ext = strchr(dep, '.');

	first_name_len = ext - dep;

	for (uprel = 0 ; slash ; uprel++, slash = strchr(slash + 1, '/')) {

		while (dep != dep_trickpoint) {
			strcpy(dofile, dep);

			if (fflag)
				dprintf(1, "%s\n", dofile_rel);

			if (access(dofile_rel, F_OK) == 0) {
				*dep = '\0';
				return uprel;
			}

			if (dep == tail)
				break;

			while (*++dep != '.');
		}

		dep = ext;

		if (uprel == 0)
			dofile_free += first_name_len;

		reserve(sizeof dirup - 1);

		dofile = stpcpy(dofile, dirup);
	}

	return -1;
}


enum errors {
	OK    = 0,
	ERROR = 1,
	BUSY  = 2
};

#define ERRORS 0xff


enum hints {
	BAD_NAME             = 0x100,
	IS_SOURCE            = 0x200,
	UPDATED_RECENTLY     = 0x400,
	IMMEDIATE_DEPENDENCY = 0x800
};

#define HINTS (~ERRORS)


static int
choose(const char *old, const char *new, int err, void (*perror_f)(const char *))
{
	if (err) {
		if ((access(new, F_OK) == 0) && (remove(new) != 0)) {
			(*perror_f)("remove new");
			err |= ERROR;
		}
	} else {
		if ((access(old, F_OK) == 0) && (remove(old) != 0)) {
			(*perror_f)("remove old");
			err |= ERROR;
		}
		if ((access(new, F_OK) == 0) && (rename(new, old) != 0)) {
			(*perror_f)("rename");
			err |= ERROR;
		}
	}

	return err;
}


static int 
run_script(int dir_fd, int lock_fd, char *dofile_rel, const char *target,
		const char *target_base, const char *target_rel)
{
	int target_err = ERROR;

	pid_t pid;

	char *target_new;
	char target_base_rel[PATH_MAX];
	char target_new_rel[PATH_MAX + sizeof target_prefix];


	if (strlen(target_rel) >= sizeof target_base_rel) {
		dprintf(2, "Target relative name too long -- %s\n", target_rel);
		return ERROR;
	}

	strcpy(target_base_rel, target_rel);
	strcpy(base_name(target_base_rel, 0), target_base);

	strcpy(target_new_rel, target_rel);
	target_new = base_name(target_new_rel, 0);
	strcpy(stpcpy(target_new, target_prefix), target);

	pid = fork();
	if (pid < 0) {
		perror("fork");
	} else if (pid == 0) {

		const char *dofile = file_chdir(&dir_fd, dofile_rel);
		char dirprefix[PATH_MAX];
		size_t dirprefix_len = target_new - target_new_rel;

		if (!dofile) {
			dprintf(2, "Damn! Someone have stolen my favorite dofile %s ...\n", dofile_rel);
			exit(ERROR);
		}

		memcpy(dirprefix, target_new_rel, dirprefix_len);
		dirprefix[dirprefix_len] = '\0';

		if ((setenvfd("REDO_LOCK_FD", lock_fd) != 0) ||
		    (setenv("REDO_DIRPREFIX", dirprefix, 1) != 0) ||
		    (setenv("REDO_TRACK", track(0, 0), 1) != 0)) {
			perror("setenv");
			exit(ERROR);
		}

		if (access(dofile, X_OK) != 0)	/* run -x files with /bin/sh */
			execl("/bin/sh", "/bin/sh", xflag ? "-ex" : "-e", dofile,
				target_rel, target_base_rel, target_new_rel, (char *)0);
		else
			execl(dofile, dofile,
				target_rel, target_base_rel, target_new_rel, (char *)0);

		perror("execl");
		exit(ERROR);
	} else {
		if (wait(&target_err) < 0) {
			perror("wait");
		} else {
			if (WCOREDUMP(target_err)) {
				dprintf(2, "Core dumped.\n");
			}
			if (WIFEXITED(target_err)) {
				target_err = WEXITSTATUS(target_err);
			} else if (WIFSIGNALED(target_err)) {
				dprintf(2, "Terminated with %d signal.\n", WTERMSIG(target_err));
			} else if (WIFSTOPPED(target_err)) {
				dprintf(2, "Stopped with %d signal.\n", WSTOPSIG(target_err));
			}
		}
	}

	return choose(target, target_new, target_err, perror);
}


static int
check_record(char *line)
{
	char *w, *last = strchr(line, '\0') - 1;

	if ((last - line) < HEXHASH_LEN + 1 + HEXDATE_LEN + 1) {
		w = "Warning - dependency record too short. Target will be rebuilt";
	} else if (*last != '\n') {
		w = "Warning - dependency record truncated. Target will be rebuilt";
	} else {
		*last = '\0';
		return 1;
	}

	msg(line, w);

	return 0;
}


static int
find_record(char *filename)
{
	char redofile[PATH_MAX + sizeof redo_prefix];
	char *target = base_name(filename, 0);
	int err = ERROR;


	strcpy(redofile, filename);
	strcpy(base_name(redofile, 0), redo_prefix);
	strcat(redofile, target);

	FILE *f = fopen(redofile, "r");

	if (f) {

		while (fgets(redoline, sizeof redoline, f) && check_record(redoline)) {
			if (strcmp(target, namebuf) == 0) {
				err = OK;
				break;
			}
		}

		fclose(f);
	}

	return err;
}


#define may_need_rehash(f,h) ((h & IS_SOURCE) || ((!(h & UPDATED_RECENTLY)) && find_record(f)))


static int
dep_changed(char *line, int hint)
{
	char *filename = line + HEXHASH_LEN + 1 + HEXDATE_LEN + 1;
	int fd;

	if (may_need_rehash(filename, hint)) {
		if (strncmp(line + HEXHASH_LEN + 1, datefilename(filename), HEXDATE_LEN) == 0)
			return 0;

		fd = open(filename, O_RDONLY);
		hashfile(fd);
		close(fd);
	} else {
		if (strncmp(line + HEXHASH_LEN + 1, hexdate, HEXDATE_LEN) == 0)
			return 0;
	}

	return strncmp(line, hexhash, HEXHASH_LEN);
}


static int
write_dep(int lock_fd, char *file, const char *dp, const char *updir, int hint)
{
	int err = 0;
	int fd;
	const char *prefix = "";


	if (may_need_rehash(file, hint)) {
		fd = open(file, O_RDONLY);
		hashfile(fd);
		datefile(fd);
		if (fd > 0)
			close(fd);
	}

	if (dp && *file != '/') {
		size_t dp_len = strlen(dp);

		if (strncmp(file, dp, dp_len) == 0)
			file += dp_len;
		else
			prefix = updir;
	}

	hexhash[HEXHASH_LEN] = 0;
	hexdate[HEXDATE_LEN] = 0;
	if (dprintf(lock_fd, "%s %s %s%s\n", hexhash, hexdate, prefix, file) < 0) {
		pperror("dprintf");
		err = ERROR;
	}

	return err;
}


#define INDENT 2


static int update_dep(int *dir_fd, char *dep_path);


static int
do_update_dep(int dir_fd, char *dep_path, int *hint)
{
	int dep_dir_fd = dir_fd, dep_err;

	level += INDENT;

	dep_err = update_dep(&dep_dir_fd, dep_path);

	if ((dep_err & BAD_NAME) == 0)
		track(0, 1);	/* strip the last record */

	if (dir_fd != dep_dir_fd) {
		if (fchdir(dir_fd) < 0) {
			pperror("chdir back");
			dep_err |= ERROR;
		}
		close(dep_dir_fd);
	}

	*hint = dep_err & HINTS;

	level -= INDENT;

	return dep_err & ERRORS;
}


static void
log_up(int err) {
	dprintf(log_fd, "%*serr = %d,\n", level, "", err);
	dprintf(log_fd, "%*s},\n", level, "");
}

#define log_name(name)   if (log_fd > 0) dprintf(log_fd, "%*s\"%s\",\n", level, "", name)

#define open_level()     if (log_fd > 0) dprintf(log_fd, "%*s{\n", level, "")
#define close_level(err) if (log_fd > 0) log_up(err)



#define NAME_MAX 255


static int
update_dep(int *dir_fd, char *dep_path)
{
	char *dep, *target_full;
	char target_base[NAME_MAX + 1];

	char dofile_rel[PATH_MAX];

	char redofile[NAME_MAX + 1];
	char lockfile[NAME_MAX + 1];

	int uprel, lock_fd, dep_err = 0, wanted = 1, hint;

	struct stat redo_st = {0};

	FILE *fredo;


	if (strchr(dep_path, TRACK_DELIMITER)) {
		msg(dep_path, "Illegal symbol " stringize(TRACK_DELIMITER));
		return ERROR | BAD_NAME;
	}

	dep = file_chdir(dir_fd, dep_path);
	if (dep == 0) {
		msg(dep_path, "Missing dependency directory");
		return ERROR | BAD_NAME;
	}

	if (strlen(dep) > (sizeof target_base - sizeof target_prefix)) {
		msg(dep, "Dependency name too long");
		return ERROR | BAD_NAME;
	}

	target_full = track(dep, 1);
	if (target_full == 0) {
		msg(dep_path, "Dependency loop attempt");
		return wflag ? IS_SOURCE : ERROR;
	}

	strcpy(target_base, dep);
	strcpy(stpcpy(redofile, redo_prefix), dep);
	strcpy(stpcpy(lockfile, lock_prefix), dep);

	log_name(target_full);

	if (fflag)
		dprintf(1, "--[[\n");

	uprel = find_dofile(target_base, dofile_rel, sizeof dofile_rel, target_full);

	if (fflag)
		dprintf(1, "--]]\n");

	if (uprel < 0)
		return IS_SOURCE;

	open_level();

	stat(redofile, &redo_st);

	if (strcmp(datestat(&redo_st), datebuild()) >= 0) {
		dep_err = (redo_st.st_mode & S_IRUSR) ? OK : ERROR;
		close_level(dep_err);
		return dep_err;
	}

	lock_fd = open(lockfile, O_CREAT | O_WRONLY | O_EXCL, 0666);
	if (lock_fd < 0) {
		if (errno == EEXIST) {
			dep_err = BUSY | IMMEDIATE_DEPENDENCY;
		} else {
			pperror("open exclusive");
			dep_err = ERROR;
		}
	}

	if (dep_err) {
		close_level(dep_err);
		return dep_err;
	}


	fredo = fopen(redofile, "r");

	if (fredo) {
		char line[HEXHASH_LEN + 1 + HEXDATE_LEN + 1 + PATH_MAX + 1];
		char *filename = line + HEXHASH_LEN + 1 + HEXDATE_LEN + 1;
		int is_dofile = 1;

		while (fgets(line, sizeof line, fredo) && check_record(line)) {
			int self = !strcmp(filename, dep);
			hint = IS_SOURCE;

			if (is_dofile && strcmp(filename, dofile_rel))
				strcpy(filename, dofile_rel);

			if (!self) {
				dep_err = do_update_dep(*dir_fd, filename, &hint);
			}

			is_dofile = 0;

			if (dep_err || dep_changed(line, hint))
				break;

			memcpy(hexhash, line, HEXHASH_LEN);

			dep_err = write_dep(lock_fd, filename, 0, 0, UPDATED_RECENTLY);
			if (dep_err)
				break;

			if (self) {
				wanted = 0;
				break;
			}
		}
		fclose(fredo);
		hint = 0;
	} else {
		dep_err = do_update_dep(*dir_fd, dofile_rel, &hint);
	}

	target_full = strrchr(track(0, 0), TRACK_DELIMITER) + 1;

	if (!dep_err && wanted) {
		lseek(lock_fd, 0, SEEK_SET);

		dep_err = write_dep(lock_fd, dofile_rel, 0, 0, hint);

		if (!dep_err) {
			start_msg();
			dep_err = run_script(*dir_fd, lock_fd, dofile_rel, dep,
					target_base, base_name(target_full, uprel));
			end_msg();

			if (!dep_err)
				dep_err = write_dep(lock_fd, dep, 0, 0, IS_SOURCE);
		}

		if (dep_err && (dep_err != BUSY)) {
			chmod(redofile, redo_st.st_mode & (~S_IRUSR));
			start_msg();
			dprintf(2, "redo %*s%s\n", level, "", target_full);
			dprintf(2, "     %*s%s -> %d\n", level, "", dofile_rel, dep_err);
			end_msg();
		}
	}

	close(lock_fd);

	close_level(dep_err);

/*
	Now we will use target_full residing in track to construct
	the lockfile_full. If fchdir() in do_update_target() failed
	then we need the full lockfile name.
*/

	strcpy(base_name(target_full, 0), lockfile);

	dep_err &= ~IMMEDIATE_DEPENDENCY;	/* implicit? */

	return choose(redofile, target_full, dep_err, pperror) | UPDATED_RECENTLY;
}


static int
envint(const char *name)
{
	char *s = getenv(name);

	return s ? strtol(s, 0, 10) : 0;
}


static void
compute_updir(const char *dp, char *u)
{
	*u = 0;

	while (dp) {
		dp = strchr(dp, '/');
		if (dp) {
			dp++;
			u = stpcpy(u, dirup);
		}
	}
}


static int
keepdir()
{
	int fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		perror("dir open");
		exit(ERROR);
	}
	return fd;
}


static int
occurrences(const char *str, int ch)
{
	int n = 0;

	while (str) {
		str = strchr(str, ch);
		if (str) {
			n++;
			if (ch == 0)
				break;
			str++;
		}
	}

	return n;
}


#define RETRIES_DEFAULT 10

#define SHORTEST 20
#define LONGEST  (SHORTEST << RETRIES_DEFAULT)

#define MSEC_PER_SEC  1000
#define NSEC_PER_MSEC 1000000

static void
hurry_up_if(int successful)
{
	static int night = SHORTEST;
	int asleep;
	struct timespec s, r;

	if (successful) {
		night = SHORTEST;
		return;
	}

	asleep = (rand() % night) + 1;

	if (night < LONGEST)
		night *= 2;

	s.tv_sec  =  asleep / MSEC_PER_SEC;
	s.tv_nsec = (asleep % MSEC_PER_SEC) * NSEC_PER_MSEC;

	nanosleep(&s, &r);
}


static void
fence(char *top, void (*hill)(void))
{
	if (log_fd > 0) {
		if (level > 0) {
			if (log_fd < 3)
				(*hill)();

		} else
			dprintf(log_fd, "%s\n", top);
	}
}


struct roadmap {
	size_t size;
	int num;
	int todo;
	int done;
	char **name;
	int32_t *status;
	int32_t *children;
	int32_t *child;
};


static int
text2int(int32_t *x, int cnt, char **p)
{
	char *ep;
	long v;

	while (cnt--) {
		v = strtol(*p, &ep, 10);
		if ((ep - *p) < (int) (sizeof (int32_t)))
			return ERROR;
		*p = ep;
		*x++ = (int32_t) v;
	}

	return OK;
}


static int
text2name(char **x, int cnt, char **p)
{
	while (cnt--) {
		*p = strchr(*p, '\n');
		if (*p == 0)
			return ERROR;
		*(*p)++ = '\0';
		*x++ = *p;
	}

	return OK;
}


static int
test_map(struct roadmap *m)
{
	int i;

	for (i = 0; i < m->children[m->num]; i++) {
		if (m->child[i] >= m->num)
			return ERROR;
	}

	return OK;
}


static int
import_map(struct roadmap *m, char *filename)
{
	struct stat st;
	int fd;
	char *ptr;

	if (stat(filename, &st) < 0)
		return ERROR;

	if (st.st_size == 0)
		return ERROR;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return ERROR;

	m->name = mmap(NULL, st.st_size + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd);
	if (m->name == MAP_FAILED)
		return ERROR;

	m->size = st.st_size + 1;

	ptr = (char *) m->name;

	ptr[st.st_size] = '\0';

	ptr = strchr(ptr, '\n');
	if (ptr == 0)
		return ERROR;

	m->num = (ptr - (char *)m->name) / 8;
	m->todo = m->num;
	m->done = 0;

	m->status = (int32_t *) ptr;
	m->children = m->status + m->num;
	m->child = m->children + m->num + 1;

	if (text2int(m->status, m->num, &ptr) ||
	    text2int(m->children, m->num + 1, &ptr) || 
	    text2int(m->child, m->children[m->num], &ptr) ||
	    text2name(m->name, m->num, &ptr))
		return ERROR;

	return test_map(m);
}


static void
init_map(struct roadmap *m, int argc, char **argv)
{
	m->num  = argc;
	m->todo = m->num;
	m->done = 0;
	m->name = argv;

	m->status = calloc(2 * m->num + 1, sizeof (int32_t));
	if (m->status == 0) {
		perror("calloc");
		exit(ERROR);
	}

	m->children = m->status + m->num;
}


static void
approve(struct roadmap *m, int i)
{
	int j;

	m->status[i] = -1;
	m->done++;
	for (j = m->children[i]; j < m->children[i + 1]; j++)
		m->status[m->child[j]]--;
}


static int
forget(struct roadmap *m, int i)
{
	int own = m->children[i];
	int num = m->children[i + 1] - own;
	int child = m->child[own];

	if ((num == 0) || ((num == 1) && (m->status[child] == 1) && forget(m, child))) {
		m->status[i] = -1;
		m->todo--;
		return 1;
	}

	return 0;
}


int
main(int argc, char *argv[])
{
	int opt;

	struct roadmap dep = {.size = 0};

	const char *dirprefix;
	char updir[PATH_MAX];

	int main_dir_fd, lock_fd = -1, retries, attempts, i, err;


	opterr = 0;

	while ((opt = getopt(argc, argv, "+dxwfl:m:")) != -1) {
		switch (opt) {
		case 'x':
			setenvfd("REDO_TRACE", 1);
			break;
		case 'w':
			setenvfd("REDO_WARNING", 1);
			break;
		case 'd':
			setenvfd("REDO_DOFILES", 1);
			break;
		case 'f':
			setenvfd("REDO_FIND", 1);
			break;
		case 'l':
			if (strcmp(optarg, "1") == 0) {
				log_fd = 1;
			} else if (strcmp(optarg, "2") == 0) {
				log_fd = 2;
			} else {
				log_fd =  open(optarg, O_CREAT | O_WRONLY | O_TRUNC, 0666);
				if (log_fd < 0) {
					perror("logfile");
					return ERROR;
				}
			}
			setenvfd("REDO_LOG_FD", log_fd);
			break;
		case 'm':
			if (import_map(&dep, optarg) != OK) {
				dprintf(2, "Warning: failed to import the roadmap from %s.\n", optarg);
				if (dep.size != 0) {
					munmap(dep.name, dep.size);
					dep.size = 0;
				}
			}
			break;
		default:
			dprintf(2, "Usage: redo [-dxwf] [-l <logname>] [-m <roadmap>] [TARGETS...]\n");
			return ERROR;
		}
	}

	xflag = envint("REDO_TRACE");
	wflag = envint("REDO_WARNING");
	dflag = envint("REDO_DOFILES");
	fflag = envint("REDO_FIND");

	log_fd = envint("REDO_LOG_FD");


	if (dep.size == 0)
		init_map(&dep, argc - optind, argv + optind);

	dirprefix = getenv("REDO_DIRPREFIX");
	compute_updir(dirprefix, updir);

	level = occurrences(track(getenv("REDO_TRACK"), 0), TRACK_DELIMITER) * INDENT;

	if (strcmp(base_name(argv[0], 0), "redo") != 0) {
		lock_fd = envint("REDO_LOCK_FD");
	}

	retries = envint("REDO_RETRIES");
	unsetenv("REDO_RETRIES");

	if ((lock_fd < 0) && (retries == 0))
		retries = RETRIES_DEFAULT;
	attempts = retries + 1;


	datebuild();

	main_dir_fd = keepdir();

	srand(getpid());

	fence("return {", close_comment);

	do {
		hurry_up_if(attempts >= retries);

		for (i = 0; i < dep.num ; i++) {
			if (dep.status[i] == 0) {
				int hint;

				err = do_update_dep(main_dir_fd, dep.name[i], &hint);

				if ((err == 0) && (lock_fd > 0))
					err = write_dep(lock_fd, dep.name[i], dirprefix, updir, hint);

				if (err == 0) {
					approve(&dep, i);
					attempts = retries + 1;
				} else if (err == BUSY) {
					if (hint & IMMEDIATE_DEPENDENCY)
						forget(&dep, i);
				} else {
					break;
				}
			}
		}
	} while ((i == dep.num) && (dep.done < dep.todo) && (--attempts > 0));

	fence("}", open_comment);

	return (i < dep.num) ? err : ((dep.done < dep.num) ? BUSY : OK);
}

