// basis_wrappers.cpp - Simple C-style wrappers to the C++ transcoder for WebGL use.
#include "basisu_transcoder.h"
#include <emscripten/bind.h>
#include <algorithm>

using namespace emscripten;
using namespace basist;

static basist::etc1_global_selector_codebook *g_pGlobal_codebook;

void basis_init()
{
  basisu_transcoder_init();

  if (!g_pGlobal_codebook)
    g_pGlobal_codebook = new basist::etc1_global_selector_codebook(g_global_selector_cb_size, g_global_selector_cb);
}

#define MAGIC 0xDEADBEE1

struct basis_file
{
  int m_magic = 0;
	basisu_transcoder m_transcoder;
  std::vector<uint8_t> m_file;

  basis_file(const emscripten::val& jsBuffer)
    : m_file([&]() {
        size_t byteLength = jsBuffer["byteLength"].as<size_t>();
        return std::vector<uint8_t>(byteLength);
      }()),
      m_transcoder(g_pGlobal_codebook)
  {
    unsigned int length = jsBuffer["length"].as<unsigned int>();
    emscripten::val memory = emscripten::val::module_property("HEAP8")["buffer"];
    emscripten::val memoryView = jsBuffer["constructor"].new_(memory, reinterpret_cast<uintptr_t>(m_file.data()), length);
    memoryView.call<void>("set", jsBuffer);

    if (!m_transcoder.validate_header(m_file.data(), m_file.size())) {
      m_file.clear();
    }

    // Initialized after validation
    m_magic = MAGIC;
  }

  void close() {
    assert(m_magic == MAGIC);
    m_file.clear();
  }

  uint32_t getHasAlpha() {
    assert(m_magic == MAGIC);
    if (m_magic != MAGIC)
      return 0;

    basisu_image_level_info li;
    if (!m_transcoder.get_image_level_info(m_file.data(), m_file.size(), li, 0, 0))
      return 0;

    return li.m_alpha_flag;
  }

  uint32_t getNumImages() {
    assert(m_magic == MAGIC);
    if (m_magic != MAGIC)
      return 0;

    return m_transcoder.get_total_images(m_file.data(), m_file.size());
  }

  uint32_t getNumLevels(uint32_t image_index) {
    assert(m_magic == MAGIC);
    if (m_magic != MAGIC)
      return 0;

    basisu_image_info ii;
    if (!m_transcoder.get_image_info(m_file.data(), m_file.size(), ii, image_index))
      return 0;

    return ii.m_total_levels;
  }

  uint32_t getImageWidth(uint32_t image_index, uint32_t level_index) {
    assert(m_magic == MAGIC);
    if (m_magic != MAGIC)
      return 0;

    uint32_t orig_width, orig_height, total_blocks;
    if (!m_transcoder.get_image_level_desc(m_file.data(), m_file.size(), image_index, level_index, orig_width, orig_height, total_blocks))
      return 0;

    return orig_width;
  }

  uint32_t getImageHeight(uint32_t image_index, uint32_t level_index) {
    assert(m_magic == MAGIC);
    if (m_magic != MAGIC)
      return 0;

    uint32_t orig_width, orig_height, total_blocks;
    if (!m_transcoder.get_image_level_desc(m_file.data(), m_file.size(), image_index, level_index, orig_width, orig_height, total_blocks))
      return 0;

    return orig_height;
  }

  uint32_t getImageTranscodedSizeInBytes(uint32_t image_index, uint32_t level_index, uint32_t format) {
    assert(m_magic == MAGIC);
    if (m_magic != MAGIC)
      return 0;

    if (format >= (int)transcoder_texture_format::cTFTotalTextureFormats)
      return 0;

	 uint32_t orig_width, orig_height, total_blocks;
	 if (!m_transcoder.get_image_level_desc(m_file.data(), m_file.size(), image_index, level_index, orig_width, orig_height, total_blocks))
		 return 0;

	 const transcoder_texture_format transcoder_format = static_cast<transcoder_texture_format>(format);

	 if (basis_transcoder_format_is_uncompressed(transcoder_format))
	 {
		 // Uncompressed formats are just plain raster images.
		 const uint32_t bytes_per_pixel = basis_get_uncompressed_bytes_per_pixel(transcoder_format);
		 const uint32_t bytes_per_line = orig_width * bytes_per_pixel;
		 const uint32_t bytes_per_slice = bytes_per_line * orig_height;
		 return bytes_per_slice;
	 }
	 else
	 {
		 // Compressed formats are 2D arrays of blocks.
		 const uint32_t bytes_per_block = basis_get_bytes_per_block(transcoder_format);

		 if (transcoder_format == transcoder_texture_format::cTFPVRTC1_4_RGB || transcoder_format == transcoder_texture_format::cTFPVRTC1_4_RGBA)
		 {
			 // For PVRTC1, Basis only writes (or requires) total_blocks * bytes_per_block. But GL requires extra padding for very small textures: 
			  // https://www.khronos.org/registry/OpenGL/extensions/IMG/IMG_texture_compression_pvrtc.txt
			 const uint32_t width = (orig_width + 3) & ~3;
			 const uint32_t height = (orig_height + 3) & ~3;
			 const uint32_t size_in_bytes = (std::max(8U, width) * std::max(8U, height) * 4 + 7) / 8;
			 return size_in_bytes;
		 }

		 return total_blocks * bytes_per_block;
	 }
  }

  uint32_t startTranscoding() {
    assert(m_magic == MAGIC);
    if (m_magic != MAGIC)
      return 0;

    return m_transcoder.start_transcoding(m_file.data(), m_file.size());
  }

  uint32_t transcodeImage(const emscripten::val& dst, uint32_t image_index, uint32_t level_index, uint32_t format, uint32_t unused, uint32_t get_alpha_for_opaque_formats) {
     (void)unused;
     
	  assert(m_magic == MAGIC);
	  if (m_magic != MAGIC)
		  return 0;

	  if (format >= (int)transcoder_texture_format::cTFTotalTextureFormats)
		  return 0;

	  const transcoder_texture_format transcoder_format = static_cast<transcoder_texture_format>(format);

     uint32_t orig_width, orig_height, total_blocks;
	  if (!m_transcoder.get_image_level_desc(m_file.data(), m_file.size(), image_index, level_index, orig_width, orig_height, total_blocks))
		  return 0;
	  
	  std::vector<uint8_t> dst_data;
	  
	  uint32_t flags = get_alpha_for_opaque_formats ? basisu_transcoder::cDecodeFlagsTranscodeAlphaDataToOpaqueFormats : 0;

	  uint32_t status;

	  if (basis_transcoder_format_is_uncompressed(transcoder_format))
	  {
		  const uint32_t bytes_per_pixel = basis_get_uncompressed_bytes_per_pixel(transcoder_format);
		  const uint32_t bytes_per_line = orig_width * bytes_per_pixel;
		  const uint32_t bytes_per_slice = bytes_per_line * orig_height;

		  dst_data.resize(bytes_per_slice);

		  status = m_transcoder.transcode_image_level(
			  m_file.data(), m_file.size(), image_index, level_index,
			  dst_data.data(), orig_width * orig_height,
			  transcoder_format,
			  flags,
			  orig_width,
			  nullptr,
			  orig_height);
	  }
	  else
	  {
		  uint32_t bytes_per_block = basis_get_bytes_per_block(transcoder_format);

		  uint32_t required_size = total_blocks * bytes_per_block;

		  if (transcoder_format == transcoder_texture_format::cTFPVRTC1_4_RGB || transcoder_format == transcoder_texture_format::cTFPVRTC1_4_RGBA)
		  {
			  // For PVRTC1, Basis only writes (or requires) total_blocks * bytes_per_block. But GL requires extra padding for very small textures: 
			  // https://www.khronos.org/registry/OpenGL/extensions/IMG/IMG_texture_compression_pvrtc.txt
			  // The transcoder will clear the extra bytes followed the used blocks to 0.
			  const uint32_t width = (orig_width + 3) & ~3;
			  const uint32_t height = (orig_height + 3) & ~3;
			  required_size = (std::max(8U, width) * std::max(8U, height) * 4 + 7) / 8;
			  assert(required_size >= total_blocks * bytes_per_block);
		  }

		  dst_data.resize(required_size);

		  status = m_transcoder.transcode_image_level(
			  m_file.data(), m_file.size(), image_index, level_index,
			  dst_data.data(), dst_data.size() / bytes_per_block,
			  static_cast<basist::transcoder_texture_format>(format),
			  flags);
	  }

	  emscripten::val memory = emscripten::val::module_property("HEAP8")["buffer"];
	  emscripten::val memoryView = emscripten::val::global("Uint8Array").new_(memory, reinterpret_cast<uintptr_t>(dst_data.data()), dst_data.size());

	  dst.call<void>("set", memoryView);
	  return status;
  }
};

EMSCRIPTEN_BINDINGS(basis_transcoder) {

  function("initializeBasis", &basis_init);

  class_<basis_file>("BasisFile")
    .constructor<const emscripten::val&>()
    .function("close", optional_override([](basis_file& self) {
      return self.close();
    }))
    .function("getHasAlpha", optional_override([](basis_file& self) {
      return self.getHasAlpha();
    }))
    .function("getNumImages", optional_override([](basis_file& self) {
      return self.getNumImages();
    }))
    .function("getNumLevels", optional_override([](basis_file& self, uint32_t imageIndex) {
      return self.getNumLevels(imageIndex);
    }))
    .function("getImageWidth", optional_override([](basis_file& self, uint32_t imageIndex, uint32_t levelIndex) {
      return self.getImageWidth(imageIndex, levelIndex);
    }))
    .function("getImageHeight", optional_override([](basis_file& self, uint32_t imageIndex, uint32_t levelIndex) {
      return self.getImageHeight(imageIndex, levelIndex);
    }))
    .function("getImageTranscodedSizeInBytes", optional_override([](basis_file& self, uint32_t imageIndex, uint32_t levelIndex, uint32_t format) {
      return self.getImageTranscodedSizeInBytes(imageIndex, levelIndex, format);
    }))
    .function("startTranscoding", optional_override([](basis_file& self) {
      return self.startTranscoding();
    }))
    .function("transcodeImage", optional_override([](basis_file& self, const emscripten::val& dst, uint32_t imageIndex, uint32_t levelIndex, uint32_t format, uint32_t unused, uint32_t getAlphaForOpaqueFormats) {
      return self.transcodeImage(dst, imageIndex, levelIndex, format, unused, getAlphaForOpaqueFormats);
    }))
  ;

}
