/*
 * Copyright 2016-2017 The Brenwill Workshop Ltd.
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

#ifndef SPIRV_CROSS_MSL_HPP
#define SPIRV_CROSS_MSL_HPP

#include "spirv_glsl.hpp"
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace spirv_cross
{

// Defines MSL characteristics of a vertex attribute at a particular location.
// The used_by_shader flag is set to true during compilation of SPIR-V to MSL
// if the shader makes use of this vertex attribute.
struct MSLVertexAttr
{
	uint32_t location = 0;
	uint32_t msl_buffer = 0;
	uint32_t msl_offset = 0;
	uint32_t msl_stride = 0;
	bool per_instance = false;
	bool used_by_shader = false;
};

// Matches the binding index of a MSL resource for a binding within a descriptor set.
// Taken together, the stage, desc_set and binding combine to form a reference to a resource
// descriptor used in a particular shading stage. Generally, only one of the buffer, texture,
// or sampler elements will be populated. The used_by_shader flag is set to true during
// compilation of SPIR-V to MSL if the shader makes use of this vertex attribute.
struct MSLResourceBinding
{
	spv::ExecutionModel stage;
	uint32_t desc_set = 0;
	uint32_t binding = 0;

	uint32_t msl_buffer = 0;
	uint32_t msl_texture = 0;
	uint32_t msl_sampler = 0;

	bool used_by_shader = false;
};

// Tracks the type ID and member index of a struct member
using MSLStructMemberKey = uint64_t;

// Special constant used in a MSLResourceBinding desc_set
// element to indicate the bindings for the push constants.
static const uint32_t kPushConstDescSet = UINT32_MAX;

// Special constant used in a MSLResourceBinding binding
// element to indicate the bindings for the push constants.
static const uint32_t kPushConstBinding = 0;

// Decompiles SPIR-V to Metal Shading Language
class CompilerMSL : public CompilerGLSL
{
public:
	// Options for compiling to Metal Shading Language
	struct Options
	{
		bool flip_vert_y = false;
		bool enable_point_size_builtin = true;
		std::string entry_point_name;
	};

	const Options &get_options() const
	{
		return options;
	}

	void set_options(Options &opts)
	{
		options = opts;
	}

	// An enum of SPIR-V functions that are implemented in additional
	// source code that is added to the shader if necessary.
	enum SPVFuncImpl
	{
		SPVFuncImplNone,
		SPVFuncImplMod,
		SPVFuncImplRadians,
		SPVFuncImplDegrees,
		SPVFuncImplFindILsb,
		SPVFuncImplFindSMsb,
		SPVFuncImplFindUMsb,
		SPVFuncImplInverse2x2,
		SPVFuncImplInverse3x3,
		SPVFuncImplInverse4x4,
	};

	// Constructs an instance to compile the SPIR-V code into Metal Shading Language,
	// using the configuration parameters, if provided:
	//  - p_vtx_attrs is an optional list of vertex attribute bindings used to match
	//    vertex content locations to MSL attributes. If vertex attributes are provided,
	//    the compiler will set the used_by_shader flag to true in any vertex attribute
	//    actually used by the MSL code.
	//  - p_res_bindings is a list of resource bindings to indicate the MSL buffer,
	//    texture or sampler index to use for a particular SPIR-V description set
	//    and binding. If resource bindings are provided, the compiler will set the
	//    used_by_shader flag to true in any resource binding actually used by the MSL code.
	CompilerMSL(std::vector<uint32_t> spirv, std::vector<MSLVertexAttr> *p_vtx_attrs = nullptr,
	            std::vector<MSLResourceBinding> *p_res_bindings = nullptr);

	// Alternate constructor avoiding use of std::vectors.
	CompilerMSL(const uint32_t *ir, size_t word_count, MSLVertexAttr *p_vtx_attrs = nullptr, size_t vtx_attrs_count = 0,
	            MSLResourceBinding *p_res_bindings = nullptr, size_t res_bindings_count = 0);

	// Compiles the SPIR-V code into Metal Shading Language.
	std::string compile() override;

	// Compiles the SPIR-V code into Metal Shading Language, overriding configuration parameters.
	// Any of the parameters here may be null to indicate that the configuration provided in the
	// constructor should be used. They are not declared as optional to avoid a conflict with the
	// inherited and overridden zero-parameter compile() function.
	std::string compile(std::vector<MSLVertexAttr> *p_vtx_attrs, std::vector<MSLResourceBinding> *p_res_bindings);

	// This legacy method is deprecated.
	typedef Options MSLConfiguration;
	SPIRV_CROSS_DEPRECATED("Please use get_options() and set_options() instead.")
	std::string compile(MSLConfiguration &msl_cfg, std::vector<MSLVertexAttr> *p_vtx_attrs = nullptr,
	                    std::vector<MSLResourceBinding> *p_res_bindings = nullptr);

protected:
	void emit_instruction(const Instruction &instr) override;
	void emit_glsl_op(uint32_t result_type, uint32_t result_id, uint32_t op, const uint32_t *args,
	                  uint32_t count) override;
	void emit_header() override;
	void emit_function_prototype(SPIRFunction &func, uint64_t return_flags) override;
	void emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id) override;
	void emit_fixup() override;
	void emit_struct_member(const SPIRType &type, uint32_t member_type_id, uint32_t index,
	                        const std::string &qualifier = "") override;
	std::string type_to_glsl(const SPIRType &type, uint32_t id = 0) override;
	std::string image_type_glsl(const SPIRType &type, uint32_t id = 0) override;
	std::string builtin_to_glsl(spv::BuiltIn builtin) override;
	std::string constant_expression(const SPIRConstant &c) override;
	size_t get_declared_struct_member_size(const SPIRType &struct_type, uint32_t index) const override;
	std::string to_func_call_arg(uint32_t id) override;
	std::string to_name(uint32_t id, bool allow_alias = true) const override;
	std::string to_function_name(uint32_t img, const SPIRType &imgtype, bool is_fetch, bool is_gather, bool is_proj,
	                             bool has_array_offsets, bool has_offset, bool has_grad, bool has_dref,
	                             uint32_t lod) override;
	std::string to_function_args(uint32_t img, const SPIRType &imgtype, bool is_fetch, bool is_gather, bool is_proj,
	                             uint32_t coord, uint32_t coord_components, uint32_t dref, uint32_t grad_x,
	                             uint32_t grad_y, uint32_t lod, uint32_t coffset, uint32_t offset, uint32_t bias,
	                             uint32_t comp, uint32_t sample, bool *p_forward) override;
	std::string unpack_expression_type(std::string expr_str, const SPIRType &type) override;
	std::string bitcast_glsl_op(const SPIRType &result_type, const SPIRType &argument_type) override;
	bool skip_argument(uint32_t id) const override;

	void preprocess_op_codes();
	void localize_global_variables();
	void extract_global_variables_from_functions();

	std::unordered_map<uint32_t, std::set<uint32_t>> function_global_vars;
	void extract_global_variables_from_function(uint32_t func_id, std::set<uint32_t> &added_arg_ids,
	                                            std::unordered_set<uint32_t> &global_var_ids,
	                                            std::unordered_set<uint32_t> &processed_func_ids);
	uint32_t add_interface_block(spv::StorageClass storage);
	void mark_location_as_used_by_shader(uint32_t location, spv::StorageClass storage);

	void emit_custom_functions();
	void emit_resources();
	void emit_specialization_constants();
	void emit_interface_block(uint32_t ib_var_id);
	void populate_func_name_overrides();
	void populate_var_name_overrides();

	std::string func_type_decl(SPIRType &type);
	std::string clean_func_name(std::string func_name) override;
	std::string entry_point_args(bool append_comma);
	std::string get_entry_point_name();
	std::string to_qualified_member_name(const SPIRType &type, uint32_t index);
	std::string ensure_valid_name(std::string name, std::string pfx);
	std::string to_sampler_expression(uint32_t id);
	std::string builtin_qualifier(spv::BuiltIn builtin);
	std::string builtin_type_decl(spv::BuiltIn builtin);
	std::string built_in_func_arg(spv::BuiltIn builtin, bool prefix_comma);
	std::string member_attribute_qualifier(const SPIRType &type, uint32_t index);
	std::string argument_decl(const SPIRFunction::Parameter &arg);
	std::string round_fp_tex_coords(std::string tex_coords, bool coord_is_fp);
	uint32_t get_metal_resource_index(SPIRVariable &var, SPIRType::BaseType basetype);
	uint32_t get_ordered_member_location(uint32_t type_id, uint32_t index);
	size_t get_declared_struct_member_alignment(const SPIRType &struct_type, uint32_t index) const;
	std::string to_component_argument(uint32_t id);
	void exclude_from_stage_in(SPIRVariable &var);
	void exclude_member_from_stage_in(const SPIRType &type, uint32_t index);
	std::string add_input_buffer_block_member(uint32_t mbr_type_id, std::string mbr_name, uint32_t mbr_locn);
	uint32_t get_input_buffer_block_var_id(uint32_t msl_buffer);
	void align_struct(SPIRType &ib_type);
	bool is_member_packable(SPIRType &ib_type, uint32_t index);
	MSLStructMemberKey get_struct_member_key(uint32_t type_id, uint32_t index);
	SPVFuncImpl get_spv_func_impl(spv::Op opcode, const uint32_t *args);
	std::string get_argument_address_space(const SPIRVariable &argument);
	void emit_atomic_func_op(uint32_t result_type, uint32_t result_id, const char *op, uint32_t mem_order_1,
	                         uint32_t mem_order_2, bool has_mem_order_2, uint32_t op0, uint32_t op1 = 0,
	                         bool op1_is_pointer = false, uint32_t op2 = 0);
	const char *get_memory_order(uint32_t spv_mem_sem);
	void add_pragma_line(const std::string &line);

	Options options;
	std::unordered_map<std::string, std::string> func_name_overrides;
	std::unordered_map<std::string, std::string> var_name_overrides;
	std::set<SPVFuncImpl> spv_function_implementations;
	std::unordered_map<uint32_t, MSLVertexAttr *> vtx_attrs_by_location;
	std::map<uint32_t, uint32_t> non_stage_in_input_var_ids;
	std::unordered_map<MSLStructMemberKey, uint32_t> struct_member_padding;
	std::vector<std::string> pragma_lines;
	std::vector<MSLResourceBinding *> resource_bindings;
	MSLResourceBinding next_metal_resource_index;
	uint32_t stage_in_var_id = 0;
	uint32_t stage_out_var_id = 0;
	uint32_t stage_uniforms_var_id = 0;
	bool needs_vertex_idx_arg = false;
	bool needs_instance_idx_arg = false;
	std::string qual_pos_var_name;
	std::string stage_in_var_name = "in";
	std::string stage_out_var_name = "out";
	std::string stage_uniform_var_name = "uniforms";
	std::string sampler_name_suffix = "Smplr";

	// OpcodeHandler that handles several MSL preprocessing operations.
	struct OpCodePreprocessor : OpcodeHandler
	{
		OpCodePreprocessor(CompilerMSL &compiler_)
		    : compiler(compiler_)
		{
		}

		bool handle(spv::Op opcode, const uint32_t *args, uint32_t length) override;

		CompilerMSL &compiler;
		bool suppress_missing_prototypes = false;
		bool uses_atomics = false;
	};

	// Sorts the members of a SPIRType and associated Meta info based on a settable sorting
	// aspect, which defines which aspect of the struct members will be used to sort them.
	// Regardless of the sorting aspect, built-in members always appear at the end of the struct.
	struct MemberSorter
	{
		enum SortAspect
		{
			Location,
			LocationReverse,
			Offset,
			OffsetThenLocationReverse,
			Alphabetical
		};

		void sort();
		bool operator()(uint32_t mbr_idx1, uint32_t mbr_idx2);
		MemberSorter(SPIRType &t, Meta &m, SortAspect sa);

		SPIRType &type;
		Meta &meta;
		SortAspect sort_aspect;
	};
};
}

#endif
