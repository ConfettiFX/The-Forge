/*
 * Copyright 2016-2017 Robert Konrad
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SPIRV_HLSL_HPP
#define SPIRV_HLSL_HPP

#include "spirv_glsl.hpp"
#include <utility>
#include <vector>

namespace spirv_cross
{
class CompilerHLSL : public CompilerGLSL
{
public:
	struct Options
	{
		uint32_t shader_model = 30; // TODO: map ps_4_0_level_9_0,... somehow
		bool fixup_clipspace = false;
		bool flip_vert_y = false;

		// Allows the PointSize builtin, and ignores it, as PointSize is not supported in HLSL.
		bool point_size_compat = false;
	};

	CompilerHLSL(std::vector<uint32_t> spirv_)
	    : CompilerGLSL(move(spirv_))
	{
	}

	CompilerHLSL(const uint32_t *ir, size_t size)
	    : CompilerGLSL(ir, size)
	{
	}

	const Options &get_options() const
	{
		return options;
	}

	void set_options(Options &opts)
	{
		options = opts;
	}

	std::string compile() override;

private:
	std::string type_to_glsl(const SPIRType &type, uint32_t id = 0) override;
	std::string image_type_hlsl(const SPIRType &type);
	std::string image_type_hlsl_modern(const SPIRType &type);
	std::string image_type_hlsl_legacy(const SPIRType &type);
	void emit_function_prototype(SPIRFunction &func, uint64_t return_flags) override;
	void emit_hlsl_entry_point();
	void emit_header() override;
	void emit_resources();
	void emit_interface_block_globally(const SPIRVariable &type);
	void emit_interface_block_in_struct(const SPIRVariable &type, std::unordered_set<uint32_t> &active_locations);
	void emit_builtin_inputs_in_struct();
	void emit_builtin_outputs_in_struct();
	void emit_texture_op(const Instruction &i) override;
	void emit_instruction(const Instruction &instruction) override;
	void emit_glsl_op(uint32_t result_type, uint32_t result_id, uint32_t op, const uint32_t *args,
	                  uint32_t count) override;
	void emit_buffer_block(const SPIRVariable &type) override;
	void emit_push_constant_block(const SPIRVariable &var) override;
	void emit_uniform(const SPIRVariable &var) override;
	void emit_modern_uniform(const SPIRVariable &var);
	void emit_legacy_uniform(const SPIRVariable &var);
	std::string layout_for_member(const SPIRType &type, uint32_t index) override;
	std::string to_interpolation_qualifiers(uint64_t flags) override;
	std::string bitcast_glsl_op(const SPIRType &result_type, const SPIRType &argument_type) override;
	std::string to_func_call_arg(uint32_t id) override;
	std::string to_sampler_expression(uint32_t id);
	std::string to_resource_binding(const SPIRVariable &var);
	std::string to_resource_binding_sampler(const SPIRVariable &var);
	void emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id) override;

	const char *to_storage_qualifiers_glsl(const SPIRVariable &var) override;

	Options options;
	bool requires_op_fmod = false;
	bool requires_textureProj = false;

	void emit_builtin_variables();
	bool require_output = false;
	bool require_input = false;

	uint32_t type_to_consumed_locations(const SPIRType &type) const;

	void emit_io_block(const SPIRVariable &var);
};
}

#endif
