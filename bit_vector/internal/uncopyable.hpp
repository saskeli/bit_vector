#ifndef UNCOPYABLE_HPP
#define UNCOPYABLE_HPP

namespace bv {
class uncopyable {
   public:
    uncopyable() {}
    uncopyable(const uncopyable&) = delete;
    uncopyable& operator=(const uncopyable&) = delete;
};
}

#endif