#include "jni_bridge.h"

#include "bzip2-1.0.8/bzlib.h"
#include "xmd5.h"

int diff_patch(int argc, char *argv[]);

int main() {
    char *argv[4] = {"bsdiff",
                     "D:/Projects/JavaEE/diff-server/fileCache/1.2.3.apk",
                     "D:/Projects/JavaEE/diff-server/fileCache/1.3.0.apk",
                     "D:/Projects/JavaEE/diff-server/fileCache/patch.patch"};
    return diff_patch(4, argv);
}

void logger(JNIEnv *env, char *msg) {
    jclass jls = (*env)->FindClass(env, "java/lang/System");
    jfieldID fid = (*env)->GetStaticFieldID(env, jls, "out", "Ljava/io/PrintStream;");
    jobject out = (*env)->GetStaticObjectField(env, jls, fid);

    jclass outJls = (*env)->GetObjectClass(env, out);
    jmethodID mid = (*env)->GetMethodID(env, outJls, "println", "(Ljava/lang/String;)V");
    jstring data = (*env)->NewStringUTF(env, "123123213");
    (*env)->CallVoidMethod(env, outJls, mid, data);
}

JNIEXPORT jint

JNICALL Java_shop_memall_diff_diffserver_bridge_JNIBridge_createPatch
        (JNIEnv *env, jclass jcls, jstring old_apk_path, jstring new_apk_path, jstring patch_path) {
    const char *argv[4] = {"bsdiff"};
    argv[1] = (*env)->GetStringUTFChars(env, old_apk_path, NULL);
    argv[2] = (*env)->GetStringUTFChars(env, new_apk_path, NULL);
    argv[3] = (*env)->GetStringUTFChars(env, patch_path, NULL);

    int ret = diff_patch(4, (char **) argv);

    (*env)->ReleaseStringUTFChars(env, old_apk_path, argv[1]);
    (*env)->ReleaseStringUTFChars(env, new_apk_path, argv[2]);
    (*env)->ReleaseStringUTFChars(env, patch_path, argv[3]);

    return ret;
}

#define MIN(x, y) (((x)<(y)) ? (x) : (y))


#define    CTRL_BLOCK_OFFSET    40
#define DIFF_BLOCK_OFFSET    48
#define NEW_SIZE_OFFSET        56
#define HEADER_SIZE            64

typedef long off_t;
typedef unsigned char u_char;

static void split(off_t *I, off_t *V, off_t start, off_t len, off_t h) {
    off_t i, j, k, x, tmp, jj, kk;

    if (len < 16) {
        for (k = start; k < start + len; k += j) {
            j = 1;
            x = V[I[k] + h];
            for (i = 1; k + i < start + len; i++) {
                if (V[I[k + i] + h] < x) {
                    x = V[I[k + i] + h];
                    j = 0;
                };
                if (V[I[k + i] + h] == x) {
                    tmp = I[k + j];
                    I[k + j] = I[k + i];
                    I[k + i] = tmp;
                    j++;
                };
            };
            for (i = 0; i < j; i++) V[I[k + i]] = k + j - 1;
            if (j == 1) I[k] = -1;
        };
        return;
    };

    x = V[I[start + len / 2] + h];
    jj = 0;
    kk = 0;
    for (i = start; i < start + len; i++) {
        if (V[I[i] + h] < x) jj++;
        if (V[I[i] + h] == x) kk++;
    };
    jj += start;
    kk += jj;

    i = start;
    j = 0;
    k = 0;
    while (i < jj) {
        if (V[I[i] + h] < x) {
            i++;
        } else if (V[I[i] + h] == x) {
            tmp = I[i];
            I[i] = I[jj + j];
            I[jj + j] = tmp;
            j++;
        } else {
            tmp = I[i];
            I[i] = I[kk + k];
            I[kk + k] = tmp;
            k++;
        };
    };

    while (jj + j < kk) {
        if (V[I[jj + j] + h] == x) {
            j++;
        } else {
            tmp = I[jj + j];
            I[jj + j] = I[kk + k];
            I[kk + k] = tmp;
            k++;
        };
    };

    if (jj > start) split(I, V, start, jj - start, h);

    for (i = 0; i < kk - jj; i++) V[I[jj + i]] = kk - 1;
    if (jj == kk - 1) I[jj] = -1;

    if (start + len > kk) split(I, V, kk, start + len - kk, h);
}

static void qsufsort(off_t *I, off_t *V, u_char *old, off_t oldsize) {
    off_t buckets[256];
    off_t i, h, len;

    for (i = 0; i < 256; i++) buckets[i] = 0;
    for (i = 0; i < oldsize; i++) buckets[old[i]]++;
    for (i = 1; i < 256; i++) buckets[i] += buckets[i - 1];
    for (i = 255; i > 0; i--) buckets[i] = buckets[i - 1];
    buckets[0] = 0;

    for (i = 0; i < oldsize; i++) I[++buckets[old[i]]] = i;
    I[0] = oldsize;
    for (i = 0; i < oldsize; i++) V[i] = buckets[old[i]];
    V[oldsize] = 0;
    for (i = 1; i < 256; i++) if (buckets[i] == buckets[i - 1] + 1) I[buckets[i]] = -1;
    I[0] = -1;

    for (h = 1; I[0] != -(oldsize + 1); h += h) {
        len = 0;
        for (i = 0; i < oldsize + 1;) {
            if (I[i] < 0) {
                len -= I[i];
                i -= I[i];
            } else {
                if (len) I[i - len] = -len;
                len = V[I[i]] + 1 - i;
                split(I, V, i, len, h);
                i += len;
                len = 0;
            };
        };
        if (len) I[i - len] = -len;
    };

    for (i = 0; i < oldsize + 1; i++) I[V[i]] = i;
}


static off_t matchlen(u_char *old, off_t oldsize, u_char *new, off_t newsize) {
    off_t i;

    for (i = 0; (i < oldsize) && (i < newsize); i++)
        if (old[i] != new[i]) break;

    return i;
}

static off_t search(off_t *I, u_char *old, off_t oldsize,
                    u_char *new, off_t newsize, off_t st, off_t en, off_t *pos) {
    off_t x, y;

    if (en - st < 2) {
        x = matchlen(old + I[st], oldsize - I[st], new, newsize);
        y = matchlen(old + I[en], oldsize - I[en], new, newsize);

        if (x > y) {
            *pos = I[st];
            return x;
        } else {
            *pos = I[en];
            return y;
        }
    };

    x = st + (en - st) / 2;
    if (memcmp(old + I[x], new, MIN(oldsize - I[x], newsize)) < 0) {
        return search(I, old, oldsize, new, newsize, x, en, pos);
    } else {
        return search(I, old, oldsize, new, newsize, st, x, pos);
    };
}

static void offtout(off_t x, u_char *buf) {
    off_t y;

    if (x < 0) y = -x; else y = x;

    buf[0] = y % 256;
    y -= buf[0];
    y = y / 256;
    buf[1] = y % 256;
    y -= buf[1];
    y = y / 256;
    buf[2] = y % 256;
    y -= buf[2];
    y = y / 256;
    buf[3] = y % 256;
    y -= buf[3];
    y = y / 256;
    buf[4] = y % 256;
    y -= buf[4];
    y = y / 256;
    buf[5] = y % 256;
    y -= buf[5];
    y = y / 256;
    buf[6] = y % 256;
    y -= buf[6];
    y = y / 256;
    buf[7] = y % 256;

    if (x < 0) buf[7] |= 0x80;
}

int diff_patch(int argc, char *argv[]) {
    FILE *fd;
    u_char *old, *newb;
    off_t oldsize, newsize;
    off_t *I, *V;
    off_t scan, pos, len;
    off_t lastscan, lastpos, lastoffset;
    off_t oldscore, scsc;
    off_t s, Sf, lenf, Sb, lenb;
    off_t overlap, Ss, lens;
    off_t i;
    off_t dblen, eblen;
    u_char *db, *eb;
    u_char buf[8];
    u_char header[64];
    char md5sum[33];
    FILE *pf;
    BZFILE *pfbz2;
    int bz2err;

    if (argc != 4) return 1;

    /* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
        that we never try to malloc(0) and get a NULL pointer */
    if (((fd = fopen(argv[1], "rb")) == NULL) ||
        (fseek(fd, 0, SEEK_END) != 0) ||
        ((oldsize = ftell(fd)) < 0) ||
        ((old = malloc(oldsize + 1)) == NULL) ||
        (fseek(fd, 0, SEEK_SET) != 0) ||
        (fread(old, 1, oldsize, fd) != oldsize) ||
        (fclose(fd) == -1))
        return 2;
    //printf("oldsize: %d\n", oldsize);
    if (((I = malloc((oldsize + 1) * sizeof(off_t))) == NULL) ||
        ((V = malloc((oldsize + 1) * sizeof(off_t))) == NULL))
        return 3;

    qsufsort(I, V, old, oldsize);

    free(V);

    /* Allocate newsize+1 bytes instead of newsize bytes to ensure
        that we never try to malloc(0) and get a NULL pointer */
    if (((fd = fopen(argv[2], "rb")) < 0) ||
        (fseek(fd, 0, SEEK_END) != 0) ||
        ((newsize = ftell(fd)) < 0) ||
        ((newb = malloc(newsize + 1)) == NULL) ||
        (fseek(fd, 0, SEEK_SET) != 0) ||
        (fread(newb, 1, newsize, fd) != newsize) ||
        (fclose(fd) == -1))
        return 4;
    //printf("newsize: %d\n", newsize);
    if (((db = malloc(newsize + 1)) == NULL) ||
        ((eb = malloc(newsize + 1)) == NULL))
        return 5;
    dblen = 0;
    eblen = 0;

    /* Create the patch file */
    if ((pf = fopen(argv[3], "wb+")) == NULL)
        return 6;

    /* Header is
        0	8	 "BSDIFF40"
        8	8	length of bzip2ed ctrl block
        16	8	length of bzip2ed diff block
        24	8	length of new file */
    /* File is
        0	32	Header
        32	??	Bzip2ed ctrl block
        ??	??	Bzip2ed diff block
        ??	??	Bzip2ed extra block */
    memset(header, 0, HEADER_SIZE);
    memcpy(header, "BSDIFF40", 8);
    offtout(0, header + CTRL_BLOCK_OFFSET);
    offtout(0, header + DIFF_BLOCK_OFFSET);
    offtout(newsize, header + NEW_SIZE_OFFSET);
    if (fwrite(header, HEADER_SIZE, 1, pf) != 1)
        return 7;

    /* Compute the differences, writing ctrl as we go */
    if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL)
        return 8;
    scan = 0;
    len = 0;
    lastscan = 0;
    lastpos = 0;
    lastoffset = 0;
    while (scan < newsize) {
        oldscore = 0;

        for (scsc = scan += len; scan < newsize; scan++) {
            len = search(I, old, oldsize, newb + scan, newsize - scan,
                         0, oldsize, &pos);

            for (; scsc < scan + len; scsc++)
                if ((scsc + lastoffset < oldsize) &&
                    (old[scsc + lastoffset] == newb[scsc]))
                    oldscore++;

            if (((len == oldscore) && (len != 0)) ||
                (len > oldscore + 8))
                break;

            if ((scan + lastoffset < oldsize) &&
                (old[scan + lastoffset] == newb[scan]))
                oldscore--;
        };

        if ((len != oldscore) || (scan == newsize)) {
            s = 0;
            Sf = 0;
            lenf = 0;
            for (i = 0; (lastscan + i < scan) && (lastpos + i < oldsize);) {
                if (old[lastpos + i] == newb[lastscan + i]) s++;
                i++;
                if (s * 2 - i > Sf * 2 - lenf) {
                    Sf = s;
                    lenf = i;
                };
            };

            lenb = 0;
            if (scan < newsize) {
                s = 0;
                Sb = 0;
                for (i = 1; (scan >= lastscan + i) && (pos >= i); i++) {
                    if (old[pos - i] == newb[scan - i]) s++;
                    if (s * 2 - i > Sb * 2 - lenb) {
                        Sb = s;
                        lenb = i;
                    };
                };
            };

            if (lastscan + lenf > scan - lenb) {
                overlap = (lastscan + lenf) - (scan - lenb);
                s = 0;
                Ss = 0;
                lens = 0;
                for (i = 0; i < overlap; i++) {
                    if (newb[lastscan + lenf - overlap + i] ==
                        old[lastpos + lenf - overlap + i])
                        s++;
                    if (newb[scan - lenb + i] ==
                        old[pos - lenb + i])
                        s--;
                    if (s > Ss) {
                        Ss = s;
                        lens = i + 1;
                    };
                };

                lenf += lens - overlap;
                lenb -= lens;
            };

            for (i = 0; i < lenf; i++)
                db[dblen + i] = newb[lastscan + i] - old[lastpos + i];
            for (i = 0; i < (scan - lenb) - (lastscan + lenf); i++)
                eb[eblen + i] = newb[lastscan + lenf + i];

            dblen += lenf;
            eblen += (scan - lenb) - (lastscan + lenf);

            offtout(lenf, buf);
            BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
            if (bz2err != BZ_OK)
                return 9;

            offtout((scan - lenb) - (lastscan + lenf), buf);
            BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
            if (bz2err != BZ_OK)
                return 10;

            offtout((pos - lenb) - (lastpos + lenf), buf);
            BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
            if (bz2err != BZ_OK)
                return 11;

            lastscan = scan - lenb;
            lastpos = pos - lenb;
            lastoffset = pos - scan;
        };
    };
    BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
    if (bz2err != BZ_OK)
        return 12;

    /* Compute size of compressed ctrl data */
    if ((len = ftell(pf)) == -1)
        return 13;
    offtout(len - HEADER_SIZE, header + CTRL_BLOCK_OFFSET);

    /* Write compressed diff data */
    if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL)
        return 14;
    BZ2_bzWrite(&bz2err, pfbz2, db, dblen);
    if (bz2err != BZ_OK)
        return 15;
    BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
    if (bz2err != BZ_OK)
        return 16;

    /* Compute size of compressed diff data */
    if ((newsize = ftell(pf)) == -1)
        return 17;
    offtout(newsize - len, header + DIFF_BLOCK_OFFSET);

    /* Write compressed extra data */
    if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL)
        return 18;
    BZ2_bzWrite(&bz2err, pfbz2, eb, eblen);
    if (bz2err != BZ_OK)
        return 19;
    BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
    if (bz2err != BZ_OK)
        return 20;

    /* Seek to the beginning, write the header, and close the file */
    if (fseek(pf, 0, SEEK_SET))
        return 21;
    if (fwrite(header, HEADER_SIZE, 1, pf) != 1)
        return 22;
    if (fflush(pf) != 0)
        return 23;
    if (fseek(pf, 40, SEEK_SET))
        return 24;
    if (md5sum_fp_to_string(pf, md5sum) == 0)
        return 25;
    if (fseek(pf, 8, SEEK_SET))
        return 26;
    if (fwrite(md5sum, 32, 1, pf) != 1)
        return 27;
    if (fclose(pf))
        return 28;

    /* Free the memory we used */
    free(db);
    free(eb);
    free(I);
    free(old);
    free(newb);

    printf("Success");
    return 0;
}
