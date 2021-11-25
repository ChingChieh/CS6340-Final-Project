#include <stdlib.h>
#include <string.h>

void allo(char **s) {
  *s = malloc(6);
  strcpy(*s, "apple");
}

int main() {
  char *ptr2;
  allo(&ptr2);
}
