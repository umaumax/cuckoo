#include <iostream>
#include <string>

int add(int a, int b) { return a + b; }

int main(int argc, char* argv[]) {
  printf("add=%p\n", add);
  std::cout << "add(1,2)=" << add(1, 2) << std::endl;
  return 0;
}
