# Earth texture source and regeneration

This face uses NASA's historical **Blue Marble2002** composite of land surface, ocean color, sea ice and clouds. The clouds are a fixed historical image, not live weather.

Source: [NASA Earth Observatory's Blue Marble article](https://science.nasa.gov/earth/earth-observatory/the-blue-marble-true-color-global-imagery-at-1km-resolution/) and its [2048×1024 cloud composite](https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57735/land_ocean_ice_cloud_2048.jpg). NASA states the images are freely available to educators, scientists, museums and the public.

Credit: NASA Goddard Space Flight Center; image by Reto Stöckli, with enhancements by Robert Simmon. See the NASA article for the underlying MODIS, USGS, NOAA and other data credits.

From the repository root, with the Python build environment active, run `python codex/tools/generate-earth-texture.py`. Add `--check` to verify the checked-in texture without rewriting the firmware header or provenance. An existing source image can be supplied with `--source <image.jpg>`.

The host-only generator downloads the source if absent and requires its pinned SHA-256, verifies 2048×1024 dimensions, projects the texture orthographically with a camera centered at longitude −100° / latitude 20°, supersamples at 2×, and emits a 206×206 indexed image with a 256-color RGB565 palette. No lighting is baked into the projection. Generated images and the downloaded source are local caches excluded from Git.

`source.json` records the original and generated hashes, Pillow version, projection axes and encoding. Its `generator_sha256` preserves the original generator's provenance; `portable_generator` identifies the publication helper with its repository-relative paths and check mode. The generated header comments retain their historical relative paths so regeneration reproduces the existing bytes exactly.

`src/displayapp/screens/EarthTexture.h` contains 42,948 bytes of flash data: 42,436 indexed pixels and 512 palette bytes. Original imagery and host previews stay outside the firmware source tree. The runtime renderer supplies lighting from the date and UTC-derived sun vector and never allocates a full decoded globe.
