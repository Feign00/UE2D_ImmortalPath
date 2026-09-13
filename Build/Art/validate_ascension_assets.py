"""Read-only UE check for the 41Q original ascension UI atlas."""
import unreal

path = "/Game/GAME/Asset/DesktopPixelV2/UI/T_AscensionAtlas"
texture = unreal.load_asset(path)
if not isinstance(texture, unreal.Texture2D):
    raise RuntimeError("Missing Texture2D: " + path)
size = (texture.blueprint_get_size_x(), texture.blueprint_get_size_y())
if size != (1536, 1024):
    raise RuntimeError(f"Wrong atlas dimensions: {size}")
if not texture.get_editor_property("never_stream"):
    raise RuntimeError("Atlas must remain resident")
if texture.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_EDITOR_ICON:
    raise RuntimeError("Unexpected UI compression")
unreal.log(f"Ascension atlas verified: size={size} never_stream=true compression=TC_EDITOR_ICON")
