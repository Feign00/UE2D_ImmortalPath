"""Read-only UE Python check for the imported 41O crafting atlas packages."""
import unreal

for name, expected in (("T_EquipmentAtlas", (1254, 1254)), ("T_ForgeAtlas", (1536, 1024))):
    path = "/Game/GAME/Asset/DesktopPixelV2/UI/" + name
    texture = unreal.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Missing Texture2D: " + path)
    actual = (texture.blueprint_get_size_x(), texture.blueprint_get_size_y())
    if actual != expected:
        raise RuntimeError(f"Wrong atlas dimensions: {name} {actual}")
    if not texture.get_editor_property("never_stream"):
        raise RuntimeError("Atlas must remain resident: " + name)
    if texture.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_EDITOR_ICON:
        raise RuntimeError("Unexpected UI texture compression: " + name)
    unreal.log(f"Crafting atlas verified: {name} size={actual} never_stream=true compression=TC_EDITOR_ICON")
