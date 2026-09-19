// Host mock entry point: drives one firmware setup() pass and a small fixed
// number of update() iterations, then exits. Used only by the mock build.
#include <cstdio>

extern void setup();
extern void loop();

int main() {
    setup();
    for (int i = 0; i < 3; ++i) {
        loop();
    }
    std::printf("firmware mock: setup + 3 loop iterations OK\n");
    return 0;
}
