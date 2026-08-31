return {
    resources = {
        { name = "GBuffer", kind = "image", size = 4096, compatibility = 1 },
        { name = "HDR", kind = "image", size = 4096, compatibility = 1 },
        { name = "Swapchain", kind = "image", imported = true },
    },
    passes = {
        { name = "Geometry", writes = { { resource = "GBuffer", state = "color-write" } } },
        {
            name = "Lighting",
            reads = { { resource = "GBuffer", state = "shader-read" } },
            writes = { { resource = "HDR", state = "color-write" } },
        },
        {
            name = "Tonemap",
            reads = { { resource = "HDR", state = "shader-read" } },
            writes = { { resource = "Swapchain", state = "present" } },
        },
    },
}
