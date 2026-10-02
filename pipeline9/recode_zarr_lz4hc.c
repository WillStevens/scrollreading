/* recode_zarr_lz4hc.c: write a copy of a uint8 zarr v2 array whose chunks are compressed with LZ4HC, no shuffle.

   Why. The growth spends a large share of its time decompressing chunks of the surface prediction, which is
   published as blosc zstd level 1 with byte shuffle. For uint8 data the shuffle does nothing but cost time, and
   LZ4HC decodes faster than zstd. The copy holds the same voxels: every chunk is decoded, encoded
   again, written, read back, decoded and compared byte for byte with the source's decode; any difference stops
   the run with exit status 1. Point the growth at the copy with SIMPAPER_SURFACE_ZARR=<dst>.

   What the copy is. The chunks are written by c-blosc2 (blosc2_compress), in the blosc2 format. zarr_1.c reads
   them, because it decodes with blosc2_decompress and takes the codec from each chunk's own header. numcodecs
   and other readers built on c-blosc 1 do not: the copy is for this pipeline, not a general zarr. The .zarray
   is copied with compressor cname lz4hc, clevel and shuffle 0; nothing else in it changes.

   Usage: recode_zarr_lz4hc <src array dir> <dst array dir> [clevel, default 5] [part/parts, default 1/1]
   With part/parts, only the chunk files whose position in the sorted list is part-1 modulo parts are done, so
   that several processes can share one copy: recode_zarr_lz4hc src dst 5 1/4 & ... recode_zarr_lz4hc src dst 5 4/4
   Absent chunks stay absent (zarr reads them as the fill value). */
#include <blosc2.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static unsigned char *readall(const char *p, long *n)
{
	FILE *f = fopen(p, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
	unsigned char *b = (unsigned char *)malloc(*n > 0 ? *n : 1);
	if (fread(b, 1, *n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
	fclose(f);
	return b;
}

static int mkdirs(char *p)
{
	for (char *s = p + 1; *s; s++)
		if (*s == '/') { *s = 0; if (mkdir(p, 0775) && errno != EEXIST) { *s = '/'; return -1; } *s = '/'; }
	return 0;
}

static char **names = NULL;
static long nnames = 0, capnames = 0;

static void walk(const char *root, const char *rel)
{
	char path[4096];
	snprintf(path, sizeof path, "%s%s%s", root, *rel ? "/" : "", rel);
	DIR *d = opendir(path);
	if (!d) return;
	struct dirent *e;
	while ((e = readdir(d)))
	{
		if (e->d_name[0] == '.') continue;   /* ., .., .zarray, .zattrs */
		char r[4096], full[4096];
		snprintf(r, sizeof r, "%s%s%s", rel, *rel ? "/" : "", e->d_name);
		snprintf(full, sizeof full, "%s/%s", root, r);
		struct stat st;
		if (stat(full, &st)) continue;
		if (S_ISDIR(st.st_mode)) walk(root, r);
		else if (S_ISREG(st.st_mode))
		{
			if (nnames == capnames) { capnames = capnames ? 2 * capnames : 4096; names = (char **)realloc(names, capnames * sizeof *names); }
			names[nnames++] = strdup(r);
		}
	}
	closedir(d);
}

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* replaces the value after "key": up to the next , or } (a string or a number) */
static int setkey(char *json, size_t cap, const char *key, const char *value)
{
	char k[64];
	snprintf(k, sizeof k, "\"%s\"", key);
	char *p = strstr(json, k);
	if (!p) return -1;
	p = strchr(p + strlen(k), ':');
	if (!p) return -1;
	p++;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
	char *q = p;
	if (*q == '"') { q = strchr(q + 1, '"'); if (!q) return -1; q++; }
	else while (*q && *q != ',' && *q != '}' && *q != '\n' && *q != '\r') q++;
	size_t tail = strlen(q);
	if ((size_t)(p - json) + strlen(value) + tail + 1 > cap) return -1;
	memmove(p + strlen(value), q, tail + 1);
	memcpy(p, value, strlen(value));
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 3 || argc > 5)
	{
		fprintf(stderr, "usage: recode_zarr_lz4hc <src array dir> <dst array dir> [clevel] [part/parts]\n");
		return 2;
	}
	const char *src = argv[1], *dst = argv[2];
	int clevel = argc > 3 ? atoi(argv[3]) : 5;
	int part = 1, parts = 1;
	if (argc > 4 && (sscanf(argv[4], "%d/%d", &part, &parts) != 2 || part < 1 || part > parts))
	{
		fprintf(stderr, "part/parts refused: %s\n", argv[4]);
		return 2;
	}

	char zp[4096], zd[4096];
	snprintf(zp, sizeof zp, "%s/.zarray", src);
	long zn = 0;
	unsigned char *z = readall(zp, &zn);
	if (!z) { fprintf(stderr, "no .zarray in %s\n", src); return 2; }
	size_t zcap = zn + 256;
	char *zj = (char *)calloc(zcap, 1);
	memcpy(zj, z, zn);
	if (!strstr(zj, "\"|u1\"")) { fprintf(stderr, "only uint8 arrays (dtype |u1) are handled\n"); return 2; }
	char lv[16];
	snprintf(lv, sizeof lv, "%d", clevel);
	if (setkey(zj, zcap, "cname", "\"lz4hc\"") || setkey(zj, zcap, "clevel", lv) || setkey(zj, zcap, "shuffle", "0"))
	{
		fprintf(stderr, "compressor keys not found in %s\n", zp);
		return 2;
	}
	snprintf(zd, sizeof zd, "%s/.zarray", dst);
	mkdirs(zd);
	if (part == 1)
	{
		FILE *w = fopen(zd, "wb");
		if (!w || fwrite(zj, 1, strlen(zj), w) != strlen(zj)) { fprintf(stderr, "cannot write %s\n", zd); return 2; }
		fclose(w);
	}

	walk(src, "");
	qsort(names, nnames, sizeof *names, cmp);

	blosc2_init();
	long done = 0, in_bytes = 0, out_bytes = 0;
	int bad = 0;
	for (long i = part - 1; i < nnames && !bad; i += parts)
	{
		char sp[4096], dp[4096];
		snprintf(sp, sizeof sp, "%s/%s", src, names[i]);
		snprintf(dp, sizeof dp, "%s/%s", dst, names[i]);
		long ns = 0, nd = 0;
		unsigned char *s = readall(sp, &ns);
		int32_t nbytes = 0, cbytes = 0, blocksize = 0;
		if (!s || blosc2_cbuffer_sizes(s, &nbytes, &cbytes, &blocksize) < 0 || nbytes <= 0)
		{
			fprintf(stderr, "%s: not a blosc chunk\n", sp);
			bad = 1; free(s); break;
		}
		unsigned char *a = (unsigned char *)malloc(nbytes), *b = (unsigned char *)malloc(nbytes);
		unsigned char *enc = (unsigned char *)malloc(nbytes + BLOSC2_MAX_OVERHEAD);
		int da = blosc2_decompress(s, (int32_t)ns, a, nbytes);
		if (blosc1_set_compressor("lz4hc") < 0) { fprintf(stderr, "lz4hc is not in this libblosc2\n"); return 2; }
		int ne = blosc2_compress(clevel, BLOSC_NOSHUFFLE, 1, a, da > 0 ? da : 0, enc, nbytes + BLOSC2_MAX_OVERHEAD);
		if (da != nbytes || ne <= 0) { fprintf(stderr, "%s: decode %d encode %d\n", sp, da, ne); bad = 1; }
		else
		{
			mkdirs(dp);
			FILE *w = fopen(dp, "wb");
			if (!w || fwrite(enc, 1, ne, w) != (size_t)ne) { fprintf(stderr, "%s: write failed\n", dp); bad = 1; }
			if (w) fclose(w);
			unsigned char *back = bad ? NULL : readall(dp, &nd);
			int db = back ? blosc2_decompress(back, (int32_t)nd, b, nbytes) : -1;
			if (!bad && (db != nbytes || memcmp(a, b, nbytes) != 0)) { fprintf(stderr, "%s: read back differs\n", dp); bad = 1; }
			free(back);
			done++; in_bytes += ns; out_bytes += nd;
		}
		free(a); free(b); free(enc); free(s);
	}
	blosc2_destroy();
	fprintf(stderr, "%ld chunks recoded and read back equal (part %d/%d), %ld bytes in, %ld bytes out%s\n",
	        done, part, parts, in_bytes, out_bytes, bad ? "; STOPPED ON A DIFFERENCE OR AN ERROR" : "");
	return bad;
}
