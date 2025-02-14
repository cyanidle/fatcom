#include "fatcom/fatcom.hpp"

struct Victim {
    FAT_IMPLEMENTS(ITest, ITest2);

    int refs = 0;

    void AddRef() {
        ++refs;
    }
    void Release() {
        if (!--refs) delete this;
    }
    bool method1(int a, int b) {
        return a > b;
    }
    void method2(int c) {
        c = c;
    }
};

static_assert(sizeof(Victim) == sizeof(int));

int main(int argc, char *argv[])
{
    ITestPtr ptr(new Victim);
    ptr.method1(1, 2);
    ITest2Ptr ptr2(ptr);
    ptr2.method2(3);
    return 0;
}
