void increOne(int *m) {
  *m += 1;
}

int main() {
  int x = 2021;
  int *ptr1 = &x;
  increOne(ptr1);
}
