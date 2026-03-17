#include "thread_pool.h"
#include "future.h"

int main() {
    ThreadPool pool(2);
    auto future = pool.Submit([] { return 42; });
    return future.get() == 42 ? 0 : 1;
}
