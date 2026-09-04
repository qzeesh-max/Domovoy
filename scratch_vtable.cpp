#include <iostream>
#include <dlfcn.h>
#include <cxxabi.h>
#include <string>

class MyClass {
public:
    virtual ~MyClass() {}
    virtual void Foo() {}
};

int main() {
    MyClass* obj = new MyClass();
    void* vptr = *(void**)obj;
    Dl_info info;
    if (dladdr(vptr, &info)) {
        std::cout << "vptr: " << vptr << std::endl;
        std::cout << "dli_fname: " << (info.dli_fname ? info.dli_fname : "null") << std::endl;
        std::cout << "dli_sname: " << (info.dli_sname ? info.dli_sname : "null") << std::endl;
        if (info.dli_sname) {
            int status = 0;
            char* demangled = abi::__cxa_demangle(info.dli_sname, 0, 0, &status);
            if (demangled) {
                std::cout << "Demangled: " << demangled << std::endl;
                free(demangled);
            }
        }
    }
    return 0;
}
