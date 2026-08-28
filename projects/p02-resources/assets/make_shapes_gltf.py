"""生成 P2-t07 用的 glTF 资产。

这个资产的每一处「不规整」都是**故意**的。t07 的任务书列了一串加载器常见坑，
如果资产本身全用最规整的形式（紧凑排列、u16 索引、只有 translation 的 node），
那些坑一条也触发不到 —— 学生把四元数分量序写反、把 byteStride 完全忽略，
测试照样全绿，参考实现里那些分支也从没被执行过。

所以三个 mesh 各自覆盖一组分支：

    mesh          顶点布局               索引类型    节点变换
    ------------  ---------------------  ----------  ----------------------
    Pyramid       交错, byteStride=32    u8  (5121)  matrix（4x4 列主序）
    Prism         每属性一个 bufferView  u32 (5125)  TRS，**带 rotation**
    Octahedron    每属性一个 bufferView  u16 (5123)  TRS，只有 translation+scale

加上「三个 mesh 合并进同一对 vertex/index buffer」这个本来就有的教学点，
t07 任务书里除「层级递归」（明确标为扩展练习）之外的每一条都有资产覆盖。

顶点数据用 base64 data URI 内嵌，整个资产就是一个文本文件 ——
不依赖外部 .bin，也不会有二进制文件进 git 历史。
"""
import base64
import json
import math
import pathlib
import struct

# --- 几何 -------------------------------------------------------------------


def pyramid():
    """正四棱锥：4 个侧面 + 底面。每个面顶点独立，好让颜色不跨面插值。"""
    apex = (0.0, 0.6, 0.0)
    base = [(-0.5, -0.2, -0.5), (0.5, -0.2, -0.5), (0.5, -0.2, 0.5), (-0.5, -0.2, 0.5)]
    face_colors = [(0.95, 0.4, 0.3), (0.35, 0.85, 0.5), (0.4, 0.55, 0.95), (0.95, 0.85, 0.35)]

    P, U, C, I = [], [], [], []
    for f in range(4):
        a, b = base[f], base[(f + 1) % 4]
        for v, uv in ((a, (0.0, 0.0)), (b, (1.0, 0.0)), (apex, (0.5, 1.0))):
            P.append(v); U.append(uv); C.append(face_colors[f])
        n = len(P)
        I += [n - 3, n - 2, n - 1]
    # 底面（两个三角形）
    n0 = len(P)
    for v, uv in zip(base, ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))):
        P.append(v); U.append(uv); C.append((0.6, 0.6, 0.7))
    I += [n0 + 0, n0 + 2, n0 + 1, n0 + 0, n0 + 3, n0 + 2]
    return P, U, C, I


def prism(sides=6):
    """正六棱柱。它的 node 带 rotation —— 侧面颜色各不相同，转歪了一眼就看得出。"""
    r, h = 0.32, 0.45
    P, U, C, I = [], [], [], []
    for s in range(sides):
        a0 = 2 * math.pi * s / sides
        a1 = 2 * math.pi * (s + 1) / sides
        p0 = (r * math.cos(a0), -h, r * math.sin(a0))
        p1 = (r * math.cos(a1), -h, r * math.sin(a1))
        p2 = (r * math.cos(a1), h, r * math.sin(a1))
        p3 = (r * math.cos(a0), h, r * math.sin(a0))
        t = s / sides
        shade = 0.45 + 0.5 * (0.5 + 0.5 * math.cos(a0))
        col = (shade, shade * 0.8, 1.0 - shade * 0.4)
        n = len(P)
        for v, uv in ((p0, (t, 0.0)), (p1, (t, 0.0)), (p2, (t, 1.0)), (p3, (t, 1.0))):
            P.append(v); U.append(uv); C.append(col)
        I += [n + 0, n + 1, n + 2, n + 2, n + 3, n + 0]
    return P, U, C, I


def octahedron():
    """正八面体。顶点最少的那个，用最规整的形式（紧凑 + u16 + 纯 TRS）作为对照组。"""
    top, bottom = (0.0, 0.42, 0.0), (0.0, -0.42, 0.0)
    ring = [(0.34, 0.0, 0.0), (0.0, 0.0, 0.34), (-0.34, 0.0, 0.0), (0.0, 0.0, -0.34)]
    warm = [(0.95, 0.62, 0.28), (0.88, 0.45, 0.42), (0.75, 0.72, 0.30), (0.92, 0.55, 0.22)]
    cool = [(0.35, 0.62, 0.78), (0.30, 0.70, 0.62), (0.42, 0.52, 0.82), (0.28, 0.66, 0.72)]

    P, U, C, I = [], [], [], []
    for i in range(4):
        a, b = ring[i], ring[(i + 1) % 4]
        for apex, palette in ((top, warm), (bottom, cool)):
            # 下半绕反，好让两半的正面朝向一致
            tri = (a, b, apex) if apex is top else (b, a, apex)
            for v, uv in zip(tri, ((0.0, 0.0), (1.0, 0.0), (0.5, 1.0))):
                P.append(v); U.append(uv); C.append(palette[i])
            n = len(P)
            I += [n - 3, n - 2, n - 1]
    return P, U, C, I


# --- 打包 -------------------------------------------------------------------

FLOAT, U8, U16, U32 = 5126, 5121, 5123, 5125
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963
INDEX_FORMAT = {U8: "<B", U16: "<H", U32: "<I"}

blob = bytearray()
buffer_views, accessors, gltf_meshes, nodes = [], [], [], []


def add_view(data, target, byte_stride=None):
    """把一段字节追加进 buffer 并登记成 bufferView。

    起点和终点都补到 4 字节：glTF 要求 bufferView 的 byteOffset 按组件大小对齐，
    补到 4 一律满足。u8 索引段尤其需要 —— 它的长度常是奇数，不补的话后面那个
    float 段会落在没对齐的地址上。
    """
    while len(blob) % 4:
        blob.append(0)
    view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data), "target": target}
    if byte_stride is not None:
        view["byteStride"] = byte_stride
    blob.extend(data)
    while len(blob) % 4:
        blob.append(0)
    buffer_views.append(view)
    return len(buffer_views) - 1


def add_accessor(view, component_type, count, type_, byte_offset=0, positions=None):
    acc = {"bufferView": view, "componentType": component_type, "count": count, "type": type_}
    if byte_offset:
        acc["byteOffset"] = byte_offset
    if positions is not None:
        xs = [p[0] for p in positions]
        ys = [p[1] for p in positions]
        zs = [p[2] for p in positions]
        acc["min"] = [min(xs), min(ys), min(zs)]
        acc["max"] = [max(xs), max(ys), max(zs)]
    accessors.append(acc)
    return len(accessors) - 1


def quaternion(axis, degrees):
    """轴角转四元数，按 **glTF 的 (x, y, z, w) 顺序** 返回。

    注意这个顺序：glm::quat 的构造函数是 (w, x, y, z)。加载器把它们按下标
    0/1/2/3 直接喂进去不会报错也不会崩 —— 分量置换保持模长为 1，得到的仍是
    一个合法的单位四元数，只是转到了别的角度。
    """
    ax, ay, az = axis
    n = math.sqrt(ax * ax + ay * ay + az * az)
    ax, ay, az = ax / n, ay / n, az / n
    half = math.radians(degrees) / 2.0
    s = math.sin(half)
    return [ax * s, ay * s, az * s, math.cos(half)]


def trs_matrix(translation, scale):
    """列主序的 4x4。glTF 和 glm 都是列主序，所以加载器不需要转置。"""
    tx, ty, tz = translation
    sx, sy, sz = scale
    return [sx, 0.0, 0.0, 0.0,
            0.0, sy, 0.0, 0.0,
            0.0, 0.0, sz, 0.0,
            tx, ty, tz, 1.0]


def add_mesh(name, geometry, *, interleaved, index_type):
    P, U, C, I = geometry
    if interleaved:
        # POSITION(vec3) + TEXCOORD_0(vec2) + COLOR_0(vec3) = 8 个 float = 32 字节
        packed = b"".join(struct.pack("<3f2f3f", *p, *u, *c) for p, u, c in zip(P, U, C))
        view = add_view(packed, ARRAY_BUFFER, byte_stride=32)
        pos = add_accessor(view, FLOAT, len(P), "VEC3", 0, positions=P)
        uv = add_accessor(view, FLOAT, len(U), "VEC2", 12)
        col = add_accessor(view, FLOAT, len(C), "VEC3", 20)
    else:
        pos = add_accessor(add_view(b"".join(struct.pack("<3f", *p) for p in P),
                                    ARRAY_BUFFER), FLOAT, len(P), "VEC3", positions=P)
        uv = add_accessor(add_view(b"".join(struct.pack("<2f", *t) for t in U),
                                   ARRAY_BUFFER), FLOAT, len(U), "VEC2")
        col = add_accessor(add_view(b"".join(struct.pack("<3f", *c) for c in C),
                                    ARRAY_BUFFER), FLOAT, len(C), "VEC3")

    packed_indices = b"".join(struct.pack(INDEX_FORMAT[index_type], i) for i in I)
    idx = add_accessor(add_view(packed_indices, ELEMENT_ARRAY_BUFFER),
                       index_type, len(I), "SCALAR")

    gltf_meshes.append({"name": name, "primitives": [{
        "attributes": {"POSITION": pos, "TEXCOORD_0": uv, "COLOR_0": col},
        "indices": idx, "mode": 4}]})
    return len(gltf_meshes) - 1


# --- 组装 -------------------------------------------------------------------

# 1) 交错顶点 + u8 索引 + matrix 形式的 node
mesh = add_mesh("Pyramid", pyramid(), interleaved=True, index_type=U8)
nodes.append({"name": "PyramidNode", "mesh": mesh,
              "matrix": trs_matrix((-1.12, 0.05, 0.05), (0.85, 0.85, 0.85))})

# 2) 紧凑顶点 + u32 索引 + 带 rotation 的 TRS
mesh = add_mesh("Prism", prism(), interleaved=False, index_type=U32)
nodes.append({"name": "PrismNode", "mesh": mesh,
              "translation": [1.52, 0.16, -0.35],
              "rotation": quaternion((0.25, 0.9, 0.35), 52.0),
              "scale": [0.85, 0.85, 0.85]})

# 3) 对照组：全部用最规整的形式
mesh = add_mesh("Octahedron", octahedron(), interleaved=False, index_type=U16)
nodes.append({"name": "OctahedronNode", "mesh": mesh,
              "translation": [0.88, 0.50, -1.70],
              "scale": [0.9, 0.9, 0.9]})

gltf = {
    "asset": {"version": "2.0", "generator": "render-workbook p02 asset generator"},
    "scene": 0,
    "scenes": [{"name": "Scene", "nodes": list(range(len(nodes)))}],
    "nodes": nodes,
    "meshes": gltf_meshes,
    "accessors": accessors,
    "bufferViews": buffer_views,
    "buffers": [{"byteLength": len(blob),
                 "uri": "data:application/octet-stream;base64," +
                        base64.b64encode(bytes(blob)).decode("ascii")}],
}

out = pathlib.Path(__file__).resolve().parents[0] / "shapes.gltf"
out.write_text(json.dumps(gltf, indent=1), encoding="utf-8", newline="\n")
print("wrote", out.name, out.stat().st_size, "bytes;",
      len(nodes), "nodes,", len(blob), "bytes of vertex data")
for m, n in zip(gltf_meshes, nodes):
    prim = m["primitives"][0]
    view = buffer_views[accessors[prim["attributes"]["POSITION"]]["bufferView"]]
    print("  {:<12} stride={:<7} index={:<5} node={}".format(
        m["name"], view.get("byteStride", "packed"),
        accessors[prim["indices"]]["componentType"],
        "matrix" if "matrix" in n else ("TRS+rot" if "rotation" in n else "TRS")))
