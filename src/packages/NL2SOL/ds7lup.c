/* ds7lup.f -- translated by f2c (version 19970211).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* Subroutine */ int ds7lup_(a, cosmin, p, size, step, u, w, wchmtd, wscale, 
	y)
doublereal *a, *cosmin;
integer *p;
doublereal *size, *step, *u, *w, *wchmtd, *wscale, *y;
{
    /* System generated locals */
    integer i__1, i__2;
    doublereal d__1, d__2, d__3;

    /* Local variables */
    static integer i__, j, k;
    static doublereal t;
    extern doublereal dd7tpr_(), dv2nrm_();
    extern /* Subroutine */ int ds7lvm_();
    static doublereal ui, wi, denmin, sdotwm;


/*  ***  UPDATE SYMMETRIC  A  SO THAT  A * STEP = Y  *** */
/*  ***  (LOWER TRIANGLE OF  A  STORED ROWWISE       *** */

/*  ***  PARAMETER DECLARATIONS  *** */

/*     DIMENSION A(P*(P+1)/2) */

/*  ***  LOCAL VARIABLES  *** */


/*     ***  CONSTANTS  *** */

/*  ***  EXTERNAL FUNCTIONS AND SUBROUTINES  *** */


/* /6 */
/*     DATA HALF/0.5D+0/, ONE/1.D+0/, ZERO/0.D+0/ */
/* /7 */
/* / */

/* -----------------------------------------------------------------------
 */

    /* Parameter adjustments */
    --a;
    --y;
    --wchmtd;
    --w;
    --u;
    --step;

    /* Function Body */
    sdotwm = dd7tpr_(p, &step[1], &wchmtd[1]);
    denmin = *cosmin * dv2nrm_(p, &step[1]) * dv2nrm_(p, &wchmtd[1]);
    *wscale = 1.;
    if (denmin != 0.) {
/* Computing MIN */
	d__2 = 1., d__3 = (d__1 = sdotwm / denmin, abs(d__1));
	*wscale = min(d__2,d__3);
    }
    t = 0.;
    if (sdotwm != 0.) {
	t = *wscale / sdotwm;
    }
    i__1 = *p;
    for (i__ = 1; i__ <= i__1; ++i__) {
/* L10: */
	w[i__] = t * wchmtd[i__];
    }
    ds7lvm_(p, &u[1], &a[1], &step[1]);
    t = (*size * dd7tpr_(p, &step[1], &u[1]) - dd7tpr_(p, &step[1], &y[1])) * 
	    .5;
    i__1 = *p;
    for (i__ = 1; i__ <= i__1; ++i__) {
/* L20: */
	u[i__] = t * w[i__] + y[i__] - *size * u[i__];
    }

/*  ***  SET  A = A + U*(W**T) + W*(U**T)  *** */

    k = 1;
    i__1 = *p;
    for (i__ = 1; i__ <= i__1; ++i__) {
	ui = u[i__];
	wi = w[i__];
	i__2 = i__;
	for (j = 1; j <= i__2; ++j) {
	    a[k] = *size * a[k] + ui * w[j] + wi * u[j];
	    ++k;
/* L30: */
	}
/* L40: */
    }

/* L999: */
    return 0;
/*  ***  LAST CARD OF DS7LUP FOLLOWS  *** */
} /* ds7lup_ */

