#include <stdio.h>
#include "libnez.h"

void dump_pego(ParsingObject *pego, char *source, int level) {
  int i;
  long j;
  if (pego[0]) {
    for (i = 0; i < level; i++) {
      fprintf(stderr, "  ");
    }
    fprintf(stderr, "{%s ", pego[0]->tag);
    if (pego[0]->child_size == 0) {
      fprintf(stderr, "'");
      if (pego[0]->value == NULL) {
        for (j = pego[0]->start_pos; j < pego[0]->end_pos; j++) {
          fprintf(stderr, "%c", source[j]);
        }
      } else {
        fprintf(stderr, "%s", pego[0]->value);
      }
      fprintf(stderr, "'");
    } else {
      fprintf(stderr, "\n");
      for (j = 0; j < pego[0]->child_size; j++) {
        dump_pego(&pego[0]->child[j], source, level + 1);
      }
      for (i = 0; i < level; i++) {
        fprintf(stderr, "  ");
      }
    }
    fprintf(stderr, "}\n");
  } else {
    fprintf(stderr, "%p tag:null\n", pego);
  }
}

void dump_pego_file(FILE *file, ParsingObject *pego, char *source, int level) {
  int i;
  long j;
  if (pego[0]) {
    for (i = 0; i < level; i++) {
      fprintf(file, " ");
    }
    fprintf(file, "{%s ", pego[0]->tag);
    if (pego[0]->child_size == 0) {
      fprintf(file, "'");
      if (pego[0]->value == NULL) {
        for (j = pego[0]->start_pos; j < pego[0]->end_pos; j++) {
          fprintf(file, "%c", source[j]);
        }
      } else {
        fprintf(file, "%s", pego[0]->value);
      }
      fprintf(file, "'");
    } else {
      fprintf(file, "\n");
      for (j = 0; j < pego[0]->child_size; j++) {
        dump_pego_file(file, &pego[0]->child[j], source, level + 1);
      }
      for (i = 0; i < level; i++) {
        fprintf(file, " ");
      }
    }
    fprintf(file, "}\n");
  } else {
    fprintf(file, "%p tag:null\n", pego);
  }
}
