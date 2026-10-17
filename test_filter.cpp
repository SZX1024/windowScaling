
#include <input.h>
#include <input_event.h>
#include <iostream>

using namespace KWin;

class TestFilter : public InputEventFilter
{
public:
    TestFilter() : InputEventFilter(InputFilterOrder::InteractiveMoveResize) {}
    ~TestFilter() override = default;

    bool pointerMotion(PointerMotionEvent *event) override {
        return false;
    }
};

int main() {
    std::cout << "TestFilter compiled!" << std::endl;
    return 0;
}
