#ifdef __cplusplus
extern "C" {
#endif
#include "lmp_f2c.h"
static doublecomplex c_b2 = {0., 0.};
static integer c__1 = 1;
int zhetri_(char *uplo, integer *n, doublecomplex *a, integer *lda, integer *ipiv,
            doublecomplex *work, integer *info, ftnlen uplo_len)
{
    integer a_dim1, a_offset, i__1, i__2, i__3;
    doublereal d__1;
    doublecomplex z__1, z__2;
    double z_lmp_abs(doublecomplex *);
    void d_lmp_cnjg(doublecomplex *, doublecomplex *);
    doublereal d__;
    integer j, k;
    doublereal t, ak;
    integer kp;
    doublereal akp1;
    doublecomplex temp, akkp1;
    extern logical lsame_(char *, char *, ftnlen, ftnlen);
    extern VOID zdotc_(doublecomplex *, integer *, doublecomplex *, integer *, doublecomplex *,
                       integer *);
    integer kstep;
    extern int zhemv_(char *, integer *, doublecomplex *, doublecomplex *, integer *,
                      doublecomplex *, integer *, doublecomplex *, doublecomplex *, integer *,
                      ftnlen);
    logical upper;
    extern int zcopy_(integer *, doublecomplex *, integer *, doublecomplex *, integer *),
        zswap_(integer *, doublecomplex *, integer *, doublecomplex *, integer *),
        xerbla_(char *, integer *, ftnlen);
    a_dim1 = *lda;
    a_offset = 1 + a_dim1;
    a -= a_offset;
    --ipiv;
    --work;
    *info = 0;
    upper = lsame_(uplo, (char *)"U", (ftnlen)1, (ftnlen)1);
    if (!upper && !lsame_(uplo, (char *)"L", (ftnlen)1, (ftnlen)1)) {
        *info = -1;
    } else if (*n < 0) {
        *info = -2;
    } else if (*lda < max(1, *n)) {
        *info = -4;
    }
    if (*info != 0) {
        i__1 = -(*info);
        xerbla_((char *)"ZHETRI", &i__1, (ftnlen)6);
        return 0;
    }
    if (*n == 0) {
        return 0;
    }
    if (upper) {
        for (*info = *n; *info >= 1; --(*info)) {
            i__1 = *info + *info * a_dim1;
            if (ipiv[*info] > 0 && (a[i__1].r == 0. && a[i__1].i == 0.)) {
                return 0;
            }
        }
    } else {
        i__1 = *n;
        for (*info = 1; *info <= i__1; ++(*info)) {
            i__2 = *info + *info * a_dim1;
            if (ipiv[*info] > 0 && (a[i__2].r == 0. && a[i__2].i == 0.)) {
                return 0;
            }
        }
    }
    *info = 0;
    if (upper) {
        k = 1;
    L30:
        if (k > *n) {
            goto L50;
        }
        if (ipiv[k] > 0) {
            i__1 = k + k * a_dim1;
            i__2 = k + k * a_dim1;
            d__1 = 1. / a[i__2].r;
            a[i__1].r = d__1, a[i__1].i = 0.;
            if (k > 1) {
                i__1 = k - 1;
                zcopy_(&i__1, &a[k * a_dim1 + 1], &c__1, &work[1], &c__1);
                i__1 = k - 1;
                z__1.r = -1., z__1.i = -0.;
                zhemv_(uplo, &i__1, &z__1, &a[a_offset], lda, &work[1], &c__1, &c_b2,
                       &a[k * a_dim1 + 1], &c__1, (ftnlen)1);
                i__1 = k + k * a_dim1;
                i__2 = k + k * a_dim1;
                i__3 = k - 1;
                zdotc_(&z__2, &i__3, &work[1], &c__1, &a[k * a_dim1 + 1], &c__1);
                d__1 = z__2.r;
                z__1.r = a[i__2].r - d__1, z__1.i = a[i__2].i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            }
            kstep = 1;
        } else {
            t = z_lmp_abs(&a[k + (k + 1) * a_dim1]);
            i__1 = k + k * a_dim1;
            ak = a[i__1].r / t;
            i__1 = k + 1 + (k + 1) * a_dim1;
            akp1 = a[i__1].r / t;
            i__1 = k + (k + 1) * a_dim1;
            z__1.r = a[i__1].r / t, z__1.i = a[i__1].i / t;
            akkp1.r = z__1.r, akkp1.i = z__1.i;
            d__ = t * (ak * akp1 - 1.);
            i__1 = k + k * a_dim1;
            d__1 = akp1 / d__;
            a[i__1].r = d__1, a[i__1].i = 0.;
            i__1 = k + 1 + (k + 1) * a_dim1;
            d__1 = ak / d__;
            a[i__1].r = d__1, a[i__1].i = 0.;
            i__1 = k + (k + 1) * a_dim1;
            z__2.r = -akkp1.r, z__2.i = -akkp1.i;
            z__1.r = z__2.r / d__, z__1.i = z__2.i / d__;
            a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            if (k > 1) {
                i__1 = k - 1;
                zcopy_(&i__1, &a[k * a_dim1 + 1], &c__1, &work[1], &c__1);
                i__1 = k - 1;
                z__1.r = -1., z__1.i = -0.;
                zhemv_(uplo, &i__1, &z__1, &a[a_offset], lda, &work[1], &c__1, &c_b2,
                       &a[k * a_dim1 + 1], &c__1, (ftnlen)1);
                i__1 = k + k * a_dim1;
                i__2 = k + k * a_dim1;
                i__3 = k - 1;
                zdotc_(&z__2, &i__3, &work[1], &c__1, &a[k * a_dim1 + 1], &c__1);
                d__1 = z__2.r;
                z__1.r = a[i__2].r - d__1, z__1.i = a[i__2].i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
                i__1 = k + (k + 1) * a_dim1;
                i__2 = k + (k + 1) * a_dim1;
                i__3 = k - 1;
                zdotc_(&z__2, &i__3, &a[k * a_dim1 + 1], &c__1, &a[(k + 1) * a_dim1 + 1], &c__1);
                z__1.r = a[i__2].r - z__2.r, z__1.i = a[i__2].i - z__2.i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
                i__1 = k - 1;
                zcopy_(&i__1, &a[(k + 1) * a_dim1 + 1], &c__1, &work[1], &c__1);
                i__1 = k - 1;
                z__1.r = -1., z__1.i = -0.;
                zhemv_(uplo, &i__1, &z__1, &a[a_offset], lda, &work[1], &c__1, &c_b2,
                       &a[(k + 1) * a_dim1 + 1], &c__1, (ftnlen)1);
                i__1 = k + 1 + (k + 1) * a_dim1;
                i__2 = k + 1 + (k + 1) * a_dim1;
                i__3 = k - 1;
                zdotc_(&z__2, &i__3, &work[1], &c__1, &a[(k + 1) * a_dim1 + 1], &c__1);
                d__1 = z__2.r;
                z__1.r = a[i__2].r - d__1, z__1.i = a[i__2].i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            }
            kstep = 2;
        }
        kp = (i__1 = ipiv[k], abs(i__1));
        if (kp != k) {
            i__1 = kp - 1;
            zswap_(&i__1, &a[k * a_dim1 + 1], &c__1, &a[kp * a_dim1 + 1], &c__1);
            i__1 = k - 1;
            for (j = kp + 1; j <= i__1; ++j) {
                d_lmp_cnjg(&z__1, &a[j + k * a_dim1]);
                temp.r = z__1.r, temp.i = z__1.i;
                i__2 = j + k * a_dim1;
                d_lmp_cnjg(&z__1, &a[kp + j * a_dim1]);
                a[i__2].r = z__1.r, a[i__2].i = z__1.i;
                i__2 = kp + j * a_dim1;
                a[i__2].r = temp.r, a[i__2].i = temp.i;
            }
            i__1 = kp + k * a_dim1;
            d_lmp_cnjg(&z__1, &a[kp + k * a_dim1]);
            a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            i__1 = k + k * a_dim1;
            temp.r = a[i__1].r, temp.i = a[i__1].i;
            i__1 = k + k * a_dim1;
            i__2 = kp + kp * a_dim1;
            a[i__1].r = a[i__2].r, a[i__1].i = a[i__2].i;
            i__1 = kp + kp * a_dim1;
            a[i__1].r = temp.r, a[i__1].i = temp.i;
            if (kstep == 2) {
                i__1 = k + (k + 1) * a_dim1;
                temp.r = a[i__1].r, temp.i = a[i__1].i;
                i__1 = k + (k + 1) * a_dim1;
                i__2 = kp + (k + 1) * a_dim1;
                a[i__1].r = a[i__2].r, a[i__1].i = a[i__2].i;
                i__1 = kp + (k + 1) * a_dim1;
                a[i__1].r = temp.r, a[i__1].i = temp.i;
            }
        }
        k += kstep;
        goto L30;
    L50:;
    } else {
        k = *n;
    L60:
        if (k < 1) {
            goto L80;
        }
        if (ipiv[k] > 0) {
            i__1 = k + k * a_dim1;
            i__2 = k + k * a_dim1;
            d__1 = 1. / a[i__2].r;
            a[i__1].r = d__1, a[i__1].i = 0.;
            if (k < *n) {
                i__1 = *n - k;
                zcopy_(&i__1, &a[k + 1 + k * a_dim1], &c__1, &work[1], &c__1);
                i__1 = *n - k;
                z__1.r = -1., z__1.i = -0.;
                zhemv_(uplo, &i__1, &z__1, &a[k + 1 + (k + 1) * a_dim1], lda, &work[1], &c__1,
                       &c_b2, &a[k + 1 + k * a_dim1], &c__1, (ftnlen)1);
                i__1 = k + k * a_dim1;
                i__2 = k + k * a_dim1;
                i__3 = *n - k;
                zdotc_(&z__2, &i__3, &work[1], &c__1, &a[k + 1 + k * a_dim1], &c__1);
                d__1 = z__2.r;
                z__1.r = a[i__2].r - d__1, z__1.i = a[i__2].i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            }
            kstep = 1;
        } else {
            t = z_lmp_abs(&a[k + (k - 1) * a_dim1]);
            i__1 = k - 1 + (k - 1) * a_dim1;
            ak = a[i__1].r / t;
            i__1 = k + k * a_dim1;
            akp1 = a[i__1].r / t;
            i__1 = k + (k - 1) * a_dim1;
            z__1.r = a[i__1].r / t, z__1.i = a[i__1].i / t;
            akkp1.r = z__1.r, akkp1.i = z__1.i;
            d__ = t * (ak * akp1 - 1.);
            i__1 = k - 1 + (k - 1) * a_dim1;
            d__1 = akp1 / d__;
            a[i__1].r = d__1, a[i__1].i = 0.;
            i__1 = k + k * a_dim1;
            d__1 = ak / d__;
            a[i__1].r = d__1, a[i__1].i = 0.;
            i__1 = k + (k - 1) * a_dim1;
            z__2.r = -akkp1.r, z__2.i = -akkp1.i;
            z__1.r = z__2.r / d__, z__1.i = z__2.i / d__;
            a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            if (k < *n) {
                i__1 = *n - k;
                zcopy_(&i__1, &a[k + 1 + k * a_dim1], &c__1, &work[1], &c__1);
                i__1 = *n - k;
                z__1.r = -1., z__1.i = -0.;
                zhemv_(uplo, &i__1, &z__1, &a[k + 1 + (k + 1) * a_dim1], lda, &work[1], &c__1,
                       &c_b2, &a[k + 1 + k * a_dim1], &c__1, (ftnlen)1);
                i__1 = k + k * a_dim1;
                i__2 = k + k * a_dim1;
                i__3 = *n - k;
                zdotc_(&z__2, &i__3, &work[1], &c__1, &a[k + 1 + k * a_dim1], &c__1);
                d__1 = z__2.r;
                z__1.r = a[i__2].r - d__1, z__1.i = a[i__2].i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
                i__1 = k + (k - 1) * a_dim1;
                i__2 = k + (k - 1) * a_dim1;
                i__3 = *n - k;
                zdotc_(&z__2, &i__3, &a[k + 1 + k * a_dim1], &c__1, &a[k + 1 + (k - 1) * a_dim1],
                       &c__1);
                z__1.r = a[i__2].r - z__2.r, z__1.i = a[i__2].i - z__2.i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
                i__1 = *n - k;
                zcopy_(&i__1, &a[k + 1 + (k - 1) * a_dim1], &c__1, &work[1], &c__1);
                i__1 = *n - k;
                z__1.r = -1., z__1.i = -0.;
                zhemv_(uplo, &i__1, &z__1, &a[k + 1 + (k + 1) * a_dim1], lda, &work[1], &c__1,
                       &c_b2, &a[k + 1 + (k - 1) * a_dim1], &c__1, (ftnlen)1);
                i__1 = k - 1 + (k - 1) * a_dim1;
                i__2 = k - 1 + (k - 1) * a_dim1;
                i__3 = *n - k;
                zdotc_(&z__2, &i__3, &work[1], &c__1, &a[k + 1 + (k - 1) * a_dim1], &c__1);
                d__1 = z__2.r;
                z__1.r = a[i__2].r - d__1, z__1.i = a[i__2].i;
                a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            }
            kstep = 2;
        }
        kp = (i__1 = ipiv[k], abs(i__1));
        if (kp != k) {
            if (kp < *n) {
                i__1 = *n - kp;
                zswap_(&i__1, &a[kp + 1 + k * a_dim1], &c__1, &a[kp + 1 + kp * a_dim1], &c__1);
            }
            i__1 = kp - 1;
            for (j = k + 1; j <= i__1; ++j) {
                d_lmp_cnjg(&z__1, &a[j + k * a_dim1]);
                temp.r = z__1.r, temp.i = z__1.i;
                i__2 = j + k * a_dim1;
                d_lmp_cnjg(&z__1, &a[kp + j * a_dim1]);
                a[i__2].r = z__1.r, a[i__2].i = z__1.i;
                i__2 = kp + j * a_dim1;
                a[i__2].r = temp.r, a[i__2].i = temp.i;
            }
            i__1 = kp + k * a_dim1;
            d_lmp_cnjg(&z__1, &a[kp + k * a_dim1]);
            a[i__1].r = z__1.r, a[i__1].i = z__1.i;
            i__1 = k + k * a_dim1;
            temp.r = a[i__1].r, temp.i = a[i__1].i;
            i__1 = k + k * a_dim1;
            i__2 = kp + kp * a_dim1;
            a[i__1].r = a[i__2].r, a[i__1].i = a[i__2].i;
            i__1 = kp + kp * a_dim1;
            a[i__1].r = temp.r, a[i__1].i = temp.i;
            if (kstep == 2) {
                i__1 = k + (k - 1) * a_dim1;
                temp.r = a[i__1].r, temp.i = a[i__1].i;
                i__1 = k + (k - 1) * a_dim1;
                i__2 = kp + (k - 1) * a_dim1;
                a[i__1].r = a[i__2].r, a[i__1].i = a[i__2].i;
                i__1 = kp + (k - 1) * a_dim1;
                a[i__1].r = temp.r, a[i__1].i = temp.i;
            }
        }
        k -= kstep;
        goto L60;
    L80:;
    }
    return 0;
}
#ifdef __cplusplus
}
#endif
