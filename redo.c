/* An implementation of the redo build system
   in portable C with zero dependencies

   http://cr.yp.to/redo.html
   https://jdebp.eu./FGA/introduction-to-redo.html
   https://github.com/apenwarr/redo/blob/master/README.md
   http://news.dieweltistgarnichtso.net/bin/redo-sh.html

   To the extent possible under law,
   Leah Neukirchen <leah@vuxu.org>
   has waived all copyright and related or neighboring rights to this work.

   http://creativecommons.org/publicdomain/zero/1.0/
*/

/*
##% cc -g -Os -Wall -Wextra -Wwrite-strings -o $STEM $FILE
*/

/*
The purpose of this redo-c version is to fully unfold all advantages of hashed
dpendencies records. It means that for the dependency tree a -> b -> c changing
"c" must trigger execution of "b.do", but if the resulting "b" has the same
content as its previous version, then "a.do" must not be envoked.
This goal reached.

Another problem is deadlocking in case of parallel builds upon looped
dependency tree. Proposed technique doesn't locks on the busy target, but
returns with TARGET_BUSY exit code and releases the dependency tree to
make loop analysis possible for another process, and probably re-starting
build process after the random delay, attempting to crawl over the whole tree
when it will be released by another build processes.
This will be available after job server implementation.

Andrey Dobrovolsky <andrey.dobrovolsky.odessa@gmail.com>
*/

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

static char *
hashfile(int fd)
{
	static char hex[16] = "0123456789abcdef";
	static char asciihash[65];

	struct sha256 ctx;
	char buf[4096];
	char *a;
	unsigned char hash[32];
	int i;
	ssize_t r;

	sha256_init(&ctx);

	while ((r = read(fd, buf, sizeof buf)) > 0) {
		sha256_update(&ctx, buf, r);
	}

	sha256_sum(&ctx, hash);

	for (i = 0, a = asciihash; i < 32; i++) {
		*a++ = hex[hash[i] / 16];
		*a++ = hex[hash[i] % 16];
	}
	*a = 0;

	return asciihash;
}


static const char redo_prefix[] =   ".redo.";
static const char lock_prefix[] =   ".redo..redo.";
static const char target_prefix[] = ".redo..redo..redo.";


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
			track_buf_size += strlen (target);
		track_buf = malloc (track_buf_size);
		if (!track_buf) {
			perror("malloc");
			exit (-1);
		}
		*track_buf = 0;
		if (target)
			strcpy (track_buf, target);
		track_op = 0;				/* enough for the first time */
	}

	if (!track_op)
		return track_buf;

	if (!target) {					/* strip last path */
		ptr = strrchr(track_buf, ':');
		if (ptr)
			*ptr = 0;
		return ptr;
	}

	/* store cwd in the track_buf */

	target_wd_offset = strlen(track_buf) + 1;
	track_engaged = target_wd_offset + strlen(target) + sizeof lock_prefix + 2;

	while (1) {
		if (track_buf_size > track_engaged) {
			target_wd = getcwd (track_buf + target_wd_offset, track_buf_size - track_engaged);
			if (target_wd)		/* getcwd successful */
				break;
		} else
			errno = ERANGE;

		if (errno == ERANGE) {  /* track_buf_size is not sufficient */
			track_buf_size += PATH_MAX;
			track_buf = realloc (track_buf, track_buf_size);
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

	*--target_wd = ':';			/* join target full path with the track_buf */

	/* searching for target full path inside track_buf */

	ptr = track_buf;
	target_len = strlen(target_wd);

	do {
		ptr = strstr(ptr, target_wd);
		if (ptr == target_wd)
			return target_wd + 1;
		ptr += target_len;
	} while (*ptr != ':');

	return 0;
}


static const char *
base_name(const char *name, int uprel)
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
envfd(const char *name)
{
	long fd;

	char *s = getenv(name);
	if (!s)
		return -1;

	fd = strtol(s, 0, 10);
	if (fd < 0 || fd > 1023)
		fd = -1;

	return fd;
}

static int
envint(const char *name)
{
	int n = envfd(name);

	if (n < 0)
		n = 0;

	return n;
}

static int
setenvfd(const char *name, int i)
{
	char buf[16];
	snprintf(buf, sizeof buf, "%d", i);

	return setenv(name, buf, 1);
}

static const char *
datestat(struct stat *st)
{
	static char hexdate[17];

	snprintf(hexdate, sizeof hexdate, "%016" PRIx64, (uint64_t)st->st_ctime);

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
	static char hexdate[17] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0','\0'};
	const char *dateptr;

	FILE *f;

	if (hexdate[0])
		return hexdate;

	dateptr = getenv("REDO_BUILD_DATE");
	if (dateptr)
		return memcpy(hexdate, dateptr, 16);

	f = tmpfile();
	memcpy(hexdate, datefile(fileno(f)), 16);
	fclose(f);

	setenv("REDO_BUILD_DATE", hexdate, 1);

	return hexdate;
}


static const char *
file_chdir(int *fd, const char *name)
{
	int fd_new;
	char *slash = strrchr(name, '/');

	if (!slash)
		return name;

	*slash = 0;
	fd_new = openat(*fd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	*slash = '/';

	if (fd_new < 0) {
		perror("openat dir");
		return 0;
	}

	if (fchdir(fd_new) < 0) {
		perror("chdir");
		return 0;
	}

	*fd = fd_new;

	return slash + 1;
}


/*
dir/base.a.b
	will look for dir/base.a.b.do,
	dir/default.a.b.do, dir/default.b.do, dir/default.do,
	default.a.b.do, default.b.do, and default.do.

this function assumes no / in target
*/

static char *
find_dofile(char *target, char *dofile_rel, size_t dofile_free, int *uprel, const char *slash)
{
	static const char default_name[] = "default", suffix[] = ".do";
	const char *name = default_name + (sizeof default_name - 1);
	char *ext  = target; 
	char *dofile = dofile_rel;

	/* we may avoid doing default*.do files */
	/*
	if ((strncmp(target, default_name, sizeof default_name - 1) == 0) &&
	    (strcmp(strchr(target,'\0') - sizeof suffix + 1, suffix) == 0))
		return 0;
	*/

	if (dofile_free < (strlen(target) + sizeof suffix))
		return 0;
	dofile_free -= sizeof suffix;

	for (*uprel = 0 ; slash ; (*uprel)++, slash = strchr(slash + 1, '/')) {
		char *s = ext;

		while (1) {
			strcpy(stpcpy(stpcpy(dofile, name), s), suffix);

			if ((access(dofile_rel, F_OK) == 0) && (strcmp(target, dofile_rel) != 0 /* no self-doing */ )) {
				if (*s == '.')
					*s = '\0';
				return dofile;
			}

			if ((*s == 0) || ((name != default_name) && (*s == '.')))
				break;

			while (*++s && (*s != '.'));

			if (name != default_name) {
				size_t required = (sizeof default_name - 1) + strlen(s);

				if (dofile_free < required)
					return 0;
				dofile_free -= required;

				name = default_name;
				ext = s;
			}
		}

		if (dofile_free < 3)
			return 0;
		dofile_free -= 3;

		*dofile++ = '.';
		*dofile++ = '.';
		*dofile++ = '/';
		*dofile = 0;
	}

	return 0;
}


enum update_target_errors {
	TARGET_UPTODATE = 0,
	TARGET_FCHDIR_FAILED = 1,
	TARGET_WRDEP_FAILED = 2,
	TARGET_RM_FAILED = 4,
	TARGET_MV_FAILED = 8,
	TARGET_BUSY = 0x10,
	TARGET_TOOLONG = 0x20,
	TARGET_REL_TOOLONG = 0x30,
	TARGET_FORK_FAILED = 0x40,
	TARGET_WAIT_FAILED = 0x50,
	TARGET_NODIR = 0x60,
	TARGET_LOOP = 0x70,
};


static int
choose(const char *old, const char *new, int err)
{
	if (err) {
		if ((access(new, F_OK) == 0) && (remove(new) != 0)) {
			perror("remove new");
			err |= TARGET_RM_FAILED;
		}
	} else {
		if ((access(old, F_OK) == 0) && (remove(old) != 0)) {
			perror("remove old");
			err |= TARGET_RM_FAILED;
		}
		if ((access(new, F_OK) == 0) && (rename(new, old) != 0)) {
			perror("rename");
			err |= TARGET_MV_FAILED;
		}
	}

	return err;
}


int xflag, fflag, sflag, tflag;


static int 
run_script(int dir_fd, int lock_fd, int nlevel, const char *dofile_rel,
	const char *target, const char *target_base, const char *target_full, int uprel)
{
	int target_err = 0;

	pid_t pid;
	const char *target_rel; 
	char target_base_rel[PATH_MAX];

	char *target_new;
	char target_new_rel[PATH_MAX + sizeof target_prefix];


	target_rel = base_name(target_full, uprel);
	if (strlen(target_rel) >= sizeof target_base_rel){
		fprintf(stderr, "Target relative name too long -- %s\n", target_rel);
		return TARGET_REL_TOOLONG;
	}

	strcpy(target_base_rel, target_rel);
	strcpy((char *) base_name(target_base_rel, 0), target_base);

	strcpy(target_new_rel, target_rel);
	target_new = (char *) base_name(target_new_rel, 0);
	strcpy(stpcpy(target_new, target_prefix), target);

	fprintf(stderr, "redo %*s %s # %s\n", nlevel * 2, "", target, dofile_rel);

	pid = fork();
	if (pid < 0) {
		perror("fork");
		target_err = TARGET_FORK_FAILED;
	} else if (pid == 0) {

		const char *dofile = file_chdir(&dir_fd, dofile_rel);
		char dirprefix[PATH_MAX];
		size_t dirprefix_len = target_new - target_new_rel;

		if (!dofile) {
			fprintf(stderr, "Damn! Someone have stolen my favorite dofile %s ...\n", dofile_rel);
			exit(-1);
		}

		memcpy(dirprefix, target_new_rel, dirprefix_len);
		dirprefix[dirprefix_len] = '\0';

		if ((setenvfd("REDO_LOCK_FD", lock_fd) != 0) ||
		    (setenvfd("REDO_LEVEL", nlevel + 1) != 0) ||
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
			target_err = TARGET_WAIT_FAILED;
		} else {
			if (WIFEXITED(target_err))
				target_err = WEXITSTATUS(target_err);
		}
	}
	fprintf(stderr, "     %*s %s # %s -> %d\n", nlevel * 2, "", target, dofile_rel, target_err);

	return choose(target, target_new, target_err);
}


static int
check_record(char *line)
{
	int line_len = strlen(line);

	if (line_len < 64 + 1 + 16 + 1 + 1)
		return 1;

	if (line[line_len - 1] != '\n') {
		fprintf(stderr, "Warning: dependency record truncated. Target will be rebuilt.\n");
		return 1;
	}

	line[line_len - 1] = 0; /* strip \n */

	return 0;
}


static int
dep_changed(const char *line)
{
	const char *filename = line + 64 + 1 + 16 + 1, *hashstr;
	int fd;

	if (strncmp(line + 64 + 1, datefilename(filename), 16) == 0)
		return 0;

	fd = open(filename, O_RDONLY);
	hashstr = hashfile(fd);
	close(fd);

	return strncmp(line, hashstr, 64) != 0;
}


static int
write_dep(int lock_fd, const char *file, const char *dp, const char *updir)
{
	int err = 0;
	int fd = open(file, O_RDONLY);
	const char *prefix = "";


	if (dp && *file != '/') {
		size_t dp_len = strlen(dp);

		if (strncmp(file, dp, dp_len) == 0)
			file += dp_len;
		else
			prefix = updir;
	}

	if (dprintf(lock_fd, "%s %s %s%s\n", hashfile(fd), datefile(fd), prefix, file) < 0) {
		perror("dprintf");
		err = TARGET_WRDEP_FAILED;
	}

	if (fd > 0)
		close(fd);

	return err;
}


static int update_dep(int *dir_fd, const char *dep_path, int nlevel);


static int
do_update_dep(int dir_fd, const char *dep_path, int nlevel)
{
	int dep_dir_fd = dir_fd;
	int dep_err = update_dep(&dep_dir_fd, dep_path, nlevel);

	track(0, 1);	/* strip the last record */

	if (dir_fd != dep_dir_fd) {
		if (fchdir(dir_fd) < 0) {
			perror("chdir back");
			dep_err |= TARGET_FCHDIR_FAILED;
		}
		close(dep_dir_fd);
	}

	return dep_err;
}


static int
update_dep(int *dir_fd, const char *dep_path, int nlevel)
{
	const char *target;
	char *target_full, target_base[PATH_MAX];

	char dofile_rel [PATH_MAX];

	int uprel;

	char redofile[PATH_MAX + sizeof redo_prefix];
	char lockfile[PATH_MAX + sizeof lock_prefix];

	int lock_fd, dep_err = 0;

	FILE *fredo;


	target = file_chdir(dir_fd, dep_path);
	if (target == 0) {
		fprintf(stderr, "Missing dependency directory -- %s\n", dep_path);
		track("", 1);	/* dummy call */
		return TARGET_NODIR;
	}

	target_full = track(target, 1);
	if (target_full == 0){
		fprintf(stderr, "Dependency loop attempt -- %s\n", dep_path);
		return TARGET_LOOP;
	}

	if (strlen(target) >= sizeof target_base) {
		fprintf(stderr, "Dependency name too long -- %s\n", target);
		return TARGET_TOOLONG;
	}

	strcpy(target_base, target);
	if (!find_dofile(target_base, dofile_rel, sizeof dofile_rel, &uprel, target_full)) {
		if (sflag)
			printf("%s\n", target_full);
		return TARGET_UPTODATE;
	}

	if (tflag)
		printf("%s\n", target_full);

	strcpy(stpcpy(redofile, redo_prefix), target);
	if (strcmp(datefilename(redofile), datebuild()) >= 0)
		return TARGET_UPTODATE;

	strcpy(stpcpy(lockfile, lock_prefix), target);

	lock_fd = open(lockfile, O_CREAT | O_EXCL | O_WRONLY, 0666);
	if (lock_fd < 0) {
		fprintf(stderr, "Target busy -- %s\n", target);
		return TARGET_BUSY;
	}

	fredo = /* fflag ? NULL : */ fopen(redofile,"r");

	if (fredo) {
		int firstline = 1;
		char line[64 + 1 + 16 + 1 + PATH_MAX + 1];
		const char *filename = line + 64 + 1 + 16 + 1;

		for ( ; fgets(line, sizeof line, fredo) ; firstline = 0) {
			if (check_record(line) != 0)
				break;

			if (firstline && strcmp(filename, dofile_rel) != 0)
					break;

			if (strcmp(filename, target) != 0) {

				dep_err = do_update_dep(*dir_fd, filename, nlevel + 1);

				if (fflag || dep_err || dep_changed(line))
					break;
			} else {
				struct stat lock_st;

				if (dep_changed(line))
					break;

				fclose(fredo);
				fstat(lock_fd, &lock_st);		/* read with umask applied */
				chmod(redofile, lock_st.st_mode);	/* freshen up redofile ctime */
				close(lock_fd);

				if (remove(lockfile) != 0) {
					perror("remove lock");
					return TARGET_RM_FAILED;
				}

				return TARGET_UPTODATE;
			}
		}
		fclose(fredo);
	}


	if (!dep_err) {
		dep_err = write_dep(lock_fd, dofile_rel, 0, 0);

		if (!dep_err) {
			dep_err = run_script(*dir_fd, lock_fd, nlevel, dofile_rel, target, target_base, target_full, uprel);

			if (!dep_err)
				dep_err = write_dep(lock_fd, target, 0, 0);
		}
	}

	close(lock_fd);

/*
	Now we will use target_full residing in track to construct
	the lockfile_full. If fchdir() in do_update_target() failed
	then we need the full lockfile name.
*/

	strcpy((char *) base_name(target_full, 0), lockfile);

	return choose(redofile, target_full, dep_err);
}


static void
compute_updir(const char *dp, char *u)
{
	*u = 0;

	while (dp) {
		dp = strchr(dp, '/');
		if (dp) {
			dp++;
			*u++ = '.';
			*u++ = '.';
			*u++ = '/';
			*u = 0;
		}
	}
}


static int
keepdir()
{
	int fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		perror("dir open");
		exit(-1);
	}
	return fd;
}


int
main(int argc, char *argv[])
{
	int opt, i;
	const char *dirprefix;
	char updir[PATH_MAX];
	int main_dir_fd, lock_fd;
	int level;
	int dep_err, redo_err = 0;

	const char *program = base_name(argv[0], 0);


	while ((opt = getopt(argc, argv, "+xfst")) != -1) {
		switch (opt) {
		case 'x':
			setenvfd("REDO_TRACE", 1);
			break;
		case 'f':
			setenvfd("REDO_FORCE", 1);
			break;
		case 's':
			setenvfd("REDO_LIST_SOURCES", 1);
			break;
		case 't':
			setenvfd("REDO_LIST_TARGETS", 1);
			break;
		default:
			fprintf(stderr, "usage: redo [-fxst]  [TARGETS...]\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	fflag = envint("REDO_FORCE");
	xflag = envint("REDO_TRACE");
	sflag = envint("REDO_LIST_SOURCES");
	tflag = envint("REDO_LIST_TARGETS");

	lock_fd = envfd("REDO_LOCK_FD");
	level = envint("REDO_LEVEL");
	dirprefix = getenv("REDO_DIRPREFIX");

	track(getenv("REDO_TRACK"), 0);

	if (strcmp(program, "redo-always") == 0)
		dprintf(lock_fd, "\n");

	compute_updir(dirprefix, updir);

	datebuild();

	main_dir_fd = keepdir();

	for (i = 0 ; i < argc ; i++) {

		dep_err = do_update_dep(main_dir_fd, argv[i], level);

		if(dep_err == 0) {
			if (lock_fd > 0) {
				dep_err = write_dep(lock_fd, argv[i], dirprefix, updir);
				if (dep_err)
					return dep_err;
			}
		} else if(dep_err == TARGET_BUSY) {
			redo_err = TARGET_BUSY;
		} else
			return dep_err;
	}

	return redo_err;
}

