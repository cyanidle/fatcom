#include "fatcom/fatcom.hpp"
#include <cassert>

FAT_UUID(ITest, "00112233-4455-6677-8899-aabbccddeeff")
FAT_INTERFACE(ITest,
              (bool, method1, (int)x, (int)y)
              )

FAT_UUID(ITest2, "00212233-4455-6677-8899-aabbccddeeff")
FAT_INTERFACE(ITest2,
              (void, method2, (int)x),
              (void, kek)
              )

FAT_UUID(ITest4, "01212233-4455-6677-8899-aabbccddeeff")
FAT_INTERFACE(ITest4,
              (void, method4)
              )

FAT_UUID(ITest3, "00312233-4455-6677-8899-aabbccddeeff")
FAT_INTERFACE_INHERIT(ITest2, ITest3,
                      (void, method3, (bool)x)
                      )

struct Victim {
    FAT_IMPLEMENTS(ITest, ITest3);

    int refs = 0;

    void AddRef() {
        ++refs;
    }
    void Release() {
        if (!--refs)
            delete this;
    }
    bool method1(int a, int b) {
        return a > b;
    }
    void method2(int c) {
        c = c;
    }
    void method3(bool c) {
        c = c;
    }
    void kek() {
        int a = 0;
        a = 1;
        a = a;
    }
};

static_assert(sizeof(Victim) == sizeof(int));

using fatcom::IUnknownPtr;

int main(int argc, char *argv[])
{
    int i = 0;
    constexpr auto ITestName = ITest_VTable_Describe::name;
    static_assert(ITestName == "ITest");
    ITest_VTable_Describe::for_each([&](auto info){
        i += info.name.size();
    });
    IUnknownPtr unk1 = IUnknownPtr::FromImplementor(new Victim);
    ITestPtr ptr = ITestPtr(unk1);
    ptr.method1(1, 2);
    ITest2Ptr ptr2(ptr);
    ptr2.method2(3);
    ITest3Ptr ptr3(ptr);
    ptr3.method3(false);

    IUnknownPtr unk(ptr);
    ITestPtr ptrBack(unk);
    ITest4Ptr ptr4(ptr);
    assert(!ptr4);
    return 0;
}
