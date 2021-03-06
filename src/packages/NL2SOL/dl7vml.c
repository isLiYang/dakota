/* dl7vml.f -- translated by f2c (version 19970211).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* Subroutine */ int dl7vml_(n, x, l, y)
integer *n;
doublereal *x, *l, *y;
{
    /* System generated locals */
    integer i__1, i__2;

    /* Local variables */
    static integer i__, j;
    static doublereal t;
    static integer i0, ii, ij, np1;


/*  ***  COMPUTE  X = L*Y, WHERE  L  IS AN  N X N  LOWER TRIANGULAR */
/*  ***  MATRIX STORED COMPACTLY BY ROWS.  X AND Y MAY OCCUPY THE SAME */
/*  ***  STORAGE.  *** */

/*     DIMENSION L(N*(N+1)/2) */
/* /6 */
/*     DATA ZERO/0.D+0/ */
/* /7 */
/* / */

    /* Parameter adjustments */
    --y;
    --x;
    --l;

    /* Function Body */
    np1 = *n + 1;
    i0 = *n * (*n + 1) / 2;
    i__1 = *n;
    for (ii = 1; ii <= i__1; ++ii) {
	i__ = np1 - ii;
	i0 -= i__;
	t = 0.;
	i__2 = i__;
	for (j = 1; j <= i__2; ++j) {
	    ij = i0 + j;
	    t += l[ij] * y[j];
/* L10: */
	}
	x[i__] = t;
/* L20: */
    }
/* L999: */
    return 0;
/*  ***  LAST CARD OF DL7VML FOLLOWS  *** */
} /* dl7vml_ */

