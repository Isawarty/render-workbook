#pragma once
#include <stdexcept>
#include <string>

namespace rwb {

// 挖空题专用异常。
//
// 骨架里每个待实现的函数都以 RWB_TODO(...) 结尾，效果是：
//   * 代码能编译通过（你才领得到题）
//   * 运行时立刻抛出、被 main/测试捕获，打印出还差哪一步
//   * 测试因此「失败得可读」，而不是段错误或 validation 刷屏
//
// 这条不变量很重要：任何 start/* tag 都必须能编译。
// 编译不过 = 你连题都领不到。
class NotImplemented : public std::runtime_error {
public:
    explicit NotImplemented(const std::string& what)
        : std::runtime_error("尚未实现: " + what) {}
};

[[noreturn]] inline void todo(const std::string& what) {
    throw NotImplemented(what);
}

} // namespace rwb

#define RWB_TODO(what) ::rwb::todo(what)
