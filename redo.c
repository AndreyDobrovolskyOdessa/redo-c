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

/***********************************************************

This redo-c version can be found at:

	https://github.com/AndreyDobrovolskyOdessa/redo-c

which is the fork of:

	https://github.com/leahneukirchen/redo-c


	Features:

- Optimized recipes envocations.
- Dependency loops could be detected during parallel builds.
- Build log is Lua table.


Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>

***********************************************************/

#define _GNU_SOURCE 1

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
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


/* -------------- Globals --------------- */

static int wflag, eflag, fflag, tflag, log_fd, level;

static struct {
	char		*buf;
	size_t		size;
	unsigned int	used;
} track;

#define HASH_LEN	32
#define HEXHASH_LEN	(2 * HASH_LEN)
#define HEXDATE_LEN	16

#define DATE_OFFSET	(HEXHASH_LEN + 1)
#define NAME_OFFSET	(DATE_OFFSET + HEXDATE_LEN + 1)

#define RECORD_SIZE	(NAME_OFFSET + PATH_MAX + 1)

static char record_buf[RECORD_SIZE];

#define hexhash (record_buf)
#define hexdate (record_buf + DATE_OFFSET)
#define namebuf (record_buf + NAME_OFFSET)

static char build_date[HEXDATE_LEN + 1];

/* ------------- Literals --------------- */

static const char

	recipe_suffix[]  = ".do",

	journal_prefix[] = ".do..",
	draft_prefix[]	 = ".do...do..",
	tmp_prefix[]	 = ".do...do...do..",

	dirup[] = "../",

	open_comment[]	= "--[====================================================================[\n",
	close_comment[]	= "--]====================================================================]\n";

/* -------------------------------------- */


#define log_guard(str)	if ((log_fd > 0) && (log_fd < 3)) dprintf(log_fd, str)

static void
msg(const char *name, const char *error)
{
	log_guard(open_comment);
	dprintf(2, "%s : %s\n", name, error);
	log_guard(close_comment);
}


static void
pperror(const char *s)
{
	msg(s, strerror(errno));
}


#define TRACK_DELIM ':'


static void
track_init(char *heritage)
{
	track.used = heritage ? strlen(heritage) : 0;
	track.size = PATH_MAX + track.used;
	track.buf = malloc(track.size);
	if (!track.buf) {
		perror("malloc");
		exit (-1);
	}
	strcpy(track.buf, heritage ? heritage : "");
}


static void
track_truncate(unsigned int cutoff)
{
	track.used = cutoff;
	track.buf[cutoff] = 0;
}


static char *
track_append(char *dep)
{
	size_t track_engaged, record_len;
	char *record, *dep_full, *ptr;


	/* store cwd in the track.buf */

	track_engaged = track.used

			+ 1			/* TRACK_DELIM */

						/* dep's directory */

			+ 1			/* '/' */

			+ strlen(dep)		/* dep */

			+ sizeof draft_prefix	/* is to be reserved in order
						to have enough free space to
						construct the draft_full
						later in really_update_dep() */

			+ 1 ;			/* terminating '\0', actually
						is not necessary, because
						sizeof draft_prefix already
						took care of it :-) */

	while (1) {
		if (track.size > track_engaged) {
			dep_full = getcwd(track.buf + track.used + 1,
					     track.size - track_engaged);

			if (dep_full)		/* getcwd successful */
				break;
		} else
			errno = ERANGE;

		if (errno == ERANGE) {		/* track.size not sufficient */
			track.size += PATH_MAX;
			track.buf = realloc(track.buf, track.size);
			if (!track.buf) {
				pperror("realloc");
				return 0;
			}
		} else {
			pperror ("getcwd");
			return 0;
		}
	}


	/* construct the dep's record and join it with the track */

	record = track.buf + track.used;

	*record = TRACK_DELIM;

	record_len = stpcpy(stpcpy(strchr(record, '\0'), "/"), dep) - record;

	track.used += record_len;


	/* search for the dep's full path inside track.buf */

	ptr = track.buf;

	do {
		ptr = strstr(ptr, record);
		if (ptr == record)
			return dep_full;
		ptr += record_len;
	} while (*ptr != TRACK_DELIM);

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


static char *
hashfile(int fd)
{
	static const char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
				   '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

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

static void
datestat(struct stat *st)
{
	snprintf(hexdate, HEXDATE_LEN + 1,
		"%0" stringize(HEXDATE_LEN) PRIx64, (uint64_t) st->st_ctime);
}


static void
datefile(int fd)
{
	struct stat st = {0};

	fstat(fd, &st);
	datestat(&st);
}


static void
datefilename(const char *name)
{
	struct stat st = {0};

	stat(name, &st);
	datestat(&st);
}


static void
datebuild(const char *s)
{
	FILE *f;

	if (s) {
		memcpy(build_date, s, HEXDATE_LEN);
		return;
	}

	f = tmpfile();
	datefile(fileno(f));
	fclose(f);

	memcpy(build_date, hexdate, HEXDATE_LEN);

	setenv("REDO_BUILD_DATE", build_date, 1);
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


#define SUFFIX_LEN	(sizeof recipe_suffix - 1)

#define reserve(space)	if (recipe_free < (space)) return -1;\
			recipe_free -= (space)

static int
find_recipe(char *dep, char *recipe_rel, size_t recipe_free, const char *slash)
{
	char *recipe = recipe_rel;

	char *end  = strchr(dep, '\0');
	char *tail = end;
	char *ext, *trickpoint;

	size_t recipe_size = (end - dep) + sizeof recipe_suffix;

	int uprel;


	/* rewind .do tail inside dependency name */

	while (1) {
		ext = tail - SUFFIX_LEN;
		if ((ext < dep) || strncmp(ext, recipe_suffix, SUFFIX_LEN))
			break;

		/* if we are still here means the dep is a recipe */

		if (!eflag)
			return -1;

		tail = ext;
	}

	trickpoint = strstr(dep, ".do.");
	if (trickpoint == tail)
		trickpoint = 0;

	reserve(recipe_size);		/* initial recipe name size */
	strcpy(end, recipe_suffix);

/*
	The full dependency name is used for recipe search only in the
	current directory. The search in the updirs omits the first name.
	Omitting increases the free space for recipe_rel and is done only
	once, that's why it can be done preliminarily.
*/

	ext = strchr(dep, '.');
	recipe_free += ext - dep;


	for (uprel = 0 ; slash ; uprel++, slash = strchr(slash + 1, '/')) {

		while (dep != trickpoint) {
			strcpy(recipe, dep);

			if (fflag)
				dprintf(1, "%s\n", recipe_rel);

			if (access(recipe_rel, F_OK) == 0) {
				*dep = '\0';
				return uprel;
			}

			if (dep == tail)
				break;

			while (*++dep != '.');
		}

		dep = ext;		/* omit the first name */

		reserve(sizeof dirup - 1);
		recipe = stpcpy(recipe, dirup);
	}

	return -1;
}


enum errors {
	OK	= 0,
	ERROR	= 1,
	BUSY	= EX_TEMPFAIL
};

#define ERRORS 0xff


enum hints {
	IS_SOURCE		= 0x100,
	UPDATED_RECENTLY	= 0x200,
	IMMEDIATE_DEPENDENCY	= 0x400
};

#define HINTS (~ERRORS)


static int
choose(const char *old, const char *new, int err, void (*perr_f)(const char *))
{
	struct stat st;

	if (err) {
		if ((lstat(new, &st) == 0) && remove(new)) {
			(*perr_f)("remove new");
			err = ERROR;
		}
	} else {
		if ((lstat(old, &st) == 0) && remove(old)) {
			(*perr_f)("remove old");
			err = ERROR;
		}
		if ((lstat(new, &st) == 0) && rename(new, old)) {
			(*perr_f)("rename");
			err = ERROR;
		}
	}

	return err;
}


static int 
run_recipe(int dir_fd, int fd, char *recipe_rel, const char *target_rel,
		const char *family, size_t dirprefix_len)
{
	int err = ERROR;

	pid_t pid;

	const char *target = target_rel + dirprefix_len;

	char family_rel[PATH_MAX];

	char tmp_rel[PATH_MAX + sizeof tmp_prefix];
	char *tmp = tmp_rel + dirprefix_len;

	char dirprefix[PATH_MAX];


	if (strlen(target_rel) >= PATH_MAX) {
		dprintf(2, "Target relative name too long : %s\n", target_rel);
		return ERROR;
	}

	memcpy(dirprefix, target_rel, dirprefix_len);
	dirprefix[dirprefix_len] = '\0';

	strcpy(stpcpy(family_rel, dirprefix), family);

	strcpy(stpcpy(stpcpy(tmp_rel, dirprefix), tmp_prefix), target);

	pid = fork();
	if (pid < 0) {
		perror("fork");
	} else if (pid == 0) {

		const char *recipe = file_chdir(&dir_fd, recipe_rel);

		if (!recipe) {
			dprintf(2, "Damn! Someone have stolen my favorite recipe %s ...\n", recipe_rel);
			exit(ERROR);
		}

		if ((setenvfd("REDO_FD", fd) != 0) ||
		    (setenv("REDO_DIRPREFIX", dirprefix, 1) != 0) ||
		    (setenv("REDO_TRACK", track.buf, 1) != 0)) {
			perror("setenv");
			exit(ERROR);
		}

		if (access(recipe, X_OK) != 0)	/* run -x files with /bin/sh */
			execl("/bin/sh", "/bin/sh", tflag ? "-ex" : "-e",
			      recipe,
				target_rel, family_rel, tmp_rel, (char *)0);
		else
			execl(recipe, recipe,
				target_rel, family_rel, tmp_rel, (char *)0);

		perror("execl");
		exit(ERROR);
	} else {
		if (wait(&err) < 0) {
			perror("wait");
		} else {
			if (WCOREDUMP(err)) {
				dprintf(2, "Core dumped.\n");
			}
			if (WIFEXITED(err)) {
				err = WEXITSTATUS(err);
			} else if (WIFSIGNALED(err)) {
				err = WTERMSIG(err);
				dprintf(2, "Terminated.\n");
			} else if (WIFSTOPPED(err)) {
				err = WSTOPSIG(err);
				dprintf(2, "Stopped.\n");
			}
		}
	}

	return choose(target, tmp, err, perror);
}


static int
valid(char *record, char *journal)
{
	char *last = strchr(record, '\0') - 1;

	if (((last - record) < NAME_OFFSET) || (*last != '\n')) {
		msg(journal, "is truncated (warning)");
		return 0;
	}

	*last = '\0';
	return 1;
}


static int
find_record(char *target_path)
{
	int err = ERROR;

	char journal_path[PATH_MAX + sizeof journal_prefix];
	char *target = base_name(target_path, 0);
	size_t dp_len = target - target_path;


	memcpy(journal_path, target_path, dp_len);
	strcpy(stpcpy(journal_path + dp_len, journal_prefix), target);

	FILE *f = fopen(journal_path, "r");

	if (f) {
		while (fgets(record_buf, RECORD_SIZE, f) &&
			valid(record_buf, journal_path))
		{
			if (strcmp(target, namebuf) == 0) {
				err = OK;
				break;
			}
		}

		fclose(f);
	}

	return err;
}


#define may_need_rehash(f,h)	(\
	(h & IS_SOURCE) || (\
		(!(h & UPDATED_RECENTLY)) &&\
		find_record(f)\
	)\
)

static int
dep_changed(char *record, int hint)
{
	char *filename = record + NAME_OFFSET;
	char *filedate = record + DATE_OFFSET;
	int fd;

	if (may_need_rehash(filename, hint)) {
		datefilename(filename);
		if (strncmp(filedate, hexdate, HEXDATE_LEN) == 0)
			return 0;

		fd = open(filename, O_RDONLY);
		hashfile(fd);
		close(fd);
	} else {
		if (strncmp(filedate, hexdate, HEXDATE_LEN) == 0)
			return 0;
	}

	return strncmp(record, hexhash, HEXHASH_LEN);
}


static int
write_dep(int fd, char *dep, const char *dirprefix, const char *updir, int hint)
{
	const char *prefix = "";


	if (may_need_rehash(dep, hint)) {
		int dep_fd = open(dep, O_RDONLY);
		hashfile(dep_fd);
		datefile(dep_fd);
		if (dep_fd > 0)
			close(dep_fd);
	}

	if (dirprefix && *dep != '/') {
		size_t dp_len = strlen(dirprefix);

		if (strncmp(dep, dirprefix, dp_len) == 0)
			dep += dp_len;
		else
			prefix = updir;
	}

	*(hexhash + HEXHASH_LEN) = '\0';
	*(hexdate + HEXDATE_LEN) = '\0';

	if (dprintf(fd, "%s %s %s%s\n", hexhash, hexdate, prefix, dep) < 0) {
		pperror("dprintf");
		return ERROR;
	}

	return OK;
}


#define INDENT 2

#define NAME_MAX 255


static int really_update_dep(int dir_fd, char *dep);


static int
update_dep(int dir_fd, char *dep_path, int *hint)
{
	int dep_dir_fd = dir_fd, err = ERROR;

	level += INDENT;

	do {
		char *dep;
		unsigned int origin = track.used;

		if (strchr(dep_path, TRACK_DELIM)) {
			msg(dep_path, "Illegal symbol " stringize(TRACK_DELIM));
			break;
		}

		dep = file_chdir(&dep_dir_fd, dep_path);
		if (dep == 0) {
			msg(dep_path, "Missing dependency directory");
			break;
		}

		if (strlen(dep) > (NAME_MAX + 1 - sizeof tmp_prefix)) {
			msg(dep, "Dependency name too long");
			break;
		}

		err = really_update_dep(dep_dir_fd, dep);
		track_truncate(origin);	/* strip the fresh new record */

	} while (0);

	if (dir_fd != dep_dir_fd) {
		if (fchdir(dir_fd) < 0) {
			pperror("chdir back");
			err |= ERROR;
		}
		close(dep_dir_fd);
	}

	*hint = err & HINTS;

	level -= INDENT;

	return err & ERRORS;
}


static long
process_times(void)
{
	struct tms t;

	times(&t);

	return t.tms_utime + t.tms_stime + t.tms_cutime + t.tms_cstime;
}


#define log_name(name) if (log_fd > 0)\
	dprintf(log_fd, "%*s\"%s\",\n", level, "", name);

#define log_err() if (log_fd > 0)\
	dprintf(log_fd, "%*s{ err = %d },\n", level, "", err)

#define log_time(format) if (log_fd > 0)\
	dprintf(log_fd, "%*s" format "\n", level, "", process_times())

#define log_close_level(format) if (log_fd > 0)\
	dprintf(log_fd, "%*s" format "\n%*s},\n",\
			level, "", process_times(), err, level, "")


static int
really_update_dep(int dir_fd, char *dep)
{
	char *whole, *target_rel;
	char family[NAME_MAX + 1];

	char recipe_rel[PATH_MAX];

	char journal[NAME_MAX + 1];
	char draft[NAME_MAX + 1];

	int uprel, fd, err = 0, wanted = 1, hint, is_recipe = 1;

	struct stat st = {0};

	FILE *f;

	unsigned int	whole_pos = track.used + 1,
			target_rel_off,
			dirprefix_len;


	whole = track_append(dep);
	if (whole == 0) {
		msg(track.buf, "Dependency loop attempt");
		return wflag ? IS_SOURCE : ERROR;
	}

	log_name(whole);

	if (fflag)
		dprintf(1, "--[[\n");

	strcpy(family, dep);
	uprel = find_recipe(family, recipe_rel, sizeof recipe_rel, whole);

	if (fflag)
		dprintf(1, "--]]\n");

	if (uprel < 0)
		return IS_SOURCE;

	target_rel = base_name(whole, uprel);
	target_rel_off = target_rel - whole;
	dirprefix_len = strlen(target_rel) - strlen(dep);

	strcpy(stpcpy(journal, journal_prefix), dep);
	stat(journal, &st);
	datestat(&st);

	if (strcmp(hexdate, build_date) >= 0) {
		err = (st.st_mode & S_IRUSR) ? OK : ERROR;
		log_err();
		return err;
	}

	strcpy(stpcpy(draft, draft_prefix), dep);
	fd = open(draft, O_CREAT | O_WRONLY | O_EXCL, 0666);
	if (fd < 0) {
		if (errno == EEXIST) {
			err = BUSY | IMMEDIATE_DEPENDENCY;
		} else {
			pperror("open exclusive");
			err = ERROR;
		}
		log_err();
		return err;
	}


	log_time("{       t0 = %ld,");

	f = fopen(journal, "r");

	if (f) {
		char record[RECORD_SIZE];
		char *filename = record + NAME_OFFSET;

		while (fgets(record, RECORD_SIZE, f) && valid(record, journal)) {
			int self = !strcmp(filename, dep);

			if (is_recipe) {
				if (strcmp(filename, recipe_rel))
					break;
				is_recipe = 0;
			}

			if (self)
				hint = IS_SOURCE;
			else
				err = update_dep(dir_fd, filename, &hint);

			if (err || dep_changed(record, hint))
				break;

/*
			The next memcpy() call restores the correct hexhash for
			write_dep(), handling the case of the matching dates
			not followed with dependency rehashing.
*/

			memcpy(hexhash, record, HEXHASH_LEN);

			err = write_dep(fd, filename, 0, 0, UPDATED_RECENTLY);
			if (err)
				break;

			if (self) {
				wanted = 0;
				break;
			}
		}

		fclose(f);
		hint = 0;
	}

	if (!f || is_recipe)
		err = update_dep(dir_fd, recipe_rel, &hint);

/*
	track.buf may be relocated during the nested update_dep() calls.
	whole and target_rel reside in it and need to be refreshed.
*/

	whole = track.buf + whole_pos;
	target_rel = whole + target_rel_off;

	if (!err && wanted) {
		lseek(fd, 0, SEEK_SET);

		err = write_dep(fd, recipe_rel, 0, 0, hint);

		if (!err) {
			log_time("             %ld, -- tdo");
			log_guard(open_comment);
			err = run_recipe(dir_fd, fd, recipe_rel, target_rel,
					family, dirprefix_len);
			log_guard(close_comment);

			if (!err)
				err = write_dep(fd, dep, 0, 0, IS_SOURCE);
		}

		if (err && (err != BUSY)) {
			chmod(journal, st.st_mode & (~S_IRUSR));
			log_guard(open_comment);
			dprintf(2, "redo %*s%s\n", level, "", whole);
			dprintf(2, "     %*s%s -> %d\n", level, "", recipe_rel, err);
			log_guard(close_comment);
		}
	}

	close(fd);

	log_close_level("        t1 = %ld, err  = %d");

/*
	If fchdir() in update_dep() failed then we need to create
	the full draft name inside the whole dep path.
*/

	strcpy(target_rel + dirprefix_len, draft);

	return choose(journal, whole, err, pperror) | UPDATED_RECENTLY;
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


#define SHORTEST	10 /* ms */
#define SCALEUPS	6
#define LONGEST		(SHORTEST << SCALEUPS)

#define MS_PER_S	1000
#define NS_PER_MS	1000000

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

	asleep = night + (rand() % night);

	if (night < LONGEST)
		night *= 2;

	s.tv_sec  =  asleep / MS_PER_S;
	s.tv_nsec = (asleep % MS_PER_S) * NS_PER_MS;

	nanosleep(&s, &r);
}


static void
fence(int log_fd_buf, const char *top, const char *hill)
{
	if (log_fd > 0) {
		if (log_fd != log_fd_buf) {
			if (log_fd_buf <= 0)
				dprintf(log_fd, top);
		} else {
			if (log_fd < 3)
				dprintf(log_fd, hill);
		}
	}
}


struct roadmap {
	size_t	size;
	int	num;
	int	todo;
	int	done;
	char	**name;
	int32_t *status;
	int32_t *children;
	int32_t *child;
};


static int
text2int(int32_t *x, int n, char **p)
{
	char *ep;
	long v;

	while (n-- > 0) {
		v = strtol(*p, &ep, 10);
		if ((ep - *p) < (int) (sizeof (int32_t)))
			break;
		*p = ep;
		*x++ = (int32_t) v;
	}

	return n != -1;
}


static int
text2name(char **x, int n, char **p)
{
	while (n-- > 0) {
		*p = strchr(*p, '\n');
		if (*p == 0)
			break;
		*(*p)++ = '\0';
		*x++ = *p;
	}

	return n != -1;
}


static int
test_map(struct roadmap *m)
{
	int i, c, n = m->num, last = m->children[0];


	if (last != 0)
		return ERROR;

	for (i = 0; i < n; i++)
		m->status[i] = 0;

	for (i = 1; i <= n; i++) {
		c = m->children[i];
		if (c < last)
			return ERROR;
		last = c;
	}

	for (i = 0; i < last; i++) {
		c = m->child[i];
		if ((c < 0) || (c >= n))
			return ERROR;
		m->status[c]++;
	}

	return OK;
}


static int
import_map(struct roadmap *m, int fd)
{
	struct stat st;
	int num;
	char *ptr;

	if (fstat(fd, &st) < 0)
		return ERROR;

	if (st.st_size == 0)
		return ERROR;

	m->name = mmap(NULL, st.st_size + 1, PROT_READ | PROT_WRITE,
					MAP_PRIVATE, fd, 0);
	close(fd);
	if (m->name == MAP_FAILED)
		return ERROR;

	m->size = st.st_size + 1;

	ptr = (char *) m->name;

	ptr[st.st_size] = '\0';

	ptr = strchr(ptr, '\n');
	if (ptr == 0)
		return ERROR;

	num = (ptr - (char *)m->name) / 8;

	m->num  = num;
	m->todo = num;
	m->done = 0;

	m->status = (int32_t *) ptr;
	m->children = m->status + num;
	m->child = m->children + num + 1;

	if (text2int(m->status, num, &ptr) ||
	    text2int(m->children, num + 1, &ptr) ||
	    text2int(m->child, m->children[num], &ptr) ||
	    text2name(m->name, num, &ptr))
		return ERROR;

	return test_map(m);
}


static void
init_map(struct roadmap *m, int n, char **argv)
{
	m->num  = n;
	m->todo = n;
	m->done = 0;
	m->name = argv;

	m->status = calloc(2 * n + 1, sizeof (int32_t));
	if (m->status == 0) {
		perror("calloc");
		exit(ERROR);
	}

	m->children = m->status + n;
}


static void
approve(struct roadmap *m, int i)
{
	int own = m->children[i];
	int num = m->children[i + 1] - own;

	int32_t *ch = m->child + own;

	m->status[i] = -1;
	m->done++;

	while (num--)
		m->status[*ch++]--;
}


static int
forget(struct roadmap *m, int i)
{
	int own = m->children[i];
	int num = m->children[i + 1] - own;

	if (num > 1)
		return 0;

	if (num == 1) {
		int child = m->child[own];

		if ((m->status[child] > 1) || (forget(m, child) == 0))
			return 0;
	}

	m->status[i] = -1;
	m->todo--;

	return 1;
}


#define RETRIES_DEFAULT 10

int
main(int argc, char *argv[])
{
	int opt, log_fd_prev, fd = -1, map_fd;
	int passes_max, passes, i, err = OK;

	struct roadmap dep = {.size = 0};

	int dir_fd = keepdir();
	const char *dirprefix = getenv("REDO_DIRPREFIX");
	char updir[PATH_MAX];


	log_fd = log_fd_prev = envint("REDO_LOG_FD");

	opterr = 0;

	while ((opt = getopt(argc, argv, "+weftdrxl:m:")) != -1) {
		switch (opt) {
		case 'w':
			setenvfd("REDO_WARNING", 1);
			break;
		case 'e':
		case 'd':
			setenvfd("REDO_RECIPES", 1);
			break;
		case 'f':
		case 'r':
			setenvfd("REDO_FIND", 1);
			break;
		case 't':
		case 'x':
			setenvfd("REDO_TRACE", 1);
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
			map_fd = open(optarg, O_RDONLY);
			if (map_fd < 0) {
				dprintf(2, "Warning: unable to open roadmap : %s.\n", optarg);
			} else {
				if (import_map(&dep, map_fd) == OK) {
					file_chdir(&dir_fd, optarg);
					dirprefix = 0;
				} else {
					dprintf(2, "Error reading roadmap: %s\n", optarg);
					return ERROR;
				}
			}
			break;
		default:
			dprintf(2, "Usage: redo [-weft] [-l <logname>] [-m <roadmap>] [TARGET [...]]\n");
			return ERROR;
		}
	}

	wflag = envint("REDO_WARNING");
	eflag = envint("REDO_RECIPES");
	fflag = envint("REDO_FIND");
	tflag = envint("REDO_TRACE");


	if (dep.size == 0)
		init_map(&dep, argc - optind, argv + optind);


	compute_updir(dirprefix, updir);

	track_init(getenv("REDO_TRACK"));
	level = occurrences(track.buf, TRACK_DELIM) * INDENT;


	passes_max = envint("REDO_RETRIES");
	unsetenv("REDO_RETRIES");

	if (strcmp(base_name(argv[0], 0), "redo") == 0) {
		if (passes_max == 0)
			passes_max = RETRIES_DEFAULT;
	} else
		fd = envint("REDO_FD");

	passes = passes_max;


	datebuild(getenv("REDO_BUILD_DATE"));

	srand(getpid());

	fence(log_fd_prev, "return {\n", close_comment);

	do {
		hurry_up_if(passes-- == passes_max);

		for (i = 0; i < dep.num ; i++) {
			if (dep.status[i] == 0) {
				int hint;

				err = update_dep(dir_fd, dep.name[i], &hint);

				if ((err == OK) && (fd > 0))
					err = write_dep(fd, dep.name[i],
							dirprefix, updir, hint);

				if (err == OK) {
					approve(&dep, i);
					if (passes_max > 0) {
						passes = passes_max;
						break;
					}
				} else if (err == BUSY) {
					if (hint & IMMEDIATE_DEPENDENCY)
						forget(&dep, i);
				} else {
					err = ERROR;
					break;
				}
			}
		}
	} while ((err != ERROR) && (dep.done < dep.todo) && (passes > 0));

	fence(log_fd_prev, "}\n", open_comment);

	if (err != ERROR)
		err = (dep.done < dep.num) ? BUSY : OK;

	return err; 
}

