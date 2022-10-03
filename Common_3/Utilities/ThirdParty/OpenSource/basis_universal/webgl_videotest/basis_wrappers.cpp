// basis_wrappers.cpp - Simple C-style wrappers to the C++ transcoder for WebGL use. 
#include "basisu_transcoder.h"

using namespace basist;

static basist::etc1_global_selector_codebook *g_pGlobal_codebook;

typedef unsigned int uint;

extern "C" 
{
  void basis_init();
  
  void *basis_open(void *src, uint src_size);
  void basis_close(void *h);

  uint basis_get_has_alpha(void *h);
	
  uint basis_get_num_images(void *h);
  uint basis_get_num_levels(void *h, uint image_index);
  
  uint basis_get_image_width(void *h, uint image_index, uint level_index);
  uint basis_get_image_height(void *h, uint image_index, uint level_index);
         
  uint basis_get_image_transcoded_size_in_bytes(void *h, uint image_index, uint level_index, uint format);
  
  uint basis_start_transcoding(void *h);
  
  uint basis_transcode_image(void *h, void *dst, uint dst_size_in_bytes, 
  uint image_index, uint level_index, uint format, 
  uint unused, uint get_alpha_for_opaque_formats);
  
  void basis_set_debug_flags(uint f) { basist::set_debug_flags(f); }
  uint basis_get_debug_flags() { return basist::get_debug_flags(); }
}

void basis_init()
{
	basisu_transcoder_init();
	
	if (!g_pGlobal_codebook)
		g_pGlobal_codebook = new basist::etc1_global_selector_codebook(g_global_selector_cb_size, g_global_selector_cb);
}

#define MAGIC 0xDEADBEE1

struct basis_file
{
	int m_magic;
	basisu_transcoder m_transcoder;
	void *m_pFile;
	uint m_file_size;

	basis_file() : 
		m_transcoder(g_pGlobal_codebook)
	{
	}
};

void *basis_open(void *src, uint src_size)
{
	if ((!src) || (!src_size))
		return nullptr;
		
	basis_file *f = new basis_file;
	f->m_pFile = src;
	f->m_file_size = src_size;
	
	if (!f->m_transcoder.validate_header(f->m_pFile, f->m_file_size))
	{
		delete f;
		return nullptr;
	}
	f->m_magic = MAGIC;
	
	return f;
}

void basis_close(void *h)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return;
	
	assert(f->m_magic == MAGIC);
	if (f->m_magic != MAGIC)
		return;
	
	delete f;
}

uint basis_get_has_alpha(void *h)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;

	assert(f->m_magic == MAGIC);	
	if (f->m_magic != MAGIC)
		return 0;
	
	basisu_image_level_info li;
	
	if (!f->m_transcoder.get_image_level_info(f->m_pFile, f->m_file_size, li, 0, 0))
		return 0;
		
	return li.m_alpha_flag;
}

uint basis_get_num_images(void *h)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;
	
	assert(f->m_magic == MAGIC);
	if (f->m_magic != MAGIC)
		return 0;
	
	return f->m_transcoder.get_total_images(f->m_pFile, f->m_file_size);
}

uint basis_get_num_levels(void *h, uint image_index)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;
	
	assert(f->m_magic == MAGIC);
	if (f->m_magic != MAGIC)
		return 0;
	
	basisu_image_info ii;
	
	if (!f->m_transcoder.get_image_info(f->m_pFile, f->m_file_size, ii, image_index))
		return 0;
		
	return ii.m_total_levels;
}

uint basis_get_image_width(void *h, uint image_index, uint level_index)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;
	
	assert(f->m_magic == MAGIC);
	if (f->m_magic != MAGIC)
		return 0;

	uint orig_width, orig_height, total_blocks;	
	if (!f->m_transcoder.get_image_level_desc(f->m_pFile, f->m_file_size, image_index, level_index, orig_width, orig_height, total_blocks))
		return 0;
	
	return orig_width;
}

uint basis_get_image_height(void *h, uint image_index, uint level_index)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;
	
	assert(f->m_magic == MAGIC);
	if (f->m_magic != MAGIC)
		return 0;		
	
	uint orig_width, orig_height, total_blocks;	
	if (!f->m_transcoder.get_image_level_desc(f->m_pFile, f->m_file_size, image_index, level_index, orig_width, orig_height, total_blocks))
		return 0;
	
	return orig_height;
}

uint basis_get_image_transcoded_size_in_bytes(void *h, uint image_index, uint level_index, uint format)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;
	
	assert(f->m_magic == MAGIC);	
	if (f->m_magic != MAGIC)
		return 0;	
	
	if (format >= (int)transcoder_texture_format::cTFTotalTextureFormats)
		return 0;
	
	uint bytes_per_block = basis_get_bytes_per_block((transcoder_texture_format)format);
	
	uint orig_width, orig_height, total_blocks;	
	if (!f->m_transcoder.get_image_level_desc(f->m_pFile, f->m_file_size, image_index, level_index, orig_width, orig_height, total_blocks))
		return 0;
	
	return total_blocks * bytes_per_block;
}

uint basis_start_transcoding(void *h)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;
    
	assert(f->m_magic == MAGIC);
	if (f->m_magic != MAGIC)
		return 0;	
		
	return f->m_transcoder.start_transcoding(f->m_pFile, f->m_file_size);
}

uint basis_transcode_image(void *h, void *dst, uint dst_size_in_bytes, 
	uint image_index, uint level_index, uint format, 
	uint unused, uint get_alpha_for_opaque_formats)
{
	basis_file *f = static_cast<basis_file *>(h);
	if (!f)
		return 0;
		
	assert(f->m_magic == MAGIC);	
	if (f->m_magic != MAGIC)
		return 0;
		
	if (format >= (int)transcoder_texture_format::cTFTotalTextureFormats)
		return 0;
		
	uint bytes_per_block = basis_get_bytes_per_block((transcoder_texture_format)format);
	
	return f->m_transcoder.transcode_image_level(f->m_pFile, f->m_file_size, image_index, level_index,
		dst, dst_size_in_bytes / bytes_per_block,
		(basist::transcoder_texture_format)format, 
		(get_alpha_for_opaque_formats ? basisu_transcoder::cDecodeFlagsTranscodeAlphaDataToOpaqueFormats : 0));
}
