#include <iostream>
#include "proqueue.h"

int main() {
    elh::proqueue<int>([](int n) {
        std::cout << n << std::endl;;
    }).push(1);
}
