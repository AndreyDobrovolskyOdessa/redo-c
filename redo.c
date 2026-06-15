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
- Build log is a Lua table.


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


/********************* Globals *********************************************/

static int wflag, eflag, fflag, tflag, log_fd, indent;

static struct {
	char	*buf;
	size_t	size, used;
} track;

#define HASH_LEN	32
#define HEXHASH_LEN	(2 * HASH_LEN)
#define HEXDATE_LEN	16

#define DATE_OFFSET	(HEXHASH_LEN + 1)
#define NAME_OFFSET	(DATE_OFFSET + HEXDATE_LEN + 1)

#define RECORD_SIZE	(NAME_OFFSET + PATH_MAX + 1)

static char record_buf[RECORD_SIZE], build_date[HEXDATE_LEN + 1];

#define hexhash (record_buf)
#define hexdate (record_buf + DATE_OFFSET)
#define namebuf (record_buf + NAME_OFFSET)


/****************** Literals ***********************************************/

static const char

	recipe_suffix[]  = ".do",

	journal_prefix[] = ".do..",
	draft_prefix[]	 = ".do...do..",
	tmp_prefix[]	 = ".do...do...do..",

	dirup[] = "../",

	open_comment[]	=

"--[====================================================================[\n",

	close_comment[]	=

"--]====================================================================]\n";



#define log_guard(s)	if ((log_fd > 0) && (log_fd < 3)) dprintf(log_fd, s)

static void
msg(const char *x, const char *y)
{
	log_guard(open_comment);
	dprintf(2, "%s : %s\n", x, y);
	log_guard(close_comment);
}


static void
pperror(const char *s)
{
	msg(s, strerror(errno));
}


#define TRACK_DELIM ':'

#define INDENT_PER_LEVEL 2

static void
track_init(const char *heritage)
{
	char *s;

	if (!heritage)
		heritage = "";
	track.used = strlen(heritage);
	track.size = track.used + PATH_MAX;
	track.buf = malloc(track.size);
	if (!track.buf) {
		perror("malloc");
		exit(-1);
	}
	strcpy(track.buf, heritage);

	for(indent = 0, s = track.buf ; *s ; ) {
		if (*s++ == TRACK_DELIM)
			indent += INDENT_PER_LEVEL;
	}
}


static void
track_truncate(size_t pos)
{
	track.used = pos;
	track.buf[pos] = '\0';
}


static char *
track_append(const char *dep)
{
	char *record, *dep_full, *ptr;

	size_t record_len, track_engaged = track.used

		+ 1			/* TRACK_DELIM */
					/* dep's directory */
		+ 1			/* '/' */
		+ sizeof draft_prefix	/* is to be reserved in order to have
					enough free space to construct the
					draft_full in really_update_dep() */
		+ strlen(dep)		/* dep */
		+ 1;			/* terminating '\0' */


	/* store cwd in the track */

	while (1) {
		if (track.size > track_engaged) {
			dep_full = getcwd(track.buf + track.used + 1,
					     track.size - track_engaged);

			if (dep_full)		/* getcwd successful */
				break;
		} else
			errno = ERANGE;

		do {
			if (errno != ERANGE)
				pperror("getcwd");
			else if (track.size < (SIZE_MAX - PATH_MAX)) {
				size_t new_size = track.size + PATH_MAX;
				char  *new_buf = realloc(track.buf, new_size);

				if (new_buf) {
					track.size = new_size;
					track.buf  = new_buf;
					break;
				}
				pperror("realloc");
			}

			msg("Failed to get cwd", dep);

			wflag = 0;	/* suppress warning, ensure error */
			return 0;

		} while(0);
	}


	/* construct the dep's record and join it with the track */

	record = dep_full - 1;
	*record = TRACK_DELIM;
	record_len = stpcpy(stpcpy(strchr(record, 0), "/"), dep) - record;
	track.used += record_len;


	/* search for the dep's full path inside the track.buf */

	ptr = track.buf;

	do {
		ptr = strstr(ptr, record);
		if (ptr == record)
			return dep_full;
		ptr += record_len;
	} while (*ptr != TRACK_DELIM);

	return 0;
}


#define track_buf()	(track.buf + 0)

#define track_used()	(track.used + 0)


static char *
base_name(char *path)
{
	char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}


static int
setenvint(const char *name, int i)
{
	char buf[32];

	snprintf(buf, sizeof buf, "%d", i);

	return setenv(name, buf, 1);
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
datefd(int fd)
{
	struct stat st;

	if (fstat(fd, &st))
		st.st_ctime = 0;
	datestat(&st);
}


static void
datefile(const char *name, struct stat *st)
{
	if(stat(name, st))
		st->st_ctime = 0;
	datestat(st);
}


static void
date_build(const char *var)
{
	const char *s = getenv(var);
	FILE *f;

	if (s) {
		memcpy(build_date, s, HEXDATE_LEN);
		return;
	}

	f = tmpfile();
	datefd(fileno(f));
	fclose(f);

	strcpy(build_date, hexdate);

	setenv(var, build_date, 1);
}


static void
rehash(char *dep, int redate)
{
	static const char hexdigit[] = "0123456789abcdef";

	struct sha256 ctx;
	char buf[4096];
	char *a;
	unsigned char hash[HASH_LEN];
	int i, fd = open(dep, O_RDONLY);
	ssize_t r;


	sha256_init(&ctx);

	while ((r = read(fd, buf, sizeof buf)) > 0) {
		sha256_update(&ctx, buf, r);
	}

	sha256_sum(&ctx, hash);

	for (i = 0, a = hexhash; i < HASH_LEN; i++) {
		*a++ = hexdigit[hash[i] / 16];
		*a++ = hexdigit[hash[i] % 16];
	}

	if (redate)
		datefd(fd);

	if (fd > 0)
		close(fd);
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

#define reserve(space)	if (recipe_free < (space)) return 0;\
			recipe_free -= (space)

static char *
find_recipe(char *dep, char *recipe_rel, size_t recipe_free, const char *slash)
{
	char	*recipe = recipe_rel,
		*end  = strchr(dep, 0),
		*tail = end,
		*ext, *shadow;

	size_t recipe_size = (end - dep) + sizeof recipe_suffix;


	/* rewind .do tail inside dependency name */

	while (1) {
		ext = tail - SUFFIX_LEN;
		if ((ext < dep) || strncmp(ext, recipe_suffix, SUFFIX_LEN))
			break;

		/* if we are still here means the dep is a recipe */

		if (!eflag)
			return 0;

		tail = ext;
	}

	shadow = strstr(dep, ".do.");
	if (shadow == tail)
		shadow = 0;

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


	for ( ; slash ; slash = strchr(slash + 1, '/')) {

		while (dep != shadow) {
			strcpy(recipe, dep);

			if (fflag)
				dprintf(1, "%s\n", recipe_rel);

			if (access(recipe_rel, F_OK) == 0) {
				*dep = '\0';
				return recipe;
			}

			if (dep == tail)
				break;

			while (*++dep != '.');
		}

		dep = ext;		/* omit the first name */

		reserve(sizeof dirup - 1);
		recipe = stpcpy(recipe, dirup);
	}

	return 0;
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
choose(const char *old, const char *new, int err)
{
	struct stat st;

	if (err) {
		if ((lstat(new, &st) == 0) && remove(new)) {
			pperror("remove new");
			err = ERROR;
		}
	} else {
		if ((lstat(old, &st) == 0) && remove(old)) {
			pperror("remove old");
			err = ERROR;
		}
		if ((lstat(new, &st) == 0) && rename(new, old)) {
			pperror("rename");
			err = ERROR;
		}
	}

	return err;
}


static long
process_times(void)
{
	struct tms t;

	times(&t);

	return t.tms_utime + t.tms_stime + t.tms_cutime + t.tms_cstime;
}


#define NAME_MAX 255

#define log_time(format) if (log_fd > 0)\
	dprintf(log_fd, "%*s" format "\n", indent, "", process_times())

static int
run_recipe(int fd, char *recipe_rel, const char *target,
				const char *family, size_t reldir_len)
{
	int err = ERROR;

	pid_t pid;

	char	reldir[PATH_MAX],
		tmp[NAME_MAX + 1];


	log_time("             %ld, -- tdo");
	log_guard(open_comment);

	memcpy(reldir, recipe_rel, reldir_len);
	reldir[reldir_len] = '\0';

	strcpy(stpcpy(tmp, tmp_prefix), target);

	pid = fork();
	if (pid < 0)
		perror("fork");
	else if (pid == 0) {

		if (setenvint("REDO_FD", fd) ||
		    setenv("REDO_TRACK", track_buf(), 1)) {
			perror("setenv");
			exit(ERROR);
		}

		if (access(recipe_rel, X_OK) != 0) /* executable? */
			execl("/bin/sh", "/bin/sh",
				tflag ? "-ex" : "-e", recipe_rel,
				target, family, tmp, reldir, (char *)0);
		else
			execl(recipe_rel, recipe_rel,
				target, family, tmp, reldir, (char *)0);

		perror("execl");
		exit(ERROR);
	} else {
		if (wait(&err) < 0)
			perror("wait");
		else {
			if (WCOREDUMP(err))
				dprintf(2, "Core dumped.\n");
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

	log_guard(close_comment);

	return choose(target, tmp, err);
}


static int
read_record(char *buf, FILE *f, char *filename)
{
	if(fgets(buf, RECORD_SIZE, f)) {
		char *eol_ch = strchr(buf, '\n');

		if (eol_ch && ((eol_ch - buf) >= NAME_OFFSET)) {
			*eol_ch = '\0';
			return 1;
		}

		msg("Warning! Truncated record found", filename);
	}

	return 0;
}


static int
find_record(char *target_path)
{
	int err = ERROR;

	char	*target = base_name(target_path),
		journal[PATH_MAX + sizeof journal_prefix];

	size_t len = target - target_path;

	FILE *journal_f;


	memcpy(journal, target_path, len);
	strcpy(stpcpy(journal + len, journal_prefix), target);

	journal_f = fopen(journal, "r");

	if (journal_f) {
		while (read_record(record_buf, journal_f, journal)) {
			if (strcmp(target, namebuf) == 0) {
				err = OK;
				break;
			}
		}

		fclose(journal_f);
	}

	return err;
}


#define may_need_rehash(dep, hint) \
(\
	(hint & IS_SOURCE) ||\
	(\
		!(hint & UPDATED_RECENTLY) &&\
		(find_record(dep) != OK)\
	)\
)


static int
dep_changed(char *record, int hint)
{
	char	*filename = record + NAME_OFFSET,
		*filedate = record + DATE_OFFSET;

	struct stat st;
	int missing = may_need_rehash(filename, hint);


	if (missing)
		datefile(filename, &st);

	if (strncmp(filedate, hexdate, HEXDATE_LEN) == 0) {
/*
		Dependency date matching its journal record date means
		that dependency was not modified and the hash from its
		journal record can be forwarded to the global buffer
		to be used by write_dep().
*/
		memcpy(hexhash, record, HEXHASH_LEN);
		return 0;
	}

	if (missing)
		rehash(filename, 0);

	return strncmp(record, hexhash, HEXHASH_LEN);
}


static int
write_dep(int fd, char *dep, int hint)
{
	if (may_need_rehash(dep, hint))
		rehash(dep, 1);

	hexhash[HEXHASH_LEN] = '\0';
	hexdate[HEXDATE_LEN] = '\0';

	if (dprintf(fd, "%s %s %s\n", hexhash, hexdate, dep) < 0) {
		pperror("dprintf");
		return ERROR;
	}

	return OK;
}


static int really_update_dep(int dir_fd, char *dep);

static int
update_dep(int dir_fd, char *dep_path, int *hint)
{
	int dep_dir_fd = dir_fd, err = ERROR;


	if (strchr(dep_path, TRACK_DELIM))
		msg("Illegal symbol "stringize(TRACK_DELIM), dep_path);
	else {
		char *dep = file_chdir(&dep_dir_fd, dep_path);

		if (!dep)
			msg("Missing dependency directory", dep_path);
		else if (strlen(dep) > (NAME_MAX + 1 - sizeof tmp_prefix))
			msg("Dependency name too long", dep);
		else {
			size_t cutoff = track_used();

			indent += INDENT_PER_LEVEL;
			err = really_update_dep(dep_dir_fd, dep);
			indent -= INDENT_PER_LEVEL;
			track_truncate(cutoff);
		}

		if (dir_fd != dep_dir_fd) {
			if (fchdir(dir_fd) < 0) {
				pperror("chdir back");
				err = ERROR;
			}
			close(dep_dir_fd);
		}
	}

	*hint = err & HINTS;

	return err & ERRORS;
}


#define log_name() if (log_fd > 0)\
	dprintf(log_fd, "%*s\"%s\",\n", indent, "", whole);

#define log_err() if (log_fd > 0)\
	dprintf(log_fd, "%*s{ err = %d },\n", indent, "", err)

#define log_close_level() if (log_fd > 0)\
	dprintf(log_fd, "%*s        t1 = %ld, err = %d\n%*s},\n",\
			indent, "", process_times(), err, indent, "")


#define CR_WR_TR (O_CREAT | O_WRONLY | O_TRUNC)

static int
really_update_dep(int dir_fd, char *dep)
{
	char	*whole, *recipe,
		recipe_rel[PATH_MAX],
		journal[NAME_MAX + 1],
		draft  [NAME_MAX + 1],
		family [NAME_MAX + 1];

	int draft_fd, err = 0, up_to_date = 0, hint, new_recipe = 1;

	struct stat st;

	FILE *journal_f;

	size_t whole_pos = track_used() + 1, dep_pos;


	whole = track_append(dep);
	if (!whole) {
		msg("Dependency loop attempt", track_buf());
		return wflag ? IS_SOURCE : ERROR;
	}

	dep_pos = strlen(whole) - strlen(dep);

	log_name();

	if (fflag)
		dprintf(1, "--[[\n");

	strcpy(family, dep);
	recipe = find_recipe(family, recipe_rel, sizeof recipe_rel, whole);

	if (fflag)
		dprintf(1, "--]]\n");

	if (!recipe)
		return IS_SOURCE;


	strcpy(stpcpy(journal, journal_prefix), dep);
	datefile(journal, &st);

	if (strcmp(hexdate, build_date) >= 0) {
		err = (st.st_mode & S_IRUSR) ? OK : ERROR;
		log_err();
		return err;
	}


	strcpy(stpcpy(draft, draft_prefix), dep);
	draft_fd = open(draft, O_CREAT | O_WRONLY | O_EXCL, 0666);

	if (draft_fd < 0) {
		if (errno == EEXIST)
			err = BUSY | IMMEDIATE_DEPENDENCY;
		else {
			pperror("open exclusive");
			err = ERROR;
		}
		log_err();
		return err;
	}


	log_time("{       t0 = %ld,");

	journal_f = fopen(journal, "r");

	if (journal_f) {
		char record[RECORD_SIZE];
		char *filename = record + NAME_OFFSET;

		while (read_record(record, journal_f, journal)) {
			int self = !strcmp(filename, dep);

			hint = IS_SOURCE;

			if ((new_recipe &&
				(new_recipe = strcmp(filename, recipe_rel))) ||
			    (!self &&
				(err = update_dep(dir_fd, filename, &hint))) ||
			    dep_changed(record, hint) ||
			    (err = write_dep(draft_fd, filename,
							UPDATED_RECENTLY)) ||
			    (self && (up_to_date = 1)))
								break;
		}

		fclose(journal_f);
		hint = 0;
	}

	if (new_recipe)
		err = update_dep(dir_fd, recipe_rel, &hint);

/*
	track.buf may be relocated during the nested update_dep() calls.
	whole and target_rel reside in it and need to be refreshed.
*/
	whole = track_buf() + whole_pos;

	if (!err && !up_to_date) {
		lseek(draft_fd, 0, SEEK_SET);

		(void)(
			(err = write_dep(draft_fd, recipe_rel, hint)) ||
			(err = run_recipe(draft_fd, recipe_rel, dep,
					family, recipe - recipe_rel)) ||
			(err = write_dep(draft_fd, dep, IS_SOURCE))
		);

		if (err && (err != BUSY)) {
			if (journal_f)
				chmod(journal, st.st_mode & (~S_IRUSR));
			else
				close(open(journal, CR_WR_TR, 0222));
			log_guard(open_comment);
			dprintf(2, "redo %*s%s\n     %*s%s -> %d\n",
				indent,"", whole, indent,"", recipe_rel, err);
			log_guard(close_comment);
		}
	}

	close(draft_fd);

	log_close_level();

/*
	If fchdir() in update_dep() failed then we need to create
	the full draft name inside the whole dep path.
*/
	strcpy(whole + dep_pos, draft);

	return choose(journal, whole, err) | UPDATED_RECENTLY;
}


static int
envint(const char *name)
{
	char *s = getenv(name);

	return s ? strtol(s, 0, 10) : 0;
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


#define SHORTEST	10
#define SCALEUPS	6
#define LONGEST		(SHORTEST << SCALEUPS)

#define MS_PER_S	1000
#define NS_PER_MS	1000000

static void
hurry_up_on(int startup_or_success)
{
	static int night;
	int asleep;
	struct timespec s, r;


	if (startup_or_success) {
		night = SHORTEST;
		return;
	}

	asleep = night + (rand() % night); /* ms */

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


typedef struct {
	int	num,
		todo,
		done,
		sorted;
	int32_t *status,
		*children,
		*child;
	char	**name;
} roadmap;


static int
text2int(int32_t *x, int n, char **p)
{
	char *ep;
	long v;

	while (n-- > 0) {
		v = strtol(*p, &ep, 10);
		if ((ep - *p) < (int) (sizeof (int32_t)))
			return ERROR;
		*p = ep;
		*x++ = (int32_t) v;
	}

	return OK;
}


static int
text2name(char **x, int n, char **p)
{
	while (n-- > 0) {
		*p = strchr(*p, '\n');
		if (*p == 0)
			return ERROR;
		*(*p)++ = '\0';
		*x++ = *p;
	}

	return strchr(*p, '\n') != 0;	/* no more filenames allowed */
}


static int
test_map(roadmap *m)
{
	int i, j, n = m->num, low = m->children[0];


	if (low != 0)
		return ERROR;

	for (i = 0; i < n; i++)
		m->status[i] = 0;

	for (i = 1, j = 0; i <= n; i++) {
		int high = m->children[i];
		if (high < low)
			return ERROR;
		while (j < high) {
			int ch = m->child[j++];
			if ((ch < 0) || (ch >= n))
				return ERROR;
			if (ch < i)
				m->sorted = 0;
			m->status[ch]++;
		}
		low = high;
	}

	return OK;
}


static int
import_map(roadmap *m, int fd)
{
	struct stat st;
	int num;
	char *buf, *ptr;


	if (fstat(fd, &st) || (st.st_size >= (off_t) INT_MAX))
		return ERROR;

	buf = mmap(NULL, st.st_size + 1, PROT_READ | PROT_WRITE,
						MAP_PRIVATE, fd, 0);
	close(fd);
	if (buf == MAP_FAILED)
		return ERROR;

	buf[st.st_size] = '\0';
	ptr = strchr(buf, '\n');
	if (!ptr)
		return ERROR;

	num = (ptr - buf) / sizeof (int64_t);

	m->num  = num;
	m->todo = num;
	m->done = 0;
	m->sorted = 1;

	m->name = (char **) buf;
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
init_map(roadmap *m, int n, char **argv)
{
	m->num  = n;
	m->todo = n;
	m->done = 0;
	m->sorted = 0;

	m->name = argv;
	m->status = calloc(2 * n + 1, sizeof (int32_t));
	if (!m->status) {
		perror("calloc");
		exit(ERROR);
	}
	m->children = m->status + n;
}


static void
approve(roadmap *m, int i)
{
	int	own = m->children[i],
		num = m->children[i + 1] - own;
	int32_t *ch = m->child + own;

	m->status[i] = -1;
	m->done++;

	while (num--)
		m->status[*ch++]--;
}


static int
forget(roadmap *m, int i)
{
	int	own = m->children[i],
		num = m->children[i + 1] - own;

	if (num > 1)
		return 0;

	if (num == 1) {
		int ch = m->child[own];

		if ((m->status[ch] > 1) || !forget(m, ch))
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
	int	opt, log_fd_prev, fd = -1, map_fd = -1, dir_fd = keepdir(),
		retries_max, retries, i, hint, err = OK,
		step, prev, cur, storage;

	roadmap map;


	log_fd = log_fd_prev = envint("REDO_LOG_FD");

	opterr = 0;

	while ((opt = getopt(argc, argv, "+weftl:m:")) != -1) {
		switch (opt) {
		case 'w':
			setenvint("REDO_WARNING", 1);
			break;
		case 'e':
			setenvint("REDO_RECIPES", 1);
			break;
		case 'f':
			setenvint("REDO_FIND", 1);
			break;
		case 't':
			setenvint("REDO_TRACE", 1);
			break;
		case 'l':
			if (strcmp(optarg, "1") == 0)
				log_fd = 1;
			else if (strcmp(optarg, "2") == 0)
				log_fd = 2;
			else {
				log_fd =  open(optarg, CR_WR_TR, 0666);
				if (log_fd < 0) {
					perror("logfile");
					return ERROR;
				}
			}
			setenvint("REDO_LOG_FD", log_fd);
			break;
		case 'm':
			map_fd = open(optarg, O_RDONLY);
			if ((map_fd >= 0) && (
				(import_map(&map, map_fd) != OK) ||
				(!file_chdir(&dir_fd, optarg))
							)) {
					dprintf(2, "Bad map : %s\n", optarg);
					return ERROR;
			}
			break;
		default:
			dprintf(2,	"redo-c-weft-8\n"
					"Usage: redo [-weft] [-l <logname>]"
					" [-m <roadmap>] [TARGET [...]]\n");
			return ERROR;
		}
	}

	wflag = envint("REDO_WARNING");
	eflag = envint("REDO_RECIPES");
	fflag = envint("REDO_FIND");
	tflag = envint("REDO_TRACE");
	date_build("REDO_BUILD_DATE");
	track_init(getenv("REDO_TRACK"));
	retries_max = envint("REDO_RETRIES");
	unsetenv("REDO_RETRIES");

	if ((strcmp(base_name(argv[0]), "redo") == 0) || (map_fd >= 0)) {
		if (retries_max == 0)
			retries_max = RETRIES_DEFAULT;
	} else
		fd = envint("REDO_FD");

	if (map_fd < 0)
		init_map(&map, argc - optind, argv + optind);

	srand(getpid());
	fence(log_fd_prev, "return {\n", close_comment);
	retries = retries_max;

	do {
		hurry_up_on(retries-- == retries_max);

		for (i = 0, cur = 0; i < map.num ; i += step) {
			prev = cur;
			cur = map.status[i];

			if (cur >= 0)
				step = 1;
			else {
				step = - cur;
				if (prev >= 0)
					storage = i;
				else
					map.status[storage] += cur;
			}

			if (cur == 0) {
				err = update_dep(dir_fd, map.name[i], &hint);
				if (!err && (fd > 0))
					err = write_dep(fd, map.name[i], hint);

				if (!err) {
					approve(&map, i);
					retries = retries_max;
					if (map.sorted)
						break;
				} else if (err != BUSY) {
					err = ERROR;
					break;
				} else if (hint & IMMEDIATE_DEPENDENCY)
					forget(&map, i);
			}
		}
	} while ((err != ERROR) && (map.done < map.todo) && (retries > 0));

	fence(log_fd_prev, "}\n", open_comment);

	if (err != ERROR)
		err = (map.done < map.num) ? BUSY : OK;

	return err;
}

