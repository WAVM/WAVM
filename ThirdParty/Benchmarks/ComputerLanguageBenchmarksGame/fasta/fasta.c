/* The Computer Language Benchmarks Game
 * https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
 *
 * by Drake Diedrich
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IM 139968
#define IA   3877
#define IC  29573
#define SEED   42
static uint32_t seed = SEED;
#define fasta_rand(max) ( seed = (seed * IA + IC ) % IM, max * seed / IM )

#ifndef LOOKUP
#define LOOKUP linear_lookup
#endif

static const char *alu =
  "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"
  "GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA"
  "CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT"
  "ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA"
  "GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG"
  "AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC"
  "AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";

static const char *iub = "acgtBDHKMNRSVWY";
static const double iub_p[] = {
  0.27,
  0.12,
  0.12,
  0.27,
  0.02,
  0.02,
  0.02,
  0.02,
  0.02,
  0.02,
  0.02,
  0.02,
  0.02,
  0.02,
  0.02 };

static const char *homosapiens = "acgt";
static const double homosapiens_p[] = {
  0.3029549426680,
  0.1979883004921,
  0.1975473066391,
  0.3015094502008
};

#define LINELEN 60

static void repeat_fasta(const char *seq,
       const int n) {
  const int len = strlen(seq);
  char *buffer = malloc(len + LINELEN);
  int i;
  if (LINELEN < len) {
    memcpy(buffer,seq,len);
    memcpy(buffer+len, seq, LINELEN);
  } else {
    for (i=0; i < LINELEN/len; i++) memcpy(buffer+i*len,seq, len);
    memcpy(buffer+i*len, seq, n - i*n);
  }

  int t = 0;
  int whole_lines = n / LINELEN;
  for (i=0; i<whole_lines; i++) {
    int wrote = fwrite(&buffer[t % len], 1, LINELEN, stdout);
    putchar('\n');
    t += wrote;
  }
  fwrite(&buffer[t % len], 1, n-t, stdout);
  free(buffer);
  if (n % LINELEN != 0) putchar('\n');
}

static int linear_lookup(const int len, const double *p, const double v) {
  int i = 0;
  for (int j=0; j<len-1; j++) if (v > p[j]) i++;
  return i;
}

static int linear_lookup2(const int len, const double *p, const double v) {
  int j;
  for (j=0; j<len-1; j++) if (v > p[j]) break;
  return j;
}

static int bisect_lookup(const int len, const double *p, const double v) {
  int low = 0;
  int high = len-1;
  int mid;
  if (v<p[0]) return 0;
  while (high > low+1) {
    mid = (low+high)/2;
    if (v < p[mid]) {
      high = mid;
    } else {
      low = mid;
    }
  }
  return high;
}

static void random_fasta(const char *symb,
       const double *probability,
       const int n) {
  const int len = strlen(symb);
  double *p = malloc(sizeof(*p) * len);
  int i,k;
  char *buffer = malloc(LINELEN+2);
  buffer[LINELEN] = '\n';

  p[0] = probability[0];
  for (i=1; i<len;i++) p[i] = probability[i] + p[i-1];

  for (i=0; i<n/LINELEN; i++) {
    for (k=0; k<LINELEN;k++) {
      double v = fasta_rand(1.0);
      buffer[k] = symb[LOOKUP(len,p,v)];
    }
    fwrite(buffer,1,LINELEN+1,stdout);
  }
  int remainder = n - (i*LINELEN);
  for (k=0; k<remainder; k++) {
    double v = fasta_rand(1.0);
    buffer[k] = symb[LOOKUP(len,p,v)];
  }
  fwrite(buffer, 1, remainder, stdout);
  if (n % LINELEN != 0) putchar('\n');
  free(buffer);
}

int main(int argc, char **argv) {
  int n=1000;
  if (argc>1) n = atoi(argv[1]);

  printf(">ONE Homo sapiens alu\n");
  repeat_fasta(alu, n*2);

  printf(">TWO IUB ambiguity codes\n");
  random_fasta(iub, iub_p, n*3);

  printf(">THREE Homo sapiens frequency\n");
  random_fasta(homosapiens, homosapiens_p, n*5);

  return 0;
}
