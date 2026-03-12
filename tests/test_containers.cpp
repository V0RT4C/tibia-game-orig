#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/containers/containers.h"

#include <cstring>

// Redirect error() so tests don't print to stdout and we can verify calls.
static char last_error[1024];

static void TestErrorHandler(const char *Text) {
    strncpy(last_error, Text, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = 0;
}

struct ErrorGuard {
    ErrorGuard() { last_error[0] = 0; SetErrorFunction(TestErrorHandler); }
    ~ErrorGuard() { SetErrorFunction(nullptr); }
};

// =============================================================================
// vector tests
// =============================================================================

TEST_CASE("vector: default construction and basic at()") {
    ErrorGuard eg;
    vector<int> v(0, 9, 10);

    // Write and read back through at()
    *v.at(0) = 42;
    *v.at(9) = 99;
    CHECK(*v.at(0) == 42);
    CHECK(*v.at(9) == 99);
}

TEST_CASE("vector: initialized constructor fills with init value") {
    ErrorGuard eg;
    vector<int> v(0, 4, 5, -1);

    CHECK(*v.at(0) == -1);
    CHECK(*v.at(2) == -1);
    CHECK(*v.at(4) == -1);
}

TEST_CASE("vector: auto-grow upward beyond max") {
    ErrorGuard eg;
    vector<int> v(0, 3, 4, 0);

    // Access beyond max should auto-grow
    *v.at(10) = 77;
    CHECK(*v.at(10) == 77);
    CHECK(v.max >= 10);
}

TEST_CASE("vector: auto-grow downward below min") {
    ErrorGuard eg;
    vector<int> v(0, 3, 4, 0);

    // Access below min should auto-grow
    *v.at(-5) = 33;
    CHECK(*v.at(-5) == 33);
    CHECK(v.min <= -5);
}

TEST_CASE("vector: copyAt returns value within bounds") {
    ErrorGuard eg;
    vector<int> v(0, 4, 5, 0);

    *v.at(2) = 123;
    CHECK(v.copyAt(2) == 123);
}

TEST_CASE("vector: copyAt returns init for out-of-bounds when initialized") {
    ErrorGuard eg;
    vector<int> v(0, 4, 5, -1);

    // Index 100 is out of bounds; should return init value
    CHECK(v.copyAt(100) == -1);
}

TEST_CASE("vector: copyAt returns default for out-of-bounds when not initialized") {
    ErrorGuard eg;
    vector<int> v(0, 4, 5);

    // Out-of-bounds with no init returns default-constructed T (0 for int)
    CHECK(v.copyAt(100) == 0);
}

// =============================================================================
// priority_queue tests
// =============================================================================

TEST_CASE("priority_queue: insert and deleteMin maintain min-heap order") {
    ErrorGuard eg;
    priority_queue<int, int> pq(16, 16);

    pq.insert(5, 50);
    pq.insert(3, 30);
    pq.insert(7, 70);
    pq.insert(1, 10);

    // Min should be key=1
    CHECK(pq.Entry->at(1)->Key == 1);
    CHECK(pq.Entry->at(1)->Data == 10);

    pq.deleteMin();
    // After removing min(1), new min should be key=3
    CHECK(pq.Entry->at(1)->Key == 3);
    CHECK(pq.Entry->at(1)->Data == 30);

    pq.deleteMin();
    // Now min should be key=5
    CHECK(pq.Entry->at(1)->Key == 5);
    CHECK(pq.Entry->at(1)->Data == 50);

    pq.deleteMin();
    // Now min should be key=7
    CHECK(pq.Entry->at(1)->Key == 7);
    CHECK(pq.Entry->at(1)->Data == 70);

    pq.deleteMin();
    CHECK(pq.Entries == 0);
}

TEST_CASE("priority_queue: deleteMin on empty queue calls error") {
    ErrorGuard eg;
    priority_queue<int, int> pq(4, 4);

    pq.deleteMin();
    CHECK(strcmp(last_error, "priority_queue::deleteMin: Queue is empty.\n") == 0);
}

TEST_CASE("priority_queue: single element insert and deleteMin") {
    ErrorGuard eg;
    priority_queue<int, int> pq(4, 4);

    pq.insert(42, 100);
    CHECK(pq.Entries == 1);
    CHECK(pq.Entry->at(1)->Key == 42);

    pq.deleteMin();
    CHECK(pq.Entries == 0);
}

// =============================================================================
// matrix tests
// =============================================================================

TEST_CASE("matrix: construction and at()") {
    ErrorGuard eg;
    matrix<int> m(0, 3, 0, 3);

    *m.at(0, 0) = 1;
    *m.at(3, 3) = 99;
    CHECK(*m.at(0, 0) == 1);
    CHECK(*m.at(3, 3) == 99);
}

TEST_CASE("matrix: initialized constructor fills entries") {
    ErrorGuard eg;
    matrix<int> m(0, 2, 0, 2, -1);

    CHECK(*m.at(0, 0) == -1);
    CHECK(*m.at(1, 1) == -1);
    CHECK(*m.at(2, 2) == -1);
}

TEST_CASE("matrix: non-zero origin indices") {
    ErrorGuard eg;
    matrix<int> m(10, 12, 20, 22);

    *m.at(10, 20) = 5;
    *m.at(12, 22) = 15;
    CHECK(*m.at(10, 20) == 5);
    CHECK(*m.at(12, 22) == 15);
}

TEST_CASE("matrix: boundedAt returns NULL for out-of-bounds") {
    ErrorGuard eg;
    matrix<int> m(0, 3, 0, 3);

    CHECK(m.boundedAt(-1, 0) == nullptr);
    CHECK(m.boundedAt(0, -1) == nullptr);
    CHECK(m.boundedAt(4, 0) == nullptr);
    CHECK(m.boundedAt(0, 4) == nullptr);
}

TEST_CASE("matrix: boundedAt returns valid pointer for in-bounds") {
    ErrorGuard eg;
    matrix<int> m(0, 3, 0, 3);

    *m.at(2, 2) = 42;
    int *p = m.boundedAt(2, 2);
    REQUIRE(p != nullptr);
    CHECK(*p == 42);
}

TEST_CASE("matrix: at() out-of-bounds calls error and returns entry[0]") {
    ErrorGuard eg;
    matrix<int> m(0, 3, 0, 3);

    *m.at(0, 0) = 999;
    int *p = m.at(5, 5);
    CHECK(last_error[0] != 0); // error was called
    CHECK(*p == 999); // returns &entry[0]
}

// =============================================================================
// matrix3d tests
// =============================================================================

TEST_CASE("matrix3d: construction and at()") {
    ErrorGuard eg;
    matrix3d<int> m(0, 2, 0, 2, 0, 2);

    *m.at(0, 0, 0) = 1;
    *m.at(2, 2, 2) = 27;
    CHECK(*m.at(0, 0, 0) == 1);
    CHECK(*m.at(2, 2, 2) == 27);
}

TEST_CASE("matrix3d: initialized constructor fills entries") {
    ErrorGuard eg;
    matrix3d<int> m(0, 1, 0, 1, 0, 1, -1);

    CHECK(*m.at(0, 0, 0) == -1);
    CHECK(*m.at(1, 1, 1) == -1);
}

TEST_CASE("matrix3d: at() out-of-bounds calls error") {
    ErrorGuard eg;
    matrix3d<int> m(0, 2, 0, 2, 0, 2);

    m.at(5, 0, 0);
    CHECK(last_error[0] != 0);
}

// =============================================================================
// list tests
// =============================================================================

TEST_CASE("list: default construction is empty") {
    ErrorGuard eg;
    list<int> l;

    CHECK(l.firstNode == nullptr);
    CHECK(l.lastNode == nullptr);
}

TEST_CASE("list: append and iterate") {
    ErrorGuard eg;
    list<int> l;

    listnode<int> *n1 = l.append();
    n1->data = 10;
    listnode<int> *n2 = l.append();
    n2->data = 20;
    listnode<int> *n3 = l.append();
    n3->data = 30;

    // Forward iteration
    CHECK(l.firstNode->data == 10);
    CHECK(l.firstNode->next->data == 20);
    CHECK(l.firstNode->next->next->data == 30);
    CHECK(l.firstNode->next->next->next == nullptr);

    // Backward iteration
    CHECK(l.lastNode->data == 30);
    CHECK(l.lastNode->prev->data == 20);
    CHECK(l.lastNode->prev->prev->data == 10);
    CHECK(l.lastNode->prev->prev->prev == nullptr);
}

TEST_CASE("list: remove middle node") {
    ErrorGuard eg;
    list<int> l;

    listnode<int> *n1 = l.append();
    n1->data = 10;
    listnode<int> *n2 = l.append();
    n2->data = 20;
    listnode<int> *n3 = l.append();
    n3->data = 30;

    l.remove(n2);

    CHECK(l.firstNode->data == 10);
    CHECK(l.firstNode->next->data == 30);
    CHECK(l.lastNode->data == 30);
    CHECK(l.lastNode->prev->data == 10);
}

TEST_CASE("list: remove first node") {
    ErrorGuard eg;
    list<int> l;

    listnode<int> *n1 = l.append();
    n1->data = 10;
    listnode<int> *n2 = l.append();
    n2->data = 20;

    l.remove(n1);

    CHECK(l.firstNode->data == 20);
    CHECK(l.firstNode == l.lastNode);
    CHECK(l.firstNode->prev == nullptr);
}

TEST_CASE("list: remove last node") {
    ErrorGuard eg;
    list<int> l;

    listnode<int> *n1 = l.append();
    n1->data = 10;
    listnode<int> *n2 = l.append();
    n2->data = 20;

    l.remove(n2);

    CHECK(l.firstNode->data == 10);
    CHECK(l.firstNode == l.lastNode);
    CHECK(l.lastNode->next == nullptr);
}

TEST_CASE("list: remove only node leaves empty list") {
    ErrorGuard eg;
    list<int> l;

    listnode<int> *n1 = l.append();
    n1->data = 10;

    l.remove(n1);

    CHECK(l.firstNode == nullptr);
    CHECK(l.lastNode == nullptr);
}

TEST_CASE("list: remove NULL calls error") {
    ErrorGuard eg;
    list<int> l;

    l.remove(nullptr);
    CHECK(strcmp(last_error, "list::remove: node is NULL.\n") == 0);
}

// =============================================================================
// fifo tests
// =============================================================================

TEST_CASE("fifo: empty fifo has null next()") {
    ErrorGuard eg;
    fifo<int> f(4);

    CHECK(f.next() == nullptr);
}

TEST_CASE("fifo: append and next (FIFO order)") {
    ErrorGuard eg;
    fifo<int> f(4);

    *f.append() = 10;
    *f.append() = 20;
    *f.append() = 30;

    // next() returns the front (Tail)
    REQUIRE(f.next() != nullptr);
    CHECK(*f.next() == 10);

    f.remove();
    REQUIRE(f.next() != nullptr);
    CHECK(*f.next() == 20);

    f.remove();
    REQUIRE(f.next() != nullptr);
    CHECK(*f.next() == 30);

    f.remove();
    CHECK(f.next() == nullptr);
}

TEST_CASE("fifo: remove on empty calls error") {
    ErrorGuard eg;
    fifo<int> f(4);

    f.remove();
    CHECK(strcmp(last_error, "fifo::remove: Fifo is empty.\n") == 0);
}

TEST_CASE("fifo: auto-grow when full") {
    ErrorGuard eg;
    fifo<int> f(2);

    // Fill beyond initial capacity of 2
    *f.append() = 1;
    *f.append() = 2;
    *f.append() = 3; // should trigger resize
    *f.append() = 4;

    CHECK(*f.next() == 1);
    f.remove();
    CHECK(*f.next() == 2);
    f.remove();
    CHECK(*f.next() == 3);
    f.remove();
    CHECK(*f.next() == 4);
    f.remove();
    CHECK(f.next() == nullptr);
}

TEST_CASE("fifo: iteration from head to tail") {
    ErrorGuard eg;
    fifo<int> f(8);

    *f.append() = 10;
    *f.append() = 20;
    *f.append() = 30;

    // iterNext iterates from Head towards Tail (newest to oldest)
    int pos = f.iterFirst();
    int *val;

    val = f.iterNext(&pos);
    REQUIRE(val != nullptr);
    CHECK(*val == 30);

    val = f.iterNext(&pos);
    REQUIRE(val != nullptr);
    CHECK(*val == 20);

    val = f.iterNext(&pos);
    REQUIRE(val != nullptr);
    CHECK(*val == 10);

    val = f.iterNext(&pos);
    CHECK(val == nullptr);
}

TEST_CASE("fifo: iteration from tail to head") {
    ErrorGuard eg;
    fifo<int> f(8);

    *f.append() = 10;
    *f.append() = 20;
    *f.append() = 30;

    // iterPrev iterates from Tail towards Head (oldest to newest)
    int pos = f.iterLast();
    int *val;

    val = f.iterPrev(&pos);
    REQUIRE(val != nullptr);
    CHECK(*val == 10);

    val = f.iterPrev(&pos);
    REQUIRE(val != nullptr);
    CHECK(*val == 20);

    val = f.iterPrev(&pos);
    REQUIRE(val != nullptr);
    CHECK(*val == 30);

    val = f.iterPrev(&pos);
    CHECK(val == nullptr);
}

// =============================================================================
// store tests
// =============================================================================

struct PODItem {
    int x;
    int y;
};

TEST_CASE("store: getFreeItem returns valid memory") {
    ErrorGuard eg;
    store<PODItem, 4> s;

    PODItem *item = s.getFreeItem();
    REQUIRE(item != nullptr);
    item->x = 10;
    item->y = 20;
    CHECK(item->x == 10);
    CHECK(item->y == 20);
}

TEST_CASE("store: putFreeItem and getFreeItem recycle items") {
    ErrorGuard eg;
    store<PODItem, 4> s;

    PODItem *item1 = s.getFreeItem();
    item1->x = 1;
    item1->y = 2;

    // Return item to the free list
    s.putFreeItem(item1);

    // Next allocation should return the same memory
    PODItem *item2 = s.getFreeItem();
    CHECK(item2 == item1);
}

TEST_CASE("store: allocate more than slab size triggers new unit") {
    ErrorGuard eg;
    store<PODItem, 2> s;

    // Allocate 3 items from a store with slab size 2
    PODItem *a = s.getFreeItem();
    PODItem *b = s.getFreeItem();
    PODItem *c = s.getFreeItem(); // should allocate a new storeunit

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    a->x = 1;
    b->x = 2;
    c->x = 3;

    CHECK(a->x == 1);
    CHECK(b->x == 2);
    CHECK(c->x == 3);
}
