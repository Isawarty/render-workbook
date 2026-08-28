# P3-t04 — Bitonic sort

## 目标

把 compare-and-swap 网络映射到 GPU：每次 dispatch 执行网络中的一个 `(k,j)` 阶段，
host 负责按正确顺序发完整网络。输入长度不要求是 2 的幂；补齐和裁剪也是算法契约的一部分。
非目标：追求比 radix sort 更高的吞吐量。

## 要实现

- `src/steps/04_bitonic.cpp`：补齐、双层网络调度、阶段 barrier、回读裁剪。
- `shaders/bitonic.comp`：partner 索引、升降序判断与单写者 compare-and-swap。

## 验收标准

```bash
python rwb.py test p03-t04
```

空输入、1、257、1023 个元素都必须与 `std::sort` 完全一致；测试包含降序、重复值、
非二次幂和 `UINT_MAX` padding 路径；validation 零 error/warning。

## 调试入口

- final 命令同上，整数走 exact compare，失败列出首 8 个索引。
- 推荐断点：双层循环内观察 `(k,j)`；shader 调试看 `i ^ j` 的 partner。
- 只有成对元素颠倒通常是方向位；大段局部有序但全局无序通常是漏阶段或漏 barrier；
  尾部出现哨兵通常是 padding/crop 错。
- sync validation 不保证理解 shader descriptor hazard；完成后抓一次 GPU capture 核对阶段序列。

## 分层提示

<details><summary>Hint 1</summary>Bitonic 网络要求长度为 2 的幂；升序 padding 用最大值。</details>
<details><summary>Hint 2</summary>`k=2,4,...,N`；每个 k 内 `j=k/2,...,1`。</details>
<details><summary>Hint 3</summary>partner=`i^j`；只让 `partner>i` 的 invocation 写这一对。</details>
