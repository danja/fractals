/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <getopt.h>

#ifdef __MINGW__
#include <windows.h>
#endif
#include <GL/glut.h>

#define VERSION_STRING "Polynomial strange attractors - version 1.0"
#define DEFAULT_X 800
#define DEFAULT_Y 600
#define DEFAULT_POINTS 65536
#define DEFAULT_ITER 8192
#define DEFAULT_ORDER 2
#define DEFAULT_DIM 3
#define NUM_CONVERGENCE_POINTS 128
#define AT_INFINITY 1000000

typedef long double *point;

struct lyapu
{
    long double lsum;
    int n;
    long double ly;
};

struct polynom
{
    long double **p;
    int length;
    int order;
};

struct attractor
{
    struct polynom *polynom;
    struct lyapu *lyapunov;
    point *array;
    int convergenceIterations;
    int numPoints;
    point bound[2];
};

struct fractal_settings
{
    unsigned int numPoints;
    unsigned int convergenceIterations;
    unsigned int order;
    unsigned int dimension;
};

struct display_settings
{
    unsigned long int w;        /* width of current window (in pixels) */
    unsigned long int h;        /* height of current window (in pixels) */
    int fullscreen;
};

const char *WINDOW_TITLE = "Strange Attractors";
static struct fractal_settings fset;
static struct display_settings dset;
static struct attractor *at;
static float angle = 3.0;
static double r = 0;

/* Probably best algo as we are dealing with low exponents here (typically < 5)
     so no need to bother with exponentation by squaring */
inline long double
power (long double base, unsigned int exp)
{
    int i;
    long double result = 1.0;

    for (i = 0; i < exp; i++)
        result *= base;
    return result;
}

inline point
newPoint (void)
{
    point p;

    if ((p = malloc (fset.dimension * (sizeof *p))) == NULL) {
        fprintf (stderr, "Unable to allocate memory for point.\n");
        fprintf (stderr,
                 "I'm trying to go on, but expect a crash pretty soon :-)\n");
    }
    return p;
}

inline point
_scalar_mul (point p, long double m)
{
    int i;

    for (i = 0; i < fset.dimension; i++)
        p[i] *= m;

    return p;
}

inline long double
_modulus (point p)
{
    long double m = 0;
    int i;

    for (i = 0; i < fset.dimension; i++)
        m += p[i] * p[i];

    return m;
}

inline long double
_abs (point p)
{
    long double a = 0;
    int i;

    for (i = 0; i < fset.dimension; i++)
        a += fabsl (p[i]);

    return a;
}

inline point
_sub (point a, point b)
{
    point c = newPoint ();
    int i;

    for (i = 0; i < fset.dimension; i++)
        c[i] = a[i] - b[i];

    return c;
}

inline point
_middle (point a, point b)
{
    point c = newPoint ();
    int i;

    for (i = 0; i < fset.dimension; i++)
        c[i] = (a[i] + b[i]) / 2;

    return c;
}

point
eval (point p, struct polynom * polynom)
{
    int coef, i, j, n;
    long double result, *c;
    point pe = newPoint ();

    for (coef = 0; coef < fset.dimension; coef++) {
        n = 0;
        result = 0;
        c = (long double *) polynom->p[coef];
        for (i = 0; i <= polynom->order; i++) {
            for (j = 0; j <= polynom->order - i; j++) {
                if (fset.dimension == 2)
                    result += c[n++] * power (p[0], j) * power (p[1], i);
                else {
                    int k;
                    for (k = 0; k <= polynom->order - i - j; k++) {
                        result +=
                            c[n++] * power (p[0], k) * power (p[1],
                                                              j) *
                            power (p[2], i);
                    }
                }
            }
        }
        pe[coef] = result;
    }
    return pe;
}

void
displayPoint (point p)
{
    fprintf (stdout, "0x%08x : [%.10Lf,%.10Lf,%.10Lf]\n", (int) p, p[0], p[1],
             p[2]);
}

point
computeLyapunov (point p, point pe, struct attractor *at)
{
    point p2, dl, np;
    long double dl2, df, rs;
    struct lyapu *lyapu = at->lyapunov;

    p2 = eval (pe, at->polynom);
    dl = _sub (p2, p);
    dl2 = _modulus (dl);

    if (dl2 == 0) {
        fprintf (stderr,
                 "Unable to compute Lyapunov exponent, trying to go on...\n");
        free (dl);
        free (p2);
        return pe;
    }

    df = 1000000000000 * dl2;
    rs = 1 / sqrt (df);

    lyapu->lsum += log (df);
    lyapu->n++;
    lyapu->ly = lyapu->lsum / lyapu->n / log (2);

    np = _sub (p, _scalar_mul (dl, rs));

    free (dl);
    free (p2);

    return np;
}

int
checkConvergence (struct attractor *at)
{
    point p, pe, pnew = NULL;
    int i, result = 0;

    p = newPoint ();
    pe = newPoint ();
    for (i = 0; i < fset.dimension; i++)
        p[i] = pe[i] = 0.1;
    pe[0] += 0.000001;
    at->lyapunov->lsum = at->lyapunov->ly = at->lyapunov->n = 0;

    for (i = 0; i < at->convergenceIterations; i++) {
        pnew = eval (p, at->polynom);

        if (_abs (pnew) > AT_INFINITY) {        /* Diverging - not an SA */
            break;
        }
        point ptmp = _sub (pnew, p);
        if (_abs (ptmp) < 1 / AT_INFINITY) {    /* Fixed point - not an SA */
            free (ptmp);
            break;
        }
        free (ptmp);
        ptmp = computeLyapunov (pnew, pe, at);
        free (pe);
        pe = ptmp;
        if (at->lyapunov->ly < 0.005 && i >= NUM_CONVERGENCE_POINTS) {  /* Limit cycle - not an SA */
            break;
        }
        free (p);
        p = pnew;
    }
    if (i == at->convergenceIterations)
        result = 1;
    free (pnew);
    free (pe);
    free (p);                   // FIXME:  p=pnew -> multiple  free here... Will this ever compile on Linux ?
    return result;
}

inline int
factorial (n)
{
    int r = 1, i;

    for (i = 1; i <= n; i++) {
        r *= i;
    }

    return r;
}

inline int
getPolynomLength (int dim, int order)
{
    return factorial (order + dim) / factorial (order) / factorial (dim);
}

void
freePolynom (struct polynom *p)
{
    int i;

    for (i = 0; i < fset.dimension; i++) {
        free (p->p[i]);
    }
    free (p->p);
    free (p);
}

void
displayPolynom (struct polynom *p)
{
    int i, j;

    for (i = 0; i < fset.dimension; i++) {
        fprintf (stdout, "[ ");
        for (j = 0; j < p->length; j++) {
            fprintf (stdout, "%+.2Lf ", (p->p[i])[j]);
        }
        fprintf (stdout, "]\n");
    }
}

void
getRandom (int order, struct polynom *p)
{
    int i, j;

    for (i = 0; i < fset.dimension; i++) {
        for (j = 0; j < p->length; j++) {
            (p->p[i])[j] = (double) ((rand () % 61) - 30) * 0.08;
        }
    }
}

void
explore (struct attractor *at, int order)
{
    while (1) {
        getRandom (order, at->polynom);
        if (checkConvergence (at)) {
            break;
        }
    }
}

void
iterateMap (struct attractor *at)
{
    point p, pnew, pmin, pmax, ptmp;
    int i, j;

    if ((at->array = malloc (at->numPoints * (sizeof *(at->array)))) == NULL) {
        fprintf (stderr,
                 "Unable to allocate memory for point array. Exiting\n");
        exit (EXIT_FAILURE);
    }

    p = newPoint ();
    pmin = newPoint ();
    pmax = newPoint ();
    ptmp = p;
    for (i = 0; i < fset.dimension; i++) {
        p[i] = 0.1;
        pmin[i] = AT_INFINITY;
        pmax[i] = -AT_INFINITY;
    }

    for (i = 0; i < at->numPoints; i++) {
        pnew = eval (p, at->polynom);
        p = pnew;
        if (i >= NUM_CONVERGENCE_POINTS) {
            at->array[i - NUM_CONVERGENCE_POINTS] = pnew;
            for (j = 0; j < fset.dimension; j++) {
                pmin[j] = min (p[j], pmin[j]);
                pmax[j] = max (p[j], pmax[j]);
            }
        }
    }
    free (ptmp);
    at->bound[0] = pmin;
    at->bound[1] = pmax;
    at->numPoints -= NUM_CONVERGENCE_POINTS;
}

void
freeAttractor (struct attractor *at)
{
    int i;

    free (at->lyapunov);
    for (i = 0; i < at->numPoints; i++) {
        free (at->array[i]);
    }
    free (at->array);
    for (i = 0; i < 2; i++) {
        free (at->bound[i]);
    }

    freePolynom (at->polynom);
    free (at);
}

void
centerAttractor (struct attractor *at)
{
    int i, j;

    point m = _middle (at->bound[0], at->bound[1]);
    for (i = 0; i < at->numPoints; i++) {
        for (j = 0; j < fset.dimension; j++) {
            at->array[i][j] -= m[j];
        }
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < fset.dimension; j++) {
            at->bound[i][j] -= m[j];
        }
    }
    free (m);
}

struct attractor *
newAttractor (void)
{
    int i;
	
    if ((at = malloc (sizeof *at)) == NULL) {
        fprintf (stderr,
                 "Unable to allocate memory for attractor. Exiting\n");
        exit (EXIT_FAILURE);
    }

    if ((at->lyapunov = malloc (sizeof *(at->lyapunov))) == NULL) {
        fprintf (stderr,
                 "Unable to allocate memory for lyapunov structure. Exiting\n");
        exit (EXIT_FAILURE);
    }

    if ((at->polynom = malloc (sizeof *(at->polynom))) == NULL) {
        fprintf (stderr, "Unable to allocate memory for polynom. Exiting\n");
        exit (EXIT_FAILURE);
    }

    if ((at->polynom->p =
         malloc (fset.dimension * sizeof *(at->polynom->p))) == NULL) {
        fprintf (stderr, "Unable to allocate memory for polynom. Exiting\n");
        exit (EXIT_FAILURE);
    }
    at->polynom->order = fset.order;
    at->polynom->length = getPolynomLength (fset.dimension, fset.order);
    for (i = 0; i < fset.dimension; i++) {
        if ((at->polynom->p[i] =
             malloc (at->polynom->length * (sizeof *(at->polynom->p[i])))) ==
            NULL) {
            fprintf (stderr,
                     "Unable to allocate memory for polynom. Exiting\n");
            exit (EXIT_FAILURE);
        }
    }

    at->convergenceIterations = fset.convergenceIterations;
    at->numPoints = fset.numPoints;
    explore (at, fset.order);
    displayPolynom (at->polynom);
    fprintf (stdout, "Lyapunov exponent: %.6Lf\n", at->lyapunov->ly);
    iterateMap (at);
    centerAttractor (at);
    return at;
}

void
usage (char *prog_name, FILE * stream)
{
}

int
numbers_from_string (long *num, char *s, char separator, int n)
{
    int i;
    char *ss = s, *p = NULL;

    for (i = 0; i < n; i++) {
        num[i] = strtol (ss, &p, 0);
        if ((num[i] == 0) && (p == ss)) {
            return -1;
        }
        if (i == n - 1) {
            break;
        }
        if (*p++ != separator) {
            return -1;
        }
        ss = p;
    }
    return 0;
}

void
parse_options (int argc, char **argv)
{
    int c;

    while (1) {
        static struct option long_options[] = {
            {"conviter", required_argument, 0, 'c'},
            {"dimension", required_argument, 0, 'd'},
            {"fullscreen", no_argument, &dset.fullscreen, 1},
            {"geometry", required_argument, 0, 'g'},
            {"npoints", required_argument, 0, 'n'},
            {"order", required_argument, 0, 'o'},
            {"version", no_argument, 0, 'v'},
            {0, 0, 0, 0}
        };

        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long (argc, argv, "c:d:fg:n:o:v", long_options,
                         &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;
        switch (c) {
        case 0:
            break;
        case 'c':
            fset.convergenceIterations = strtol (optarg, NULL, 0);
            break;
        case 'd':
            fset.dimension = strtol (optarg, NULL, 0);
            if (fset.dimension < 2) {
                fprintf (stderr, "Specified dimension out of bound\n");
                usage (argv[0], stderr);
                exit (EXIT_FAILURE);
            }
            break;
        case 'g':
            {
                long n[2];
                if (numbers_from_string (n, optarg, 'x', 2) == -1) {
                    fprintf (stderr, "Bad geometry string\n");
                    exit (EXIT_FAILURE);
                }
                dset.w = n[0];
                dset.h = n[1];
                break;
            }

        case 'n':
            fset.numPoints = strtol (optarg, NULL, 0);
            break;

        case 'o':
            fset.order = strtol (optarg, NULL, 0);
            break;

        case 'h':
            usage (argv[0], stdout);
            exit (EXIT_SUCCESS);

        case 'v':
            fprintf (stdout, "%s\n", VERSION_STRING);
            exit (EXIT_SUCCESS);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            abort ();
        }
    }

    /* Print any remaining command line arguments (not options). */
    if (optind < argc) {
        while (optind < argc)
            fprintf (stderr,
                     "%s is not recognized as a valid option or argument\n",
                     argv[optind++]);
        usage (argv[0], stderr);
    }
}

void
default_settings (void)
{
    dset.w = DEFAULT_X;
    dset.h = DEFAULT_Y;
    dset.fullscreen = 0;
    fset.numPoints = DEFAULT_POINTS;
    fset.convergenceIterations = DEFAULT_ITER;
    fset.order = DEFAULT_ORDER;
    fset.dimension = DEFAULT_DIM;
}

void
initDisplay ()
{
    int i;

    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    glColor4f (1.0f, 1.0f, 1.0f, 0.02f);
    glViewport (0, 0, dset.w, dset.h);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();

    /* The square of the diagonal length */
    for (i = 0; i < fset.dimension; i++) {
        r += (at->bound[1][i] - at->bound[0][i]) * (at->bound[1][i] -
                                                    at->bound[0][i]);
    }
    /* Add 2% to move the clipping planes a bit further */
    r = 0.51 * sqrt (r);
    glOrtho (-r, r, -r, r, -r, r);

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    if (fset.dimension == 2) {
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glEnable (GL_POINT_SMOOTH);
    glPointSize (1.0f);
}

void
display ()
{
    int i;

    glClear (GL_COLOR_BUFFER_BIT);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    glRotatef (angle, 1.0, 1.0, 1.0);

    glBegin (GL_POINTS);
    for (i = 0; i < at->numPoints; i++) {
        if (fset.dimension == 2)
            glVertex2f (at->array[i][0], at->array[i][1]);
        else
            glVertex3f (at->array[i][0], at->array[i][1], at->array[i][2]);
    }
    glEnd ();

    glutSwapBuffers ();
    angle += 1.0;
}

/* Keep aspect ratio */
void
reshape (int w, int h)
{
    GLdouble ar;

    glViewport (0, 0, w, h);
    ar = (GLdouble) w / (GLdouble) h;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();

    if (ar < 1.0) {
        glOrtho (-r, r, -r / ar, r / ar, -r, r);
    }
    else {
        glOrtho (-r * ar, r * ar, -r, r, -r, r);
    }
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
}

void
key (unsigned char mychar, int x, int y)
{
    // exit the program when the Esc key is pressed
    if (mychar == 27) {
        exit (0);
    }
}

void
animate (int argc, char **argv)
{
    glutInit (&argc, argv);
    glutInitDisplayMode (GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);

    glutInitWindowSize (dset.w, dset.h);
    glutCreateWindow (WINDOW_TITLE);

    /* Even if there are no events, redraw our gl scene. */
    glutIdleFunc (display);
    glutDisplayFunc (display);
    glutKeyboardFunc (key);
    glutReshapeFunc (reshape);

    initDisplay ();
    glutMainLoop ();
}

int
main (int argc, char **argv)
{
#ifdef __MINGW__
    freopen ("CON", "w", stdout);
    freopen ("CON", "w", stderr);
#endif

    srand (time (NULL));
    default_settings ();
    parse_options (argc, argv);
    at = newAttractor ();
    animate (argc, argv);
    freeAttractor (at);
    return EXIT_SUCCESS;
}
