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

#include "spirv_hlsl.hpp"
#include "GLSL.std.450.h"
#include <algorithm>

using namespace spv;
using namespace spirv_cross;
using namespace std;

// Returns true if an arithmetic operation does not change behavior depending on signedness.
static bool opcode_is_sign_invariant(Op opcode)
{
	switch (opcode)
	{
	case OpIEqual:
	case OpINotEqual:
	case OpISub:
	case OpIAdd:
	case OpIMul:
	case OpShiftLeftLogical:
	case OpBitwiseOr:
	case OpBitwiseXor:
	case OpBitwiseAnd:
		return true;

	default:
		return false;
	}
}

string CompilerHLSL::image_type_hlsl_modern(const SPIRType &type)
{
	auto &imagetype = get<SPIRType>(type.image.type);
	const char *dim = nullptr;
	switch (type.image.dim)
	{
	case Dim1D:
		dim = "1D";
		break;
	case Dim2D:
		dim = "2D";
		break;
	case Dim3D:
		dim = "3D";
		break;
	case DimCube:
		dim = "Cube";
		break;
	case DimRect:
		SPIRV_CROSS_THROW("Rectangle texture support is not yet implemented for HLSL"); // TODO
	case DimBuffer:
		// Buffer/RWBuffer.
		SPIRV_CROSS_THROW("Buffer/RWBuffer support is not yet implemented for HLSL"); // TODO
	case DimSubpassData:
		// This should be implemented same way as desktop GL. Fetch on a 2D texture based on int2(SV_Position).
		SPIRV_CROSS_THROW("Subpass data support is not yet implemented for HLSL"); // TODO
	default:
		SPIRV_CROSS_THROW("Invalid dimension.");
	}
	uint32_t components = 4;
	const char *arrayed = type.image.arrayed ? "Array" : "";
	return join("Texture", dim, arrayed, "<", type_to_glsl(imagetype), components, ">");
}

string CompilerHLSL::image_type_hlsl_legacy(const SPIRType &type)
{
	auto &imagetype = get<SPIRType>(type.image.type);
	string res;

	switch (imagetype.basetype)
	{
	case SPIRType::Int:
		res = "i";
		break;
	case SPIRType::UInt:
		res = "u";
		break;
	default:
		break;
	}

	if (type.basetype == SPIRType::Image && type.image.dim == DimSubpassData)
		return res + "subpassInput" + (type.image.ms ? "MS" : "");

	// If we're emulating subpassInput with samplers, force sampler2D
	// so we don't have to specify format.
	if (type.basetype == SPIRType::Image && type.image.dim != DimSubpassData)
	{
		// Sampler buffers are always declared as samplerBuffer even though they might be separate images in the SPIR-V.
		if (type.image.dim == DimBuffer && type.image.sampled == 1)
			res += "sampler";
		else
			res += type.image.sampled == 2 ? "image" : "texture";
	}
	else
		res += "sampler";

	switch (type.image.dim)
	{
	case Dim1D:
		res += "1D";
		break;
	case Dim2D:
		res += "2D";
		break;
	case Dim3D:
		res += "3D";
		break;
	case DimCube:
		res += "CUBE";
		break;

	case DimBuffer:
		res += "Buffer";
		break;

	case DimSubpassData:
		res += "2D";
		break;
	default:
		SPIRV_CROSS_THROW("Only 1D, 2D, 3D, Buffer, InputTarget and Cube textures supported.");
	}

	if (type.image.ms)
		res += "MS";
	if (type.image.arrayed)
		res += "Array";
	if (type.image.depth)
		res += "Shadow";

	return res;
}

string CompilerHLSL::image_type_hlsl(const SPIRType &type)
{
	if (options.shader_model <= 30)
		return image_type_hlsl_legacy(type);
	else
		return image_type_hlsl_modern(type);
}

// The optional id parameter indicates the object whose type we are trying
// to find the description for. It is optional. Most type descriptions do not
// depend on a specific object's use of that type.
string CompilerHLSL::type_to_glsl(const SPIRType &type, uint32_t id)
{
	// Ignore the pointer type since GLSL doesn't have pointers.

	switch (type.basetype)
	{
	case SPIRType::Struct:
		// Need OpName lookup here to get a "sensible" name for a struct.
		if (backend.explicit_struct_type)
			return join("struct ", to_name(type.self));
		else
			return to_name(type.self);

	case SPIRType::Image:
	case SPIRType::SampledImage:
		return image_type_hlsl(type);

	case SPIRType::Sampler:
		return comparison_samplers.count(id) ? "SamplerComparisonState" : "SamplerState";

	case SPIRType::Void:
		return "void";

	default:
		break;
	}

	if (type.vecsize == 1 && type.columns == 1) // Scalar builtin
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return "bool";
		case SPIRType::Int:
			return backend.basic_int_type;
		case SPIRType::UInt:
			return backend.basic_uint_type;
		case SPIRType::AtomicCounter:
			return "atomic_uint";
		case SPIRType::Float:
			return "float";
		case SPIRType::Double:
			return "double";
		case SPIRType::Int64:
			return "int64_t";
		case SPIRType::UInt64:
			return "uint64_t";
		default:
			return "???";
		}
	}
	else if (type.vecsize > 1 && type.columns == 1) // Vector builtin
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return join("bool", type.vecsize);
		case SPIRType::Int:
			return join("int", type.vecsize);
		case SPIRType::UInt:
			return join("uint", type.vecsize);
		case SPIRType::Float:
			return join("float", type.vecsize);
		case SPIRType::Double:
			return join("double", type.vecsize);
		case SPIRType::Int64:
			return join("i64vec", type.vecsize);
		case SPIRType::UInt64:
			return join("u64vec", type.vecsize);
		default:
			return "???";
		}
	}
	else
	{
		switch (type.basetype)
		{
		case SPIRType::Boolean:
			return join("bool", type.columns, "x", type.vecsize);
		case SPIRType::Int:
			return join("int", type.columns, "x", type.vecsize);
		case SPIRType::UInt:
			return join("uint", type.columns, "x", type.vecsize);
		case SPIRType::Float:
			return join("float", type.columns, "x", type.vecsize);
		case SPIRType::Double:
			return join("double", type.columns, "x", type.vecsize);
		// Matrix types not supported for int64/uint64.
		default:
			return "???";
		}
	}
}

void CompilerHLSL::emit_header()
{
	for (auto &header : header_lines)
		statement(header);

	if (header_lines.size() > 0)
	{
		statement("");
	}
}

void CompilerHLSL::emit_interface_block_globally(const SPIRVariable &var)
{
	add_resource_name(var.self);

	// The global copies of I/O variables should not contain interpolation qualifiers.
	// These are emitted inside the interface structs.
	auto &flags = meta[var.self].decoration.decoration_flags;
	auto old_flags = flags;
	flags = 0;
	statement("static ", variable_decl(var), ";");
	flags = old_flags;
}

const char *CompilerHLSL::to_storage_qualifiers_glsl(const SPIRVariable &var)
{
	// Input and output variables are handled specially in HLSL backend.
	// The variables are declared as global, private variables, and do not need any qualifiers.
	if (var.storage == StorageClassUniformConstant || var.storage == StorageClassUniform ||
	    var.storage == StorageClassPushConstant)
	{
		return "uniform ";
	}

	return "";
}

void CompilerHLSL::emit_builtin_outputs_in_struct()
{
	bool legacy = options.shader_model <= 30;
	for (uint32_t i = 0; i < 64; i++)
	{
		if (!(active_output_builtins & (1ull << i)))
			continue;

		const char *type = nullptr;
		const char *semantic = nullptr;
		auto builtin = static_cast<BuiltIn>(i);
		switch (builtin)
		{
		case BuiltInPosition:
			type = "float4";
			semantic = legacy ? "POSITION" : "SV_Position";
			break;

		case BuiltInFragDepth:
			type = "float";
			semantic = legacy ? "DEPTH" : "SV_Depth";
			break;

		case BuiltInPointSize:
			// If point_size_compat is enabled, just ignore PointSize.
			// PointSize does not exist in HLSL, but some code bases might want to be able to use these shaders,
			// even if it means working around the missing feature.
			if (options.point_size_compat)
				break;
			else
				SPIRV_CROSS_THROW("Unsupported builtin in HLSL.");

		default:
			SPIRV_CROSS_THROW("Unsupported builtin in HLSL.");
			break;
		}

		if (type && semantic)
			statement(type, " ", builtin_to_glsl(builtin), " : ", semantic, ";");
	}
}

void CompilerHLSL::emit_builtin_inputs_in_struct()
{
	bool legacy = options.shader_model <= 30;
	for (uint32_t i = 0; i < 64; i++)
	{
		if (!(active_input_builtins & (1ull << i)))
			continue;

		const char *type = nullptr;
		const char *semantic = nullptr;
		auto builtin = static_cast<BuiltIn>(i);
		switch (builtin)
		{
		case BuiltInFragCoord:
			type = "float4";
			semantic = legacy ? "VPOS" : "SV_Position";
			break;

		case BuiltInVertexIndex:
			if (legacy)
				SPIRV_CROSS_THROW("Vertex index not supported in SM 3.0 or lower.");
			type = "uint";
			semantic = "SV_VertexID";
			break;

		case BuiltInInstanceIndex:
			if (legacy)
				SPIRV_CROSS_THROW("Instance index not supported in SM 3.0 or lower.");
			type = "uint";
			semantic = "SV_InstanceID";
			break;

		case BuiltInSampleId:
			if (legacy)
				SPIRV_CROSS_THROW("Sample ID not supported in SM 3.0 or lower.");
			type = "uint";
			semantic = "SV_SampleIndex";
			break;

		default:
			SPIRV_CROSS_THROW("Unsupported builtin in HLSL.");
			break;
		}

		if (type && semantic)
			statement(type, " ", builtin_to_glsl(builtin), " : ", semantic, ";");
	}
}

uint32_t CompilerHLSL::type_to_consumed_locations(const SPIRType &type) const
{
	// TODO: Need to verify correctness.
	uint32_t elements = 0;

	if (type.basetype == SPIRType::Struct)
	{
		for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++)
			elements += type_to_consumed_locations(get<SPIRType>(type.member_types[i]));
	}
	else
	{
		uint32_t array_multiplier = 1;
		for (uint32_t i = 0; i < uint32_t(type.array.size()); i++)
		{
			if (type.array_size_literal[i])
				array_multiplier *= type.array[i];
			else
				array_multiplier *= get<SPIRConstant>(type.array[i]).scalar();
		}
		elements += array_multiplier * type.columns;
	}
	return elements;
}

string CompilerHLSL::to_interpolation_qualifiers(uint64_t flags)
{
	string res;
	//if (flags & (1ull << DecorationSmooth))
	//    res += "linear ";
	if (flags & (1ull << DecorationFlat))
		res += "nointerpolation ";
	if (flags & (1ull << DecorationNoPerspective))
		res += "noperspective ";
	if (flags & (1ull << DecorationCentroid))
		res += "centroid ";
	if (flags & (1ull << DecorationPatch))
		res += "patch "; // Seems to be different in actual HLSL.
	if (flags & (1ull << DecorationSample))
		res += "sample ";
	if (flags & (1ull << DecorationInvariant))
		res += "invariant "; // Not supported?

	return res;
}

void CompilerHLSL::emit_io_block(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	add_resource_name(type.self);

	statement("struct ", to_name(type.self));
	begin_scope();
	type.member_name_cache.clear();

	uint32_t base_location = get_decoration(var.self, DecorationLocation);

	for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++)
	{
		string semantic;
		if (has_member_decoration(type.self, i, DecorationLocation))
		{
			uint32_t location = get_member_decoration(type.self, i, DecorationLocation);
			semantic = join(" : TEXCOORD", location);
		}
		else
		{
			// If the block itself has a location, but not its members, use the implicit location.
			// There could be a conflict if the block members partially specialize the locations.
			// It is unclear how SPIR-V deals with this. Assume this does not happen for now.
			uint32_t location = base_location + i;
			semantic = join(" : TEXCOORD", location);
		}

		add_member_name(type, i);

		auto &membertype = get<SPIRType>(type.member_types[i]);
		statement(to_interpolation_qualifiers(get_member_decoration_mask(type.self, i)),
		          variable_decl(membertype, to_member_name(type, i)), semantic, ";");
	}

	end_scope_decl();
	statement("");

	statement("static ", variable_decl(var), ";");
	statement("");
}

void CompilerHLSL::emit_interface_block_in_struct(const SPIRVariable &var, unordered_set<uint32_t> &active_locations)
{
	auto &execution = get_entry_point();
	auto &type = get<SPIRType>(var.basetype);

	string binding;
	bool use_binding_number = true;
	bool legacy = options.shader_model <= 30;
	if (execution.model == ExecutionModelFragment && var.storage == StorageClassOutput)
	{
		binding = join(legacy ? "COLOR" : "SV_Target", get_decoration(var.self, DecorationLocation));
		use_binding_number = false;
	}

	const auto get_vacant_location = [&]() -> uint32_t {
		for (uint32_t i = 0; i < 64; i++)
			if (!active_locations.count(i))
				return i;
		SPIRV_CROSS_THROW("All locations from 0 to 63 are exhausted.");
	};

	auto &m = meta[var.self].decoration;
	auto name = to_name(var.self);
	if (use_binding_number)
	{
		uint32_t binding_number;

		// If an explicit location exists, use it with TEXCOORD[N] semantic.
		// Otherwise, pick a vacant location.
		if (m.decoration_flags & (1ull << DecorationLocation))
			binding_number = m.location;
		else
			binding_number = get_vacant_location();

		if (type.columns > 1)
		{
			if (!type.array.empty())
				SPIRV_CROSS_THROW("Arrays of matrices used as input/output. This is not supported.");

			// Unroll matrices.
			for (uint32_t i = 0; i < type.columns; i++)
			{
				SPIRType newtype = type;
				newtype.columns = 1;
				statement(to_interpolation_qualifiers(get_decoration_mask(var.self)),
				          variable_decl(newtype, join(name, "_", i)), " : TEXCOORD", binding_number, ";");
				active_locations.insert(binding_number++);
			}
		}
		else
		{
			statement(to_interpolation_qualifiers(get_decoration_mask(var.self)), variable_decl(type, name),
			          " : TEXCOORD", binding_number, ";");

			// Structs and arrays should consume more locations.
			uint32_t consumed_locations = type_to_consumed_locations(type);
			for (uint32_t i = 0; i < consumed_locations; i++)
				active_locations.insert(binding_number + i);
		}
	}
	else
		statement(variable_decl(type, name), " : ", binding, ";");
}

void CompilerHLSL::emit_builtin_variables()
{
	// Emit global variables for the interface variables which are statically used by the shader.
	for (uint32_t i = 0; i < 64; i++)
	{
		if (!((active_input_builtins | active_output_builtins) & (1ull << i)))
			continue;

		const char *type = nullptr;
		auto builtin = static_cast<BuiltIn>(i);

		switch (builtin)
		{
		case BuiltInFragCoord:
		case BuiltInPosition:
			type = "float4";
			break;

		case BuiltInFragDepth:
			type = "float";
			break;

		case BuiltInVertexIndex:
		case BuiltInInstanceIndex:
		case BuiltInSampleId:
			type = "int";
			break;

		case BuiltInPointSize:
			if (options.point_size_compat)
			{
				// Just emit the global variable, it will be ignored.
				type = "float";
				break;
			}
			else
				SPIRV_CROSS_THROW(join("Unsupported builtin in HLSL: ", unsigned(builtin)));

		default:
			SPIRV_CROSS_THROW(join("Unsupported builtin in HLSL: ", unsigned(builtin)));
			break;
		}

		if (type)
			statement("static ", type, " ", builtin_to_glsl(builtin), ";");
	}
}

void CompilerHLSL::emit_resources()
{
	auto &execution = get_entry_point();

	// Output all basic struct types which are not Block or BufferBlock as these are declared inplace
	// when such variables are instantiated.
	for (auto &id : ids)
	{
		if (id.get_type() == TypeType)
		{
			auto &type = id.get<SPIRType>();
			if (type.basetype == SPIRType::Struct && type.array.empty() && !type.pointer &&
			    (meta[type.self].decoration.decoration_flags &
			     ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))) == 0)
			{
				emit_struct(type);
			}
		}
	}

	bool emitted = false;

	// Output UBOs and SSBOs
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			if (var.storage != StorageClassFunction && type.pointer && type.storage == StorageClassUniform &&
			    !is_hidden_variable(var) && (meta[type.self].decoration.decoration_flags &
			                                 ((1ull << DecorationBlock) | (1ull << DecorationBufferBlock))))
			{
				emit_buffer_block(var);
				emitted = true;
			}
		}
	}

	// Output push constant blocks
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			if (var.storage != StorageClassFunction && type.pointer && type.storage == StorageClassPushConstant &&
			    !is_hidden_variable(var))
			{
				emit_push_constant_block(var);
				emitted = true;
			}
		}
	}

	if (execution.model == ExecutionModelVertex && options.shader_model <= 30)
	{
		statement("uniform float4 gl_HalfPixel;");
		emitted = true;
	}

	// Output Uniform Constants (values, samplers, images, etc).
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);

			if (var.storage != StorageClassFunction && !is_builtin_variable(var) && !var.remapped_variable &&
			    type.pointer &&
			    (type.storage == StorageClassUniformConstant || type.storage == StorageClassAtomicCounter))
			{
				emit_uniform(var);
				emitted = true;
			}
		}
	}

	if (emitted)
		statement("");
	emitted = false;

	// Emit builtin input and output variables here.
	emit_builtin_variables();

	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			bool block = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)) != 0;

			// Do not emit I/O blocks here.
			// I/O blocks can be arrayed, so we must deal with them separately to support geometry shaders
			// and tessellation down the line.
			if (!block && var.storage != StorageClassFunction && !var.remapped_variable && type.pointer &&
			    (var.storage == StorageClassInput || var.storage == StorageClassOutput) && !is_builtin_variable(var) &&
			    interface_variable_exists_in_entry_point(var.self))
			{
				// Only emit non-builtins which are not blocks here. Builtin variables are handled separately.
				emit_interface_block_globally(var);
				emitted = true;
			}
		}
	}

	if (emitted)
		statement("");
	emitted = false;

	require_input = false;
	require_output = false;
	unordered_set<uint32_t> active_inputs;
	unordered_set<uint32_t> active_outputs;
	vector<SPIRVariable *> input_variables;
	vector<SPIRVariable *> output_variables;
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			bool block = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)) != 0;

			if (var.storage != StorageClassInput && var.storage != StorageClassOutput)
				continue;

			// Do not emit I/O blocks here.
			// I/O blocks can be arrayed, so we must deal with them separately to support geometry shaders
			// and tessellation down the line.
			if (!block && !var.remapped_variable && type.pointer && !is_builtin_variable(var) &&
			    interface_variable_exists_in_entry_point(var.self))
			{
				if (var.storage == StorageClassInput)
					input_variables.push_back(&var);
				else
					output_variables.push_back(&var);
			}

			// Reserve input and output locations for block variables as necessary.
			if (block && !is_builtin_variable(var) && interface_variable_exists_in_entry_point(var.self))
			{
				auto &active = var.storage == StorageClassInput ? active_inputs : active_outputs;
				for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++)
				{
					if (has_member_decoration(type.self, i, DecorationLocation))
					{
						uint32_t location = get_member_decoration(type.self, i, DecorationLocation);
						active.insert(location);
					}
				}

				// Emit the block struct and a global variable here.
				emit_io_block(var);
			}
		}
	}

	const auto variable_compare = [&](const SPIRVariable *a, const SPIRVariable *b) -> bool {
		// Sort input and output variables based on, from more robust to less robust:
		// - Location
		// - Variable has a location
		// - Name comparison
		// - Variable has a name
		// - Fallback: ID
		bool has_location_a = has_decoration(a->self, DecorationLocation);
		bool has_location_b = has_decoration(b->self, DecorationLocation);

		if (has_location_a && has_location_b)
		{
			return get_decoration(a->self, DecorationLocation) < get_decoration(b->self, DecorationLocation);
		}
		else if (has_location_a && !has_location_b)
			return true;
		else if (!has_location_a && has_location_b)
			return false;

		const auto &name1 = to_name(a->self);
		const auto &name2 = to_name(b->self);

		if (name1.empty() && name2.empty())
			return a->self < b->self;
		else if (name1.empty())
			return true;
		else if (name2.empty())
			return false;

		return name1.compare(name2) < 0;
	};

	if (!input_variables.empty() || active_input_builtins)
	{
		require_input = true;
		statement("struct SPIRV_Cross_Input");

		begin_scope();
		sort(input_variables.begin(), input_variables.end(), variable_compare);
		for (auto var : input_variables)
			emit_interface_block_in_struct(*var, active_inputs);
		emit_builtin_inputs_in_struct();
		end_scope_decl();
		statement("");
	}

	if (!output_variables.empty() || active_output_builtins)
	{
		require_output = true;
		statement("struct SPIRV_Cross_Output");

		begin_scope();
		// FIXME: Use locations properly if they exist.
		sort(output_variables.begin(), output_variables.end(), variable_compare);
		for (auto var : output_variables)
			emit_interface_block_in_struct(*var, active_outputs);
		emit_builtin_outputs_in_struct();
		end_scope_decl();
		statement("");
	}

	// Global variables.
	for (auto global : global_variables)
	{
		auto &var = get<SPIRVariable>(global);
		if (var.storage != StorageClassOutput)
		{
			add_resource_name(var.self);
			statement("static ", variable_decl(var), ";");
			emitted = true;
		}
	}

	if (emitted)
		statement("");

	if (requires_op_fmod)
	{
		statement("float mod(float x, float y)");
		begin_scope();
		statement("return x - y * floor(x / y);");
		end_scope();
		statement("");
	}

	if (requires_textureProj)
	{
		if (options.shader_model >= 40)
		{
			statement("float SPIRV_Cross_projectTextureCoordinate(float2 coord)");
			begin_scope();
			statement("return coord.x / coord.y;");
			end_scope();
			statement("");

			statement("float2 SPIRV_Cross_projectTextureCoordinate(float3 coord)");
			begin_scope();
			statement("return float2(coord.x, coord.y) / coord.z;");
			end_scope();
			statement("");

			statement("float3 SPIRV_Cross_projectTextureCoordinate(float4 coord)");
			begin_scope();
			statement("return float3(coord.x, coord.y, coord.z) / coord.w;");
			end_scope();
			statement("");
		}
		else
		{
			statement("float4 SPIRV_Cross_projectTextureCoordinate(float2 coord)");
			begin_scope();
			statement("return float4(coord.x, 0.0, 0.0, coord.y);");
			end_scope();
			statement("");

			statement("float4 SPIRV_Cross_projectTextureCoordinate(float3 coord)");
			begin_scope();
			statement("return float4(coord.x, coord.y, 0.0, coord.z);");
			end_scope();
			statement("");

			statement("float4 SPIRV_Cross_projectTextureCoordinate(float4 coord)");
			begin_scope();
			statement("return coord;");
			end_scope();
			statement("");
		}
	}
}

string CompilerHLSL::layout_for_member(const SPIRType &, uint32_t)
{
	return "";
}

void CompilerHLSL::emit_buffer_block(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);

	bool is_uav = has_decoration(type.self, DecorationBufferBlock);
	if (is_uav)
		SPIRV_CROSS_THROW("Buffer is SSBO (UAV). This is currently unsupported.");

	add_resource_name(type.self);

	string struct_name;
	if (options.shader_model >= 51)
		struct_name = to_name(type.self);
	else
		struct_name = join("_", to_name(type.self));

	// First, declare the struct of the UBO.
	statement("struct ", struct_name);
	begin_scope();

	type.member_name_cache.clear();

	uint32_t i = 0;
	for (auto &member : type.member_types)
	{
		add_member_name(type, i);
		emit_struct_member(type, member, i);
		i++;
	}
	end_scope_decl();
	statement("");

	if (options.shader_model >= 51) // SM 5.1 uses ConstantBuffer<T> instead of cbuffer.
	{
		statement("ConstantBuffer<", struct_name, "> ", to_name(var.self), type_to_array_glsl(type), to_resource_binding(var), ";");
	}
	else
	{
		statement("cbuffer ", to_name(type.self), to_resource_binding(var));
		begin_scope();
		statement(struct_name, " ", to_name(var.self), type_to_array_glsl(type), ";");
		end_scope_decl();
	}
}

void CompilerHLSL::emit_push_constant_block(const SPIRVariable &var)
{
	emit_buffer_block(var);
}

string CompilerHLSL::to_sampler_expression(uint32_t id)
{
	return join("_", to_expression(id), "_sampler");
}

void CompilerHLSL::emit_sampled_image_op(uint32_t result_type, uint32_t result_id, uint32_t image_id, uint32_t samp_id)
{
	set<SPIRCombinedImageSampler>(result_id, result_type, image_id, samp_id);
}

string CompilerHLSL::to_func_call_arg(uint32_t id)
{
	string arg_str = CompilerGLSL::to_func_call_arg(id);

	if (options.shader_model <= 30)
		return arg_str;

	// Manufacture automatic sampler arg if the arg is a SampledImage texture and we're in modern HLSL.
	auto *var = maybe_get<SPIRVariable>(id);
	if (var)
	{
		auto &type = get<SPIRType>(var->basetype);

		// We don't have to consider combined image samplers here via OpSampledImage because
		// those variables cannot be passed as arguments to functions.
		// Only global SampledImage variables may be used as arguments.
		if (type.basetype == SPIRType::SampledImage)
			arg_str += ", " + to_sampler_expression(id);
	}

	return arg_str;
}

void CompilerHLSL::emit_function_prototype(SPIRFunction &func, uint64_t return_flags)
{
	auto &execution = get_entry_point();
	// Avoid shadow declarations.
	local_variable_names = resource_names;

	string decl;

	auto &type = get<SPIRType>(func.return_type);
	decl += flags_to_precision_qualifiers_glsl(type, return_flags);
	decl += type_to_glsl(type);
	decl += " ";

	if (func.self == entry_point)
	{
		if (execution.model == ExecutionModelVertex)
		{
			decl += "vert_main";
		}
		else
		{
			decl += "frag_main";
		}
		processing_entry_point = true;
	}
	else
		decl += to_name(func.self);

	decl += "(";
	for (auto &arg : func.arguments)
	{
		// Might change the variable name if it already exists in this function.
		// SPIRV OpName doesn't have any semantic effect, so it's valid for an implementation
		// to use same name for variables.
		// Since we want to make the GLSL debuggable and somewhat sane, use fallback names for variables which are duplicates.
		add_local_variable_name(arg.id);

		decl += argument_decl(arg);

		// Flatten a combined sampler to two separate arguments in modern HLSL.
		auto &arg_type = get<SPIRType>(arg.type);
		if (options.shader_model > 30 && arg_type.basetype == SPIRType::SampledImage)
		{
			// Manufacture automatic sampler arg for SampledImage texture
			decl += ", ";
			if (arg_type.basetype == SPIRType::SampledImage)
				decl += join(arg_type.image.depth ? "SamplerComparisonState " : "SamplerState ",
				             to_sampler_expression(arg.id));
		}

		if (&arg != &func.arguments.back())
			decl += ", ";

		// Hold a pointer to the parameter so we can invalidate the readonly field if needed.
		auto *var = maybe_get<SPIRVariable>(arg.id);
		if (var)
			var->parameter = &arg;
	}

	decl += ")";
	statement(decl);
}

void CompilerHLSL::emit_hlsl_entry_point()
{
	vector<string> arguments;

	if (require_input)
		arguments.push_back("SPIRV_Cross_Input stage_input");

	// Add I/O blocks as separate arguments with appropriate storage qualifier.
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			bool block = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)) != 0;

			if (var.storage != StorageClassInput && var.storage != StorageClassOutput)
				continue;

			if (block && !is_builtin_variable(var) && interface_variable_exists_in_entry_point(var.self))
			{
				if (var.storage == StorageClassInput)
				{
					arguments.push_back(join("in ", variable_decl(type, join("stage_input", to_name(var.self)))));
				}
				else if (var.storage == StorageClassOutput)
				{
					arguments.push_back(join("out ", variable_decl(type, join("stage_output", to_name(var.self)))));
				}
			}
		}
	}

	auto &execution = get_entry_point();
	statement(require_output ? "SPIRV_Cross_Output " : "void ", "main(", merge(arguments), ")");
	begin_scope();
	bool legacy = options.shader_model <= 30;

	// Copy builtins from entry point arguments to globals.
	for (uint32_t i = 0; i < 64; i++)
	{
		if (!(active_input_builtins & (1ull << i)))
			continue;

		auto builtin = builtin_to_glsl(static_cast<BuiltIn>(i));
		switch (static_cast<BuiltIn>(i))
		{
		case BuiltInFragCoord:
			// VPOS in D3D9 is sampled at integer locations, apply half-pixel offset to be consistent.
			// TODO: Do we need an option here? Any reason why a D3D9 shader would be used
			// on a D3D10+ system with a different rasterization config?
			if (legacy)
				statement(builtin, " = stage_input.", builtin, " + float4(0.5f, 0.5f, 0.0f, 0.0f);");
			else
				statement(builtin, " = stage_input.", builtin, ";");
			break;

		case BuiltInVertexIndex:
		case BuiltInInstanceIndex:
			// D3D semantics are uint, but shader wants int.
			statement(builtin, " = int(stage_input.", builtin, ");");
			break;

		default:
			statement(builtin, " = stage_input.", builtin, ";");
			break;
		}
	}

	// Copy from stage input struct to globals.
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			bool block = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)) != 0;

			if (var.storage != StorageClassInput)
				continue;

			if (!block && !var.remapped_variable && type.pointer && !is_builtin_variable(var) &&
			    interface_variable_exists_in_entry_point(var.self))
			{
				auto name = to_name(var.self);
				auto &mtype = get<SPIRType>(var.basetype);
				if (mtype.columns > 1)
				{
					// Unroll matrices.
					for (uint32_t col = 0; col < mtype.columns; col++)
						statement(name, "[", col, "] = stage_input.", name, "_", col, ";");
				}
				else
				{
					statement(name, " = stage_input.", name, ";");
				}
			}

			// I/O blocks don't use the common stage input/output struct, but separate outputs.
			if (block && !is_builtin_variable(var) && interface_variable_exists_in_entry_point(var.self))
			{
				auto name = to_name(var.self);
				statement(name, " = stage_input", name, ";");
			}
		}
	}

	// Run the shader.
	if (execution.model == ExecutionModelVertex)
		statement("vert_main();");
	else if (execution.model == ExecutionModelFragment)
		statement("frag_main();");
	else
		SPIRV_CROSS_THROW("Unsupported shader stage.");

	// Copy block outputs.
	for (auto &id : ids)
	{
		if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			auto &type = get<SPIRType>(var.basetype);
			bool block = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)) != 0;

			if (var.storage != StorageClassOutput)
				continue;

			// I/O blocks don't use the common stage input/output struct, but separate outputs.
			if (block && !is_builtin_variable(var) && interface_variable_exists_in_entry_point(var.self))
			{
				auto name = to_name(var.self);
				statement("stage_output", name, " = ", name, ";");
			}
		}
	}

	// Copy stage outputs.
	if (require_output)
	{
		statement("SPIRV_Cross_Output stage_output;");

		// Copy builtins from globals to return struct.
		for (uint32_t i = 0; i < 64; i++)
		{
			if (!(active_output_builtins & (1ull << i)))
				continue;

			// PointSize doesn't exist in HLSL.
			if (i == BuiltInPointSize)
				continue;

			auto builtin = builtin_to_glsl(static_cast<BuiltIn>(i));
			statement("stage_output.", builtin, " = ", builtin, ";");
		}

		for (auto &id : ids)
		{
			if (id.get_type() == TypeVariable)
			{
				auto &var = id.get<SPIRVariable>();
				auto &type = get<SPIRType>(var.basetype);
				bool block = (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)) != 0;

				if (var.storage != StorageClassOutput)
					continue;

				if (!block && var.storage != StorageClassFunction && !var.remapped_variable && type.pointer &&
				    !is_builtin_variable(var) && interface_variable_exists_in_entry_point(var.self))
				{
					auto name = to_name(var.self);
					statement("stage_output.", name, " = ", name, ";");
				}
			}
		}

		if (execution.model == ExecutionModelVertex)
		{
			// Do various mangling on the gl_Position.
			if (options.shader_model <= 30)
			{
				statement("stage_output.gl_Position.x = stage_output.gl_Position.x - gl_HalfPixel.x * "
				          "stage_output.gl_Position.w;");
				statement("stage_output.gl_Position.y = stage_output.gl_Position.y + gl_HalfPixel.y * "
				          "stage_output.gl_Position.w;");
			}
			if (options.flip_vert_y)
			{
				statement("stage_output.gl_Position.y = -stage_output.gl_Position.y;");
			}
			if (options.fixup_clipspace)
			{
				statement(
				    "stage_output.gl_Position.z = (stage_output.gl_Position.z + stage_output.gl_Position.w) * 0.5;");
			}
		}

		statement("return stage_output;");
	}

	end_scope();
}

void CompilerHLSL::emit_texture_op(const Instruction &i)
{
	auto ops = stream(i);
	auto op = static_cast<Op>(i.op);
	uint32_t length = i.length;

	if (i.offset + length > spirv.size())
		SPIRV_CROSS_THROW("Compiler::parse() opcode out of range.");

	uint32_t result_type = ops[0];
	uint32_t id = ops[1];
	uint32_t img = ops[2];
	uint32_t coord = ops[3];
	uint32_t dref = 0;
	uint32_t comp = 0;
	bool gather = false;
	bool proj = false;
	const uint32_t *opt = nullptr;
	auto *combined_image = maybe_get<SPIRCombinedImageSampler>(img);
	auto img_expr = to_expression(combined_image ? combined_image->image : img);

	switch (op)
	{
	case OpImageSampleDrefImplicitLod:
	case OpImageSampleDrefExplicitLod:
		dref = ops[4];
		opt = &ops[5];
		length -= 5;
		break;

	case OpImageSampleProjDrefImplicitLod:
	case OpImageSampleProjDrefExplicitLod:
		dref = ops[4];
		proj = true;
		opt = &ops[5];
		length -= 5;
		break;

	case OpImageDrefGather:
		dref = ops[4];
		opt = &ops[5];
		gather = true;
		length -= 5;
		break;

	case OpImageGather:
		comp = ops[4];
		opt = &ops[5];
		gather = true;
		length -= 5;
		break;

	case OpImageSampleProjImplicitLod:
	case OpImageSampleProjExplicitLod:
		opt = &ops[4];
		length -= 4;
		proj = true;
		break;

	default:
		opt = &ops[4];
		length -= 4;
		break;
	}

	auto &imgtype = expression_type(img);
	uint32_t coord_components = 0;
	switch (imgtype.image.dim)
	{
	case spv::Dim1D:
		coord_components = 1;
		break;
	case spv::Dim2D:
		coord_components = 2;
		break;
	case spv::Dim3D:
		coord_components = 3;
		break;
	case spv::DimCube:
		coord_components = 3;
		break;
	case spv::DimBuffer:
		coord_components = 1;
		break;
	default:
		coord_components = 2;
		break;
	}

	if (proj)
		coord_components++;
	if (imgtype.image.arrayed)
		coord_components++;

	uint32_t bias = 0;
	uint32_t lod = 0;
	uint32_t grad_x = 0;
	uint32_t grad_y = 0;
	uint32_t coffset = 0;
	uint32_t offset = 0;
	uint32_t coffsets = 0;
	uint32_t sample = 0;
	uint32_t flags = 0;

	if (length)
	{
		flags = opt[0];
		opt++;
		length--;
	}

	auto test = [&](uint32_t &v, uint32_t flag) {
		if (length && (flags & flag))
		{
			v = *opt++;
			length--;
		}
	};

	test(bias, ImageOperandsBiasMask);
	test(lod, ImageOperandsLodMask);
	test(grad_x, ImageOperandsGradMask);
	test(grad_y, ImageOperandsGradMask);
	test(coffset, ImageOperandsConstOffsetMask);
	test(offset, ImageOperandsOffsetMask);
	test(coffsets, ImageOperandsConstOffsetsMask);
	test(sample, ImageOperandsSampleMask);

	string expr;
	string texop;

	if (op == OpImageFetch)
	{
		if (options.shader_model < 40)
		{
			SPIRV_CROSS_THROW("texelFetch is not supported in HLSL shader model 2/3.");
		}
		texop += img_expr;
		texop += ".Load";
	}
	else
	{
		auto &imgformat = get<SPIRType>(imgtype.image.type);
		if (imgformat.basetype != SPIRType::Float)
		{
			SPIRV_CROSS_THROW("Sampling non-float textures is not supported in HLSL.");
		}

		if (options.shader_model >= 40)
		{
			texop += img_expr;

			if (imgtype.image.depth)
				texop += ".SampleCmp";
			else if (gather)
				texop += ".Gather";
			else if (bias)
				texop += ".SampleBias";
			else if (grad_x || grad_y)
				texop += ".SampleGrad";
			else if (lod)
				texop += ".SampleLevel";
			else
				texop += ".Sample";
		}
		else
		{
			switch (imgtype.image.dim)
			{
			case Dim1D:
				texop += "tex1D";
				break;
			case Dim2D:
				texop += "tex2D";
				break;
			case Dim3D:
				texop += "tex3D";
				break;
			case DimCube:
				texop += "texCUBE";
				break;
			case DimRect:
			case DimBuffer:
			case DimSubpassData:
				SPIRV_CROSS_THROW("Buffer texture support is not yet implemented for HLSL"); // TODO
			default:
				SPIRV_CROSS_THROW("Invalid dimension.");
			}

			if (gather)
				SPIRV_CROSS_THROW("textureGather is not supported in HLSL shader model 2/3.");
			if (offset || coffset)
				SPIRV_CROSS_THROW("textureOffset is not supported in HLSL shader model 2/3.");
			if (proj)
				texop += "proj";
			if (grad_x || grad_y)
				texop += "grad";
			if (lod)
				texop += "lod";
			if (bias)
				texop += "bias";
		}
	}

	expr += texop;
	expr += "(";
	if (options.shader_model < 40)
	{
		if (combined_image)
			SPIRV_CROSS_THROW("Separate images/samplers are not supported in HLSL shader model 2/3.");
		expr += to_expression(img);
	}
	else if (op != OpImageFetch)
	{
		string sampler_expr;
		if (combined_image)
			sampler_expr = to_expression(combined_image->sampler);
		else
			sampler_expr = to_sampler_expression(img);
		expr += sampler_expr;
	}

	auto swizzle = [](uint32_t comps, uint32_t in_comps) -> const char * {
		if (comps == in_comps)
			return "";

		switch (comps)
		{
		case 1:
			return ".x";
		case 2:
			return ".xy";
		case 3:
			return ".xyz";
		default:
			return "";
		}
	};

	bool forward = should_forward(coord);

	// The IR can give us more components than we need, so chop them off as needed.
	auto coord_expr = to_expression(coord) + swizzle(coord_components, expression_type(coord).vecsize);

	if (proj)
	{
		if (!requires_textureProj)
		{
			requires_textureProj = true;
			force_recompile = true;
		}
		coord_expr = "SPIRV_Cross_projectTextureCoordinate(" + coord_expr + ")";
	}

	if (options.shader_model < 40 && lod)
	{
		auto &coordtype = expression_type(coord);
		string coord_filler;
		for (uint32_t size = coordtype.vecsize; size < 3; ++size)
		{
			coord_filler += ", 0.0";
		}
		coord_expr = "float4(" + coord_expr + coord_filler + ", " + to_expression(lod) + ")";
	}

	if (options.shader_model < 40 && bias)
	{
		auto &coordtype = expression_type(coord);
		string coord_filler;
		for (uint32_t size = coordtype.vecsize; size < 3; ++size)
		{
			coord_filler += ", 0.0";
		}
		coord_expr = "float4(" + coord_expr + coord_filler + ", " + to_expression(bias) + ")";
	}

	if (op == OpImageFetch)
	{
		auto &coordtype = expression_type(coord);
		coord_expr = join("int", coordtype.vecsize + 1, "(", coord_expr, ", ", to_expression(lod), ")");
	}

	if (op != OpImageFetch)
	{
		expr += ", ";
	}
	expr += coord_expr;

	if (dref)
	{
		forward = forward && should_forward(dref);
		expr += ", ";
		expr += to_expression(dref);
	}

	if (grad_x || grad_y)
	{
		forward = forward && should_forward(grad_x);
		forward = forward && should_forward(grad_y);
		expr += ", ";
		expr += to_expression(grad_x);
		expr += ", ";
		expr += to_expression(grad_y);
	}

	if (lod && options.shader_model >= 40 && op != OpImageFetch)
	{
		forward = forward && should_forward(lod);
		expr += ", ";
		expr += to_expression(lod);
	}

	if (bias && options.shader_model >= 40)
	{
		forward = forward && should_forward(bias);
		expr += ", ";
		expr += to_expression(bias);
	}

	if (coffset)
	{
		forward = forward && should_forward(coffset);
		expr += ", ";
		expr += to_expression(coffset);
	}
	else if (offset)
	{
		forward = forward && should_forward(offset);
		expr += ", ";
		expr += to_expression(offset);
	}

	if (comp)
	{
		forward = forward && should_forward(comp);
		expr += ", ";
		expr += to_expression(comp);
	}

	if (sample)
	{
		expr += ", ";
		expr += to_expression(sample);
	}

	expr += ")";

	emit_op(result_type, id, expr, forward, false);
}

string CompilerHLSL::to_resource_binding(const SPIRVariable &var)
{
	// TODO: Basic implementation, might need special consideration for RW/RO structured buffers,
	// RW/RO images, and so on.

	if (!has_decoration(var.self, DecorationBinding))
		return "";

	auto &type = get<SPIRType>(var.basetype);
	const char *space = nullptr;

	switch (type.basetype)
	{
	case SPIRType::SampledImage:
	case SPIRType::Image:
		space = "t"; // SRV
		break;

	case SPIRType::Sampler:
		space = "s";
		break;

	case SPIRType::Struct:
	{
		auto storage = type.storage;
		if (storage == StorageClassUniform)
		{
			if (has_decoration(type.self, DecorationBufferBlock))
				space = "u"; // UAV
			else if (has_decoration(type.self, DecorationBlock))
			{
				if (options.shader_model >= 51)
					space = "b"; // Constant buffers
				else
					space = "c"; // Constant buffers
			}
		}
		else if (storage == StorageClassPushConstant)
		{
			if (options.shader_model >= 51)
				space = "b"; // Constant buffers
			else
				space = "c"; // Constant buffers
		}

		break;
	}
	default:
		break;
	}

	if (!space)
		return "";

	return join(" : register(", space, get_decoration(var.self, DecorationBinding), ")");
}

string CompilerHLSL::to_resource_binding_sampler(const SPIRVariable &var)
{
	// For combined image samplers.
	if (!has_decoration(var.self, DecorationBinding))
		return "";
	return join(" : register(s", get_decoration(var.self, DecorationBinding), ")");
}

void CompilerHLSL::emit_modern_uniform(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	switch (type.basetype)
	{
	case SPIRType::SampledImage:
	case SPIRType::Image:
	{
		statement(image_type_hlsl_modern(type), " ", to_name(var.self), to_resource_binding(var), ";");

		if (type.basetype == SPIRType::SampledImage)
		{
			// For combined image samplers, also emit a combined image sampler.
			if (type.image.depth)
				statement("SamplerComparisonState ", to_sampler_expression(var.self), to_resource_binding_sampler(var),
				          ";");
			else
				statement("SamplerState ", to_sampler_expression(var.self), to_resource_binding_sampler(var), ";");
		}
		break;
	}

	case SPIRType::Sampler:
		if (comparison_samplers.count(var.self))
			statement("SamplerComparisonState ", to_name(var.self), to_resource_binding(var), ";");
		else
			statement("SamplerState ", to_name(var.self), to_resource_binding(var), ";");
		break;

	default:
		statement(variable_decl(var), to_resource_binding(var), ";");
		break;
	}
}

void CompilerHLSL::emit_legacy_uniform(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	switch (type.basetype)
	{
	case SPIRType::Sampler:
	case SPIRType::Image:
		SPIRV_CROSS_THROW("Separate image and samplers not supported in legacy HLSL.");

	default:
		statement(variable_decl(var), ";");
		break;
	}
}

void CompilerHLSL::emit_uniform(const SPIRVariable &var)
{
	add_resource_name(var.self);
	if (options.shader_model >= 40)
		emit_modern_uniform(var);
	else
		emit_legacy_uniform(var);
}

string CompilerHLSL::bitcast_glsl_op(const SPIRType &out_type, const SPIRType &in_type)
{
	if (out_type.basetype == SPIRType::UInt && in_type.basetype == SPIRType::Int)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::UInt64 && in_type.basetype == SPIRType::Int64)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::UInt && in_type.basetype == SPIRType::Float)
		return "asuint";
	else if (out_type.basetype == SPIRType::Int && in_type.basetype == SPIRType::UInt)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::Int64 && in_type.basetype == SPIRType::UInt64)
		return type_to_glsl(out_type);
	else if (out_type.basetype == SPIRType::Int && in_type.basetype == SPIRType::Float)
		return "asint";
	else if (out_type.basetype == SPIRType::Float && in_type.basetype == SPIRType::UInt)
		return "asfloat";
	else if (out_type.basetype == SPIRType::Float && in_type.basetype == SPIRType::Int)
		return "asfloat";
	else if (out_type.basetype == SPIRType::Int64 && in_type.basetype == SPIRType::Double)
		SPIRV_CROSS_THROW("Double to Int64 is not supported in HLSL.");
	else if (out_type.basetype == SPIRType::UInt64 && in_type.basetype == SPIRType::Double)
		SPIRV_CROSS_THROW("Double to UInt64 is not supported in HLSL.");
	else if (out_type.basetype == SPIRType::Double && in_type.basetype == SPIRType::Int64)
		return "asdouble";
	else if (out_type.basetype == SPIRType::Double && in_type.basetype == SPIRType::UInt64)
		return "asdouble";
	else
		return "";
}

void CompilerHLSL::emit_glsl_op(uint32_t result_type, uint32_t id, uint32_t eop, const uint32_t *args, uint32_t count)
{
	GLSLstd450 op = static_cast<GLSLstd450>(eop);

	switch (op)
	{
	case GLSLstd450InverseSqrt:
		emit_unary_func_op(result_type, id, args[0], "rsqrt");
		break;

	case GLSLstd450Fract:
		emit_unary_func_op(result_type, id, args[0], "frac");
		break;

	case GLSLstd450FMix:
	case GLSLstd450IMix:
		emit_trinary_func_op(result_type, id, args[0], args[1], args[2], "lerp");
		break;

	case GLSLstd450Atan2:
		emit_binary_func_op(result_type, id, args[1], args[0], "atan2");
		break;

	case GLSLstd450Fma:
		emit_trinary_func_op(result_type, id, args[0], args[1], args[2], "mad");
		break;

	case GLSLstd450InterpolateAtCentroid:
		emit_unary_func_op(result_type, id, args[0], "EvaluateAttributeAtCentroid");
		break;
	case GLSLstd450InterpolateAtSample:
		emit_binary_func_op(result_type, id, args[0], args[1], "EvaluateAttributeAtSample");
		break;
	case GLSLstd450InterpolateAtOffset:
		emit_binary_func_op(result_type, id, args[0], args[1], "EvaluateAttributeSnapped");
		break;

	default:
		CompilerGLSL::emit_glsl_op(result_type, id, eop, args, count);
		break;
	}
}

void CompilerHLSL::emit_instruction(const Instruction &instruction)
{
	auto ops = stream(instruction);
	auto opcode = static_cast<Op>(instruction.op);

#define BOP(op) emit_binary_op(ops[0], ops[1], ops[2], ops[3], #op)
#define BOP_CAST(op, type) \
	emit_binary_op_cast(ops[0], ops[1], ops[2], ops[3], #op, type, opcode_is_sign_invariant(opcode))
#define UOP(op) emit_unary_op(ops[0], ops[1], ops[2], #op)
#define QFOP(op) emit_quaternary_func_op(ops[0], ops[1], ops[2], ops[3], ops[4], ops[5], #op)
#define TFOP(op) emit_trinary_func_op(ops[0], ops[1], ops[2], ops[3], ops[4], #op)
#define BFOP(op) emit_binary_func_op(ops[0], ops[1], ops[2], ops[3], #op)
#define BFOP_CAST(op, type) \
	emit_binary_func_op_cast(ops[0], ops[1], ops[2], ops[3], #op, type, opcode_is_sign_invariant(opcode))
#define BFOP(op) emit_binary_func_op(ops[0], ops[1], ops[2], ops[3], #op)
#define UFOP(op) emit_unary_func_op(ops[0], ops[1], ops[2], #op)

	switch (opcode)
	{
	case OpMatrixTimesVector:
	{
		emit_binary_func_op(ops[0], ops[1], ops[3], ops[2], "mul");
		break;
	}

	case OpVectorTimesMatrix:
	{
		emit_binary_func_op(ops[0], ops[1], ops[3], ops[2], "mul");
		break;
	}

	case OpMatrixTimesMatrix:
	{
		emit_binary_func_op(ops[0], ops[1], ops[3], ops[2], "mul");
		break;
	}

	case OpFMod:
	{
		if (!requires_op_fmod)
		{
			requires_op_fmod = true;
			force_recompile = true;
		}
		CompilerGLSL::emit_instruction(instruction);
		break;
	}

	case OpImage:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		emit_op(result_type, id, to_expression(ops[2]), true, true);
		// TODO: Maybe change this when separate samplers/images are supported
		break;
	}

	case OpDPdx:
		UFOP(ddx);
		break;

	case OpDPdy:
		UFOP(ddy);
		break;

	case OpDPdxFine:
		UFOP(ddx_fine);
		break;

	case OpDPdyFine:
		UFOP(ddy_fine);
		break;

	case OpDPdxCoarse:
		UFOP(ddx_coarse);
		break;

	case OpDPdyCoarse:
		UFOP(ddy_coarse);
		break;

	case OpLogicalNot:
	{
		auto result_type = ops[0];
		auto id = ops[1];
		auto &type = get<SPIRType>(result_type);

		if (type.vecsize > 1)
			emit_unrolled_unary_op(result_type, id, ops[2], "!");
		else
			UOP(!);
		break;
	}

	case OpIEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "==");
		else
			BOP_CAST(==, SPIRType::Int);
		break;
	}

	case OpLogicalEqual:
	case OpFOrdEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "==");
		else
			BOP(==);
		break;
	}

	case OpINotEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "!=");
		else
			BOP_CAST(!=, SPIRType::Int);
		break;
	}

	case OpLogicalNotEqual:
	case OpFOrdNotEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "!=");
		else
			BOP(!=);
		break;
	}

	case OpUGreaterThan:
	case OpSGreaterThan:
	{
		auto result_type = ops[0];
		auto id = ops[1];
		auto type = opcode == OpUGreaterThan ? SPIRType::UInt : SPIRType::Int;

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], ">");
		else
			BOP_CAST(>, type);
		break;
	}

	case OpFOrdGreaterThan:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], ">");
		else
			BOP(>);
		break;
	}

	case OpUGreaterThanEqual:
	case OpSGreaterThanEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		auto type = opcode == OpUGreaterThanEqual ? SPIRType::UInt : SPIRType::Int;
		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], ">=");
		else
			BOP_CAST(>=, type);
		break;
	}

	case OpFOrdGreaterThanEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], ">=");
		else
			BOP(>=);
		break;
	}

	case OpULessThan:
	case OpSLessThan:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		auto type = opcode == OpULessThan ? SPIRType::UInt : SPIRType::Int;
		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "<");
		else
			BOP_CAST(<, type);
		break;
	}

	case OpFOrdLessThan:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "<");
		else
			BOP(<);
		break;
	}

	case OpULessThanEqual:
	case OpSLessThanEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		auto type = opcode == OpULessThanEqual ? SPIRType::UInt : SPIRType::Int;
		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "<=");
		else
			BOP_CAST(<=, type);
		break;
	}

	case OpFOrdLessThanEqual:
	{
		auto result_type = ops[0];
		auto id = ops[1];

		if (expression_type(ops[2]).vecsize > 1)
			emit_unrolled_binary_op(result_type, id, ops[2], ops[3], "<=");
		else
			BOP(<=);
		break;
	}

	default:
		CompilerGLSL::emit_instruction(instruction);
		break;
	}
}

string CompilerHLSL::compile()
{
	// Do not deal with ES-isms like precision, older extensions and such.
	CompilerGLSL::options.es = false;
	CompilerGLSL::options.version = 450;
	CompilerGLSL::options.vulkan_semantics = true;
	backend.float_literal_suffix = true;
	backend.double_literal_suffix = false;
	backend.long_long_literal_suffix = true;
	backend.uint32_t_literal_suffix = true;
	backend.basic_int_type = "int";
	backend.basic_uint_type = "uint";
	backend.swizzle_is_function = false;
	backend.shared_is_implied = true;
	backend.flexible_member_array_supported = false;
	backend.explicit_struct_type = false;
	backend.use_initializer_list = true;
	backend.use_constructor_splatting = false;
	backend.boolean_mix_support = false;

	update_active_builtins();
	analyze_sampler_comparison_states();

	uint32_t pass_count = 0;
	do
	{
		if (pass_count >= 3)
			SPIRV_CROSS_THROW("Over 3 compilation loops detected. Must be a bug!");

		reset();

		// Move constructor for this type is broken on GCC 4.9 ...
		buffer = unique_ptr<ostringstream>(new ostringstream());

		emit_header();
		emit_resources();

		emit_function(get<SPIRFunction>(entry_point), 0);
		emit_hlsl_entry_point();

		pass_count++;
	} while (force_recompile);

	return buffer->str();
}
