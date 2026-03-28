/* The Computer Language Benchmarks Game
 * https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
 *
 * contributed by Ledrug Katz
 *
 *   compile: gcc -Wall -O3 -fomit-frame-pointer -lm -mfpmath=sse
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define pi 3.141592653589793
#define solar_mass (4 * pi * pi)
#define year 365.24
#define for_k for(k = 0; k < 3; k++)

typedef struct planet { double x[3], v[3], mass; } planet;

void advance(int nbodies, struct planet *bodies, double dt, int steps)
{
   int i, j;
   register planet *a, *b;
   double d[3], d2, mag;

   while (steps--) {
      for (a = bodies, i = 0; i < nbodies; a++, i++) {
         for (b = a + 1, j = i + 1; j < nbodies; b++, j++) {
            d[0] = a->x[0] - b->x[0];
            d[1] = a->x[1] - b->x[1];
            d[2] = a->x[2] - b->x[2];

            d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            mag = dt / (d2 * sqrt(d2));

            a->v[0] -= d[0] * b->mass * mag;
            a->v[1] -= d[1] * b->mass * mag;
            a->v[2] -= d[2] * b->mass * mag;

            b->v[0] += d[0] * a->mass * mag;
            b->v[1] += d[1] * a->mass * mag;
            b->v[2] += d[2] * a->mass * mag;
         }
      }

      for (a = bodies, i = 0; i < nbodies; i++, a++) {
         a->x[0] += dt * a->v[0];
         a->x[1] += dt * a->v[1];
         a->x[2] += dt * a->v[2];
      }
   }
}

double energy(int nbodies, planet *bodies)
{
   double e, d[3];
   int i, j, k;
   planet *a, *b;

   e = 0.0;
   for (i = 0, a = bodies; i < nbodies; a++, i++) {
      for_k { e += a->mass * a->v[k] * a->v[k] / 2; }

      for (j = i + 1, b = a + 1; j < nbodies; b++, j++) {
         for_k { d[k] = a->x[k] - b->x[k]; }
         e -= (a->mass * b->mass) /
            sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      }
   }
   return e;
}

void offset_momentum(int nbodies, planet *bodies)
{
   int i, k;
   for (i = 0; i < nbodies; i++)
      for_k { bodies[0].v[k] -= bodies[i].v[k] * bodies[i].mass / solar_mass; }
}

struct planet bodies[] = {
   {   /* sun */
      {0, 0, 0},
      {0, 0, 0},
      solar_mass
   }, {   /* jupiter */
      { 4.84143144246472090e+00, -1.16032004402742839e+00, -1.03622044471123109e-01 },
      {
         1.66007664274403694e-03 * year,
         7.69901118419740425e-03 * year,
         -6.90460016972063023e-05 * year
      },
      9.54791938424326609e-04 * solar_mass
   }, {   /* saturn */
      { 8.34336671824457987e+00, 4.12479856412430479e+00, -4.03523417114321381e-01 },
      {
         -2.76742510726862411e-03 * year,
         4.99852801234917238e-03 * year,
         2.30417297573763929e-05 * year
      },
      2.85885980666130812e-04 * solar_mass
   }, {   /* uranus */
      { 1.28943695621391310e+01, -1.51111514016986312e+01, -2.23307578892655734e-01 },
      {
         2.96460137564761618e-03 * year,
         2.37847173959480950e-03 * year,
         -2.96589568540237556e-05 * year
      },
      4.36624404335156298e-05 * solar_mass
   }, {   /* neptune */
      { 1.53796971148509165e+01, -2.59193146099879641e+01, 1.79258772950371181e-01 },
      {
         2.68067772490389322e-03 * year,
         1.62824170038242295e-03 * year,
         -9.51592254519715870e-05 * year
      },
      5.15138902046611451e-05 * solar_mass
   }
};

#define N sizeof(bodies)/sizeof(planet)

int main(int argc, char **argv)
{
   int n = atoi(argv[1]);

   offset_momentum(N, bodies);
   printf("%.9f\n", energy(N, bodies));

   advance(N, bodies, 0.01, n);

   printf("%.9f\n", energy(N, bodies));
   return 0;
}
