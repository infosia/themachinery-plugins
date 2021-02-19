# themachinery-plugins
Custom plugins for The Machinery

## tm_ig_glb: glTF binary (.glb) importer

glTF binary (.glb) asset importer that is based on [cgltf](https://github.com/jkuhlmann/cgltf). glTF animation is not supported (just because I don't use it). May be more lightweight than `tm_assimp`. Note that you need to disable `tm_assimp` in order to use this because `tm_assimp` supports `.glb` too. I'm usually doing this by renaming `tm_assimp.dll` to `tm_assimp.dll_`.

## tm_ig_vrm: VRM importer

Custom asset importer that enables [VRM](https://vrm.dev/en/) on The Machinery.

## tm_ig_animation_puppet

Custom component that just follows entity animation that has a tag `tm_ig_animation_puppet_root`.

## Building

Running `tmbuild` in the plugin directory should just work. Note that it installs the plugin to `"$(TM_SDK_DIR)/bin/plugins`.

## License

Available to anybody free of charge, under the terms of MIT License (see LICENSE.md).