return {
    materials = {
        {
            name = "helmet",
            base_color = vec3.new(0.72, 0.24, 0.08),
            metallic = 0.85,
            roughness = 0.28,
        },
        {
            name = "ground",
            base_color = vec3.new(0.18, 0.20, 0.22),
            metallic = 0.0,
            roughness = 0.92,
        },
    },
    entities = {
        {
            name = "hero",
            mesh = "SciFiHelmet.glb",
            material = "helmet",
            position = vec3.new(0.0, 0.6, 0.0),
        },
        {
            name = "floor",
            mesh = "plane",
            material = "ground",
            position = vec3.new(0.0, 0.0, 0.0),
        },
    },
}
