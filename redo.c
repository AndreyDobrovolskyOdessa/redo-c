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

/*-------------------------------------------------------------------------**
This redo-c version can be found at:

https://github.com/AndreyDobrovolskyOdessa/redo-c/tree/lualog

which is the fork of:

https://github.com/leahneukirchen/redo-c


Features:

1. Optimized recipes envocations.

2. Dependency loops detected during parallel builds.

3. Build log is Lua table.


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>
**-------------------------------------------------------------------------*/


#define _GNU_SOURCE 1

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

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


/*------------------------------ Globals ------------------------------------*/

int dflag, xflag, wflag, log_fd;

/*---------------------------------------------------------------------------*/


static void
start_msg(void) {
	if ((log_fd == 1) || (log_fd == 2))
		dprintf(log_fd, "--[=========[\n");
}

static void
end_msg(void) {
	if ((log_fd == 1) || (log_fd == 2))
		dprintf(log_fd, "--]=========]\n");
}

static void
pperror(const char *s) {
	char *e = strerror(errno);
	start_msg();
	dprintf(2, "%s : %s\n", s, e);
	end_msg();
}


static const char redo_suffix[] =	".do";
static const char trickpoint[]  =	".do.";
static const char redo_prefix[] =	".do..";
static const char lock_prefix[] =	".do...do..";
static const char target_prefix[] =	".do...do...do..";

static const char updir[] = "../";


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
				perror("realloc");
				return 0;
			}
		} else {
			perror ("getcwd");
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


/*--------- find_dofile() logs examples ---------**

$ redo -l 1 x.y
"/tmp/x.y",
--[[
x.y.do
.y.do
.do
../.y.do
../.do
--]]

$ redo -l 1 .x.y
"/tmp/.x.y",
--[[
.x.y.do
.y.do
.do
../.x.y.do
../.y.do
../.do
--]]

$ redo -d -l 1 x.y.do
"/tmp/x.y.do",
--[[
x.y.do.do
.y.do.do
.do.do
../.y.do.do
../.do.do
--]]

$ redo -d -l 1 .y.do
"/tmp/.y.do",
--[[
.y.do.do
.do.do
../.y.do.do
../.do.do
--]]

$ redo -l 1 ''
"/tmp/",
--[[
.do
../.do
--]]

$ redo -l 1 x.do.y
"/tmp/x.do.y",
--[[
x.do.y.do
--]]

$ redo -l 1 x.y.do.z
"/tmp/x.y.do.z",
--[[
x.y.do.z.do
.y.do.z.do
../.y.do.z.do
--]]

**-----------------------------------------------*/


static char *
find_dofile(char *dep, char *dofile_rel, size_t dofile_free, int *uprel, const char *slash)
{
	char *dofile = dofile_rel;

	char *dep_end  = strchr(dep, '\0');
	char *dep_ptr  = dep_end;
	char *dep_tail = dep_end;
	char *dep_trickpoint;

	const char *suffix_ptr = redo_suffix + sizeof redo_suffix - 1;

	size_t doname_size = (dep_end - dep) + sizeof redo_suffix;

	char *extension;


	/* rewind .do tail inside dependency */

	while (dep_ptr > dep) {
		if (*--dep_ptr != *--suffix_ptr)
			break;
		if (suffix_ptr == redo_suffix) {
			dep_tail = dep_ptr;
			suffix_ptr += sizeof redo_suffix - 1;
		}
	}

	/* we can suppress dofiles doing */

	if ((dep_tail != dep_end) && (!dflag))
		return 0;


	if (dofile_free < doname_size)
		return 0;
	dofile_free -= doname_size;

	dep_trickpoint = strstr(dep, trickpoint);

	strcpy(dep_end, redo_suffix);

	dep_ptr = dep;
	extension = strchr(dep, '.');

	for (*uprel = 0 ; slash ; (*uprel)++, slash = strchr(slash + 1, '/')) {

		while ((dep_ptr != dep_trickpoint) || (dep_ptr == dep_tail)) {
			strcpy(dofile, dep_ptr);

			if (log_fd > 0)
				dprintf(log_fd, "%s\n", dofile_rel);

			if (access(dofile_rel, F_OK) == 0) {
				*dep_ptr = '\0';
				return dofile;
			}

			if (dep_ptr == dep_tail)
				break;

			while (*++dep_ptr != '.');
		}

		dep_ptr = extension;

		if (dofile_free < (sizeof updir - 1))
			return 0;
		dofile_free -= sizeof updir - 1;

		dofile = stpcpy(dofile, updir);
	}

	return 0;
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
		const char *target_base, char *target_full, int uprel)
{
	int target_err = 0;

	pid_t pid;
	const char *target_rel;
	char target_base_rel[PATH_MAX];

	char *target_new;
	char target_new_rel[PATH_MAX + sizeof target_prefix];


	target_rel = base_name(target_full, uprel);
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
		target_err = ERROR;
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
			exit(-1);
		}

		if (access(dofile, X_OK) != 0)   /* run -x files with /bin/sh */
			execl("/bin/sh", "/bin/sh", xflag ? "-ex" : "-e",
			dofile, target_rel, target_base_rel, target_new_rel, (char *)0);
		else
			execl(dofile,
			dofile, target_rel, target_base_rel, target_new_rel, (char *)0);

		perror("execl");
		exit(-1);
	} else {
		if (wait(&target_err) < 0) {
			perror("wait");
			target_err = ERROR;
		} else {
			if (WIFEXITED(target_err)) {
				target_err = WEXITSTATUS(target_err);
			} else {
				if (WCOREDUMP(target_err)) {
					dprintf(2, "Core dumped.\n");
				}
				if (WIFSIGNALED(target_err)) {
					target_err = WTERMSIG(target_err);
					dprintf(2, "Terminated with %d signal.\n", target_err);
				} else if (WIFSTOPPED(target_err)) {
					target_err = WSTOPSIG(target_err);
					dprintf(2, "Stopped with %d signal.\n", target_err);
				}
				target_err = ERROR;
			}
		}
	}

	return choose(target, target_new, target_err, perror);
}


static int
check_record(char *line)
{
	int line_len = strlen(line);

	if (line_len < HEXHASH_LEN + 1 + HEXDATE_LEN + 1 + 1) {
		start_msg();
		dprintf(2, "Warning: dependency record too short. Target will be rebuilt.\n");
		end_msg();
		return 0;
	}

	if (line[line_len - 1] != '\n') {
		start_msg();
		dprintf(2, "Warning: dependency record truncated. Target will be rebuilt.\n");
		end_msg();
		return 0;
	}

	line[line_len - 1] = 0; /* strip \n */

	return 1;
}


static int
find_record(char *filename)
{
	char redofile[PATH_MAX + sizeof redo_prefix];
	char *target = base_name(filename, 0);
	int find_err = 1;


	strcpy(redofile, filename);
	strcpy(base_name(redofile, 0), redo_prefix);
	strcat(redofile, target);

	FILE *f = fopen(redofile, "r");

	if (f) {

		while (fgets(redoline, sizeof redoline, f) && check_record(redoline)) {
			if (strcmp(target, namebuf) == 0) {
				find_err = 0;
				break;
			}
		}

		fclose(f);
	}

	return find_err;
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

	if (strncmp(line, hexhash, HEXHASH_LEN) == 0)
		return 0;

	return 1;
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


static int update_dep(int *dir_fd, char *dep_path, int nlevel);


static int
do_update_dep(int dir_fd, char *dep_path, int nlevel, int *hint)
{
	int dep_dir_fd = dir_fd;
	int dep_err = update_dep(&dep_dir_fd, dep_path, nlevel);

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

	return dep_err & ERRORS;
}


#define NAME_MAX 255


static int
update_dep(int *dir_fd, char *dep_path, int nlevel)
{
	char *dep, *dofile;
	char *target_full, target_base[NAME_MAX + 1];

	char dofile_rel[PATH_MAX];

	char redofile[NAME_MAX + 1];
	char lockfile[NAME_MAX + 1];

	int uprel, lock_fd, dep_err = 0, wanted = 1, hint;

	struct stat redo_st = {0};

	FILE *fredo;


	if (strchr(dep_path, TRACK_DELIMITER)) {
		start_msg();
		dprintf(2, "Illegal symbol \'%c\' in  -- %s\n", TRACK_DELIMITER, dep_path);
		end_msg();
		return ERROR | BAD_NAME;
	}

	dep = file_chdir(dir_fd, dep_path);
	if (dep == 0) {
		start_msg();
		dprintf(2, "Missing dependency directory -- %s\n", dep_path);
		end_msg();
		return ERROR | BAD_NAME;
	}

	if (strlen(dep) > (sizeof target_base - sizeof target_prefix)) {
		start_msg();
		dprintf(2, "Dependency name too long -- %s\n", dep);
		end_msg();
		return ERROR | BAD_NAME;
	}

	target_full = track(dep, 1);
	if (target_full == 0) {
		start_msg();
		dprintf(2, "Dependency loop attempt -- %s\n", dep_path);
		end_msg();
		return wflag ? IS_SOURCE : ERROR;
	}

	strcpy(target_base, dep);
	strcpy(stpcpy(redofile, redo_prefix), dep);
	strcpy(stpcpy(lockfile, lock_prefix), dep);

	if (log_fd > 0)
		dprintf(log_fd, "%*s\"%s\",\n", nlevel * 2, "", target_full);

	if (strncmp(dep, redo_prefix, sizeof redo_prefix - 1) == 0)
		return IS_SOURCE;

	if (log_fd > 0)
		dprintf(log_fd, "--[[\n");

	dofile = find_dofile(target_base, dofile_rel, sizeof dofile_rel, &uprel, target_full);

	if (log_fd > 0)
		dprintf(log_fd, "--]]\n");

	if (!dofile)
		return IS_SOURCE;

	stat(redofile, &redo_st);

	if (strcmp(datestat(&redo_st), datebuild()) >= 0) {
		dep_err = (redo_st.st_mode & S_IRUSR) ? OK : ERROR;
		if (log_fd > 0)
			dprintf(log_fd, "%*s{ err = %d },\n", nlevel * 2, "", dep_err);

		return dep_err;
	}

	if (log_fd > 0)
		dprintf(log_fd, "%*s{\n", nlevel * 2, "");

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
		if (log_fd > 0) {
			dprintf(log_fd, "%*serr = %d,\n", nlevel * 2 + 2, "", dep_err);
			dprintf(log_fd, "%*s},\n", nlevel * 2, "");
		}
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
				dep_err = do_update_dep(*dir_fd, filename, nlevel + 1, &hint);
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
		dep_err = do_update_dep(*dir_fd, dofile_rel, nlevel + 1, &hint);
	}

	target_full = strrchr(track(0, 0), TRACK_DELIMITER) + 1;

	if (!dep_err && wanted) {
		lseek(lock_fd, 0, SEEK_SET);

		dep_err = write_dep(lock_fd, dofile_rel, 0, 0, hint);

		if (!dep_err) {
			start_msg();
			dep_err = run_script(*dir_fd, lock_fd, dofile_rel, dep,
						target_base, target_full, uprel);
			end_msg();

			if (!dep_err)
				dep_err = write_dep(lock_fd, dep, 0, 0, IS_SOURCE);
		}

		if (dep_err && (dep_err != BUSY)) {
			chmod(redofile, redo_st.st_mode & (~S_IRUSR));
			start_msg();
			dprintf(2, "redo %*s%s\n", nlevel * 2, "", target_full);
			dprintf(2, "     %*s%s -> %d\n", nlevel * 2, "", dofile_rel, dep_err);
			end_msg();
		}
	}

	close(lock_fd);

	if (log_fd > 0) {
		dprintf(log_fd, "%*serr = %d,\n", nlevel * 2 + 2, "", dep_err);
		dprintf(log_fd, "%*s},\n", nlevel * 2, "");
	}

/*
	Now we will use target_full residing in track to construct
	the lockfile_full. If fchdir() in do_update_target() failed
	then we need the full lockfile name.
*/

	strcpy(base_name(target_full, 0), lockfile);

	dep_err &= ~IMMEDIATE_DEPENDENCY;

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
			u = stpcpy(u, updir);
		}
	}
}


static int
keepdir()
{
	int fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		pperror("dir open");
		exit(-1);
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

#define SHORTEST 10
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


int
main(int argc, char *argv[])
{
	int opt, lock_fd = -1;

	const char *dirprefix;
	char updir[PATH_MAX];
	int level, main_dir_fd;
	int deps_todo, deps_done = 0, retries, attempts;

	int *dep_status, i;


	if (strcmp(base_name(argv[0], 0), "redo") != 0) {
		lock_fd = envint("REDO_LOCK_FD");
	}

	opterr = 0;

	while ((opt = getopt(argc, argv, "+dxwl:")) != -1) {
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
		case 'l':
			if (strcmp(optarg, "1") == 0) {
				setenvfd("REDO_LOG_FD", 1);
			} else if (strcmp(optarg, "2") == 0) {
				setenvfd("REDO_LOG_FD", 2);
			} else {
				setenvfd("REDO_LOG_FD", open(optarg, O_CREAT | O_WRONLY | O_TRUNC, 0666));
			}
			break;
		default:
			dprintf(2, "Usage: redo [-dxw] [-l <logname>] [TARGETS...]\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		return 0;


	deps_todo = argc;

	dep_status = malloc(deps_todo * sizeof (int));
	if (!dep_status) {
		perror("malloc");
		exit(-1);
	}


	for (i = 0; i < deps_todo; i++)
		dep_status[i] = BUSY;

	xflag = envint("REDO_TRACE");
	wflag = envint("REDO_WARNING");
	dflag = envint("REDO_DOFILES");

	log_fd = envint("REDO_LOG_FD");

	dirprefix = getenv("REDO_DIRPREFIX");
	compute_updir(dirprefix, updir);

	level = occurrences(track(getenv("REDO_TRACK"), 0), TRACK_DELIMITER);

	if (level > 0)
		end_msg();

	retries = envint("REDO_RETRIES");
	unsetenv("REDO_RETRIES");

	if ((lock_fd < 0) && (retries == 0))
		retries = RETRIES_DEFAULT;
	attempts = retries + 1;

	datebuild();

	main_dir_fd = keepdir();

	srand(getpid());

	do {
		hurry_up_if(attempts >= retries);

		for (i = 0; i < argc ; i++) {
			if (dep_status[i] != OK) {
				int redo_err, hint;

				redo_err = do_update_dep(main_dir_fd, argv[i], level, &hint);

				if ((redo_err == 0) && (lock_fd > 0))
					redo_err = write_dep(lock_fd, argv[i], dirprefix, updir, hint);

				if (redo_err == 0) {
					dep_status[i] = OK;
					deps_done++;
					attempts = retries + 1;
				} else if (redo_err == BUSY) {
					if (hint & IMMEDIATE_DEPENDENCY) {
						dep_status[i] = OK;
						deps_todo--;		/* forget it */
					}
				} else {
					if (level > 0)
						start_msg();
					return redo_err;
				}

			}
		}
	} while ((deps_done < deps_todo) && (--attempts > 0));

	if (level > 0)
		start_msg();

	return (deps_done < argc) ? BUSY : OK;
}

