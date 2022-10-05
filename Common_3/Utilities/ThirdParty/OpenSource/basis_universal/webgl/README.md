# WebGL Examples

Requires WebAssembly and WebGL support.

## Texture

[Live demo: `texture/index.html`](https://basis-universal-webgl.now.sh/texture/)

(Note the Live texture demo hasn't been updated to the latest release yet.)

Renders a single texture, using the transcoder (compiled to WASM with emscripten) to generate one of the following compressed texture formats:

* ASTC
* BC1 (no alpha)
* BC3
* ETC1 (no alpha)
* PVRTC

On browsers that don't support any compressed texture format, there's a low-quality fallback code path for opaque textures. Note that the fallback path only converts to 16-bit RGB images at the moment, so the quality isn't as good as it should be.

![Screenshot showing a basis texture rendered as a 2D image in a webpage.](texture/preview.png)

## glTF 3D Model

[Live demo: `gltf/index.html`](https://basis-universal-webgl.now.sh/gltf/)

Renders a glTF 3D model with `.basis` texture files, transcoded into one of the following compressed texture formats:

* ASTC
  * Tested in Chrome on Android, Pixel 3 XL.
* DTX (BC1/BC3)
  * Tested in Chrome (Linux and macOS) and Firefox (macOS).
* ETC1
  * Tested in Chrome on Android, Pixel 3 XL.
* PVRTC
  * Tested in Chrome and Safari on iOS iPhone 6 Plus.

The glTF model in this demo uses a hypothetical `GOOGLE_texture_basis` extension. That extension is defined for the sake of example only – the glTF format will officially embed Basis files within a KTX2 wrapper, through a new
extension that is [currently in development](https://github.com/KhronosGroup/glTF/pull/1612).

![Screenshot showing a basis texture rendered as the base color texture for a 3D model in a webpage.](gltf/preview.png)

## Testing locally

See [how to run things locally](https://threejs.org/docs/#manual/en/introduction/How-to-run-things-locally), or (with [Node.js](https://nodejs.org/en/) installed), run:

```
npx serve
```

The console will display a `localhost` URL for local testing, and (on supported WiFi networks and devices) may also display an IP address accessible by other devices on the same network. Note that mobile devices must support WebAssembly to run this demo. Learn more about [remote debugging your android devices](https://developers.google.com/web/tools/chrome-devtools/remote-debugging/).
