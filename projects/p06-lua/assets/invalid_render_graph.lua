return {
    resources = { { name = "HDR", kind = "image" } },
    passes = { { name = "Broken", reads = { { resource = "Missing", state = "shader-read" } } } },
}
