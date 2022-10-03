/**
 * Transcodes DXT into RGB565.
 * This is an optimized version of dxtToRgb565Unoptimized() below.
 * Optimizations:
 * 1. Use integer math to compute c2 and c3 instead of floating point
 *    math.  Specifically:
 *      c2 = 5/8 * c0 + 3/8 * c1
 *      c3 = 3/8 * c0 + 5/8 * c1
 *    This is about a 40% performance improvement.  It also appears to
 *    match what hardware DXT decoders do, as the colors produced
 *    by this integer math match what hardware produces, while the
 *    floating point in dxtToRgb565Unoptimized() produce slightly
 *    different colors (for one GPU this was tested on).
 * 2. Unroll the inner loop.  Another ~10% improvement.
 * 3. Compute r0, g0, b0, r1, g1, b1 only once instead of twice.
 *    Another 10% improvement.
 * 4. Use a Uint16Array instead of a Uint8Array.  Another 10% improvement.
 * @param {Uint16Array} src The src DXT bits as a Uint16Array.
 * @param {number} srcByteOffset
 * @param {number} width
 * @param {number} height
 * @return {Uint16Array} dst
 */
function dxtToRgb565(src, src16Offset, width, height) {
  var c = new Uint16Array(4);
  var dst = new Uint16Array(width * height);
  var nWords = (width * height) / 4;
  var m = 0;
  var dstI = 0;
  var i = 0;
  var r0 = 0, g0 = 0, b0 = 0, r1 = 0, g1 = 0, b1 = 0;

  var blockWidth = width / 4;
  var blockHeight = height / 4;
  for (var blockY = 0; blockY < blockHeight; blockY++) {
    for (var blockX = 0; blockX < blockWidth; blockX++) {
      i = src16Offset + 4 * (blockY * blockWidth + blockX);
      c[0] = src[i];
      c[1] = src[i + 1];
	  
      r0 = c[0] & 0x1f;
      g0 = c[0] & 0x7e0;
      b0 = c[0] & 0xf800;
      r1 = c[1] & 0x1f;
      g1 = c[1] & 0x7e0;
      b1 = c[1] & 0xf800;
      // Interpolate between c0 and c1 to get c2 and c3.
      // Note that we approximate 1/3 as 3/8 and 2/3 as 5/8 for
      // speed.  This also appears to be what the hardware DXT
      // decoder in many GPUs does :)

	  // rg FIXME: This is most likely leading to wrong results vs. a GPU
	  
      c[2] = ((5 * r0 + 3 * r1) >> 3)
             | (((5 * g0 + 3 * g1) >> 3) & 0x7e0)
             | (((5 * b0 + 3 * b1) >> 3) & 0xf800);
      c[3] = ((5 * r1 + 3 * r0) >> 3)
             | (((5 * g1 + 3 * g0) >> 3) & 0x7e0)
             | (((5 * b1 + 3 * b0) >> 3) & 0xf800);
      m = src[i + 2];
      dstI = (blockY * 4) * width + blockX * 4;
      dst[dstI] = c[m & 0x3];
      dst[dstI + 1] = c[(m >> 2) & 0x3];
      dst[dstI + 2] = c[(m >> 4) & 0x3];
      dst[dstI + 3] = c[(m >> 6) & 0x3];
      dstI += width;
      dst[dstI] = c[(m >> 8) & 0x3];
      dst[dstI + 1] = c[(m >> 10) & 0x3];
      dst[dstI + 2] = c[(m >> 12) & 0x3];
      dst[dstI + 3] = c[(m >> 14)];
      m = src[i + 3];
      dstI += width;
      dst[dstI] = c[m & 0x3];
      dst[dstI + 1] = c[(m >> 2) & 0x3];
      dst[dstI + 2] = c[(m >> 4) & 0x3];
      dst[dstI + 3] = c[(m >> 6) & 0x3];
      dstI += width;
      dst[dstI] = c[(m >> 8) & 0x3];
      dst[dstI + 1] = c[(m >> 10) & 0x3];
      dst[dstI + 2] = c[(m >> 12) & 0x3];
      dst[dstI + 3] = c[(m >> 14)];
    }
  }
  return dst;
}


/**
 * An unoptimized version of dxtToRgb565.  Also, the floating
 * point math used to compute the colors actually results in
 * slightly different colors compared to hardware DXT decoders.
 * @param {Uint8Array} src
 * @param {number} srcByteOffset
 * @param {number} width
 * @param {number} height
 * @return {Uint16Array} dst
 */
function dxtToRgb565Unoptimized(src, srcByteOffset, width, height) {
  var c = new Uint16Array(4);
  var dst = new Uint16Array(width * height);
  var nWords = (width * height) / 4;

  var blockWidth = width / 4;
  var blockHeight = height / 4;
  for (var blockY = 0; blockY < blockHeight; blockY++) {
    for (var blockX = 0; blockX < blockWidth; blockX++) {
      var i = srcByteOffset + 8 * (blockY * blockWidth + blockX);
      c[0] = src[i] | (src[i + 1] << 8);
      c[1] = src[i + 2] | (src[i + 3] << 8);
      c[2] = (2 * (c[0] & 0x1f) + 1 * (c[1] & 0x1f)) / 3
             | (((2 * (c[0] & 0x7e0) + 1 * (c[1] & 0x7e0)) / 3) & 0x7e0)
             | (((2 * (c[0] & 0xf800) + 1 * (c[1] & 0xf800)) / 3) & 0xf800);
      c[3] = (2 * (c[1] & 0x1f) + 1 * (c[0] & 0x1f)) / 3
             | (((2 * (c[1] & 0x7e0) + 1 * (c[0] & 0x7e0)) / 3) & 0x7e0)
             | (((2 * (c[1] & 0xf800) + 1 * (c[0] & 0xf800)) / 3) & 0xf800);
      for (var row = 0; row < 4; row++) {
        var m = src[i + 4 + row];
        var dstI = (blockY * 4 + row) * width + blockX * 4;
        dst[dstI++] = c[m & 0x3];
        dst[dstI++] = c[(m >> 2) & 0x3];
        dst[dstI++] = c[(m >> 4) & 0x3];
        dst[dstI++] = c[(m >> 6) & 0x3];
      }
    }
  }
  return dst;
}

