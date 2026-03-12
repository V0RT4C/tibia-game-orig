#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "threads/semaphore/semaphore.h"

TEST_CASE("Semaphore: initial value and up/down") {
    Semaphore sem(1);
    sem.down(); // should not block (value was 1)
    sem.up();   // value back to 1
    sem.down(); // should not block
    sem.up();
}

TEST_CASE("Semaphore: initial value 0, up then down") {
    Semaphore sem(0);
    sem.up();
    sem.down(); // should not block (value was incremented to 1)
}
