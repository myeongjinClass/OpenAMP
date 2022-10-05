//--------------------------------------------------------------------------
// 
//  Copyright (c) Microsoft Corporation.  All rights reserved. 
// 
//  File: Morph_lib.cpp
//
//--------------------------------------------------------------------------

#include "stdafx.h"
#include <assert.h>
#include <iostream>
using namespace concurrency;
using namespace concurrency::graphics;
using namespace concurrency::fast_math;

namespace {

struct line
{
	float_2 start, end;
	static_assert(sizeof(float_2) == 2 * sizeof(float), "Requiring unpadded type!");
};

struct line_pair
{
	line a, b;
};

float dot(const float_2& a, const float_2& b) restrict(cpu,amp)
{
	return a.x * b.x + a.y * b.y;
}

float length_sq(const float_2& a) restrict(cpu,amp)
{
	return dot(a, a);
}

float length(const float_2& a) restrict(cpu,amp)
{
	return sqrtf(length_sq(a));
}

float_2 flip_perpendicular(const float_2& a) restrict(cpu,amp)
{
	return float_2(-a.y, a.x);
}

unorm_4 mul(const unorm_4& v, float s) restrict(cpu,amp)
{
	return unorm_4(v.x * s, v.y * s, v.z * s, v.w * s);
}

class compute_morph
{
public:
	compute_morph(const float* line_pairs_ptr, unsigned line_pairs_num, float const_a, float const_b, float const_p)
		: const_a(const_a)
		, const_b(const_b)
		, const_p(const_p)
		, line_pairs(line_pairs_num)
		, forwards_line_pairs(line_pairs_num)
		, backwards_line_pairs(line_pairs_num)
	{
		memcpy(line_pairs.data(), line_pairs_ptr, line_pairs_num * sizeof(line_pair));
	}

	virtual ~compute_morph() {}

	virtual void compute(char* output_ptr, float percentage) = 0;

protected:
	void interpolate_lines(float percentage);
	static float_2 get_preimage_location(const index<2>& idx, const line_pair* line_pairs, unsigned line_pairs_num, float const_a, float const_b, float const_p) restrict(cpu,amp);
	static index<2> clamp_point(const float_2& p, const extent<2>& ext) restrict(cpu,amp);
	
	float const_a;
	float const_b;
	float const_p;
	std::vector<line_pair> forwards_line_pairs;
	std::vector<line_pair> backwards_line_pairs;

private:
	std::vector<line_pair> line_pairs;
};

void compute_morph::interpolate_lines(float percentage)
{
	for(size_t i = 0; i < line_pairs.size(); i++)
	{
		const line_pair& pair = line_pairs[i];

        // Source line is the same as the original; dest line is the interpolated line
		// Add the new pair to the forwards list and the inverse to the backwards list.
		forwards_line_pairs[i].a = pair.a;
		forwards_line_pairs[i].b.start = pair.a.start + (pair.b.start - pair.a.start) * percentage;
		forwards_line_pairs[i].b.end = pair.a.end + (pair.b.end - pair.a.end) * percentage;
		backwards_line_pairs[i].a = pair.b;
		backwards_line_pairs[i].b = forwards_line_pairs[i].b;
	}
}

float_2 compute_morph::get_preimage_location(const index<2>& idx, const line_pair* line_pairs, unsigned line_pairs_num, float const_a, float const_b, float const_p) restrict(cpu,amp)
{
	float_2 p(static_cast<float>(idx[1]), static_cast<float>(idx[0]));

	if(line_pairs_num == 0)
	{
		return p;
	}

	float_2 sum(0, 0);
	float weight_sum = 0;

	for(unsigned i = 0; i < line_pairs_num; i++)
	{
		const line_pair& pair = line_pairs[i];
		const float_2& src_start = pair.a.start;
		const float_2& src_end = pair.a.end;
		const float_2& dst_start = pair.b.start;
		const float_2& dst_end = pair.b.end;
		
		if(src_start == src_end
			|| dst_start == dst_end)
		{
			continue;
		}

		float_2 PQ = dst_end - dst_start;
		float PQ_length_sq = length_sq(PQ);
		float PQ_length = sqrtf(PQ_length_sq);
		float_2 PpQp = src_end - src_start;
		float_2 PX = p - dst_start;

		float u = dot(PX, PQ) / PQ_length_sq;
		float v = dot(PX, flip_perpendicular(PQ)) / PQ_length;

		float_2 Xp = src_start + PpQp * u + flip_perpendicular(PpQp) * v / length(PpQp);

		// Compute shortest distance from X to the line segment PQ
		float dist;
		if(u < 0)
		{
			dist = length(p - dst_start);
		}
		else if(u > 1)
		{
			dist = length(p - dst_end);
		}
		else
		{
			dist = fabsf(v);
		}

		float strength = powf(PQ_length, const_p) / (const_a + dist);
		float weight = powf(strength, const_b);
		sum += (Xp - p) * weight;
		weight_sum += weight;
	}

	return p + sum / weight_sum;
}

index<2> compute_morph::clamp_point(const float_2& p, const extent<2>& ext) restrict(cpu,amp)
{
	int x = static_cast<int>(p.x);
	int y = static_cast<int>(p.y);

	if(x < 0)
	{
		x = 0;
	}
	else if(x >= ext[1])
	{
		x = ext[1] - 1;
	}

	if(y < 0)
	{
		y = 0;
	}
	else if(y >= ext[0])
	{
		y = ext[0] - 1;
	}

	return index<2>(y, x);
}

class compute_morph_amp : public compute_morph
{
public:
	compute_morph_amp(const accelerator_view& av, const char* start_image_ptr, const char* end_image_ptr, int width, int height,
		float* line_pairs_ptr, unsigned line_pairs_num, float const_a, float const_b, float const_p)
		: compute_morph(line_pairs_ptr, line_pairs_num, const_a, const_b, const_p)
		, start(height, width, const_cast<char*>(start_image_ptr), height * width * 4U, 8U, av)
		, end(height, width, const_cast<char*>(end_image_ptr), height * width * 4U, 8U, av)
		, start_intermediate(height, width, 8U, av)
		, end_intermediate(height, width, 8U, av)
		, result(height, width, 8U, av)
	{
	}

	void compute(char* output_ptr, float percentage) override;

private:
	struct morph_params
	{
		static const size_t max_line_pairs = 128;
		line_pair line_pairs[max_line_pairs];
		unsigned line_pairs_num;
		float const_a;
		float const_b;
		float const_p;
	};

	void compute_preimage(const texture<unorm_4, 2>& source, texture<unorm_4, 2>& dest, const std::vector<line_pair>& line_pairs);
	void blend_images(float blend_factor);
	void operator=(compute_morph_amp&);

	const texture<unorm_4, 2> start;
	const texture<unorm_4, 2> end;
	texture<unorm_4, 2> start_intermediate;
	texture<unorm_4, 2> end_intermediate;
	texture<unorm_4, 2> result;
};

void compute_morph_amp::compute(char* output_ptr, float percentage)
{
	interpolate_lines(percentage);
	compute_preimage(start, start_intermediate, forwards_line_pairs);
	compute_preimage(end, end_intermediate, backwards_line_pairs);
	blend_images(1 - percentage);
	copy(result, output_ptr, result.extent.size() * 4U);
}

void compute_morph_amp::compute_preimage(const texture<unorm_4, 2>& source, texture<unorm_4, 2>& dest, const std::vector<line_pair>& line_pairs)
{
	if(line_pairs.size() > morph_params::max_line_pairs)
	{
		throw std::runtime_error("Max number of line pairs exceeded!");
	}

	morph_params params;
	memcpy(params.line_pairs, line_pairs.data(), line_pairs.size() * sizeof(line_pair));
	params.line_pairs_num = line_pairs.size();
	params.const_a = const_a;
	params.const_b = const_b;
	params.const_p = const_p;

	writeonly_texture_view<unorm_4, 2> output(dest);

	parallel_for_each(output.extent, [=, &source](index<2> idx) restrict(amp)
	{
		float_2 pf = get_preimage_location(idx, params.line_pairs, params.line_pairs_num, params.const_a, params.const_b, params.const_p);
		index<2> p = clamp_point(pf, output.extent);
		output.set(idx, source[p]);
	});
}

void compute_morph_amp::blend_images(float blend_factor)
{
	auto& start = start_intermediate;
	auto& end = end_intermediate;
	writeonly_texture_view<unorm_4, 2> output(result);

	parallel_for_each(output.extent, [=, &start, &end](index<2> idx) restrict(amp)
	{
		output.set(idx, mul(start[idx], blend_factor) + mul(end[idx], 1 - blend_factor));
	});
}

class compute_morph_cpp : public compute_morph
{
public:
	compute_morph_cpp(bool use_ppl, const char* start_image_ptr, const char* end_image_ptr, int width, int height,
		float* line_pairs_ptr, unsigned line_pairs_num, float const_a, float const_b, float const_p)
		: compute_morph(line_pairs_ptr, line_pairs_num, const_a, const_b, const_p)
		, use_ppl(use_ppl)
		, size(height, width)
		, start(new unsigned char[size.size() * 4])
		, end(new unsigned char[size.size() * 4])
		, start_intermediate(new unsigned char[size.size() * 4])
		, end_intermediate(new unsigned char[size.size() * 4])
	{
		memcpy(start.get(), start_image_ptr, size.size() * 4);
		memcpy(end.get(), end_image_ptr, size.size() * 4);
	}

	void compute(char* output_ptr, float percentage) override;

private:
	int linearize(const index<2>& idx) const;
	unsigned char saturate(float v) const;
	void compute_preimage(const unsigned char* source, unsigned char* dest, const std::vector<line_pair>& line_pairs);
	void blend_images(unsigned char* output_ptr, float blend_factor);
	void operator=(compute_morph_cpp&);

	bool use_ppl;
	extent<2> size;
	std::unique_ptr<unsigned char[]> start;
	std::unique_ptr<unsigned char[]> end;
	std::unique_ptr<unsigned char[]> start_intermediate;
	std::unique_ptr<unsigned char[]> end_intermediate;
};

void compute_morph_cpp::compute(char* output_ptr, float percentage)
{
	interpolate_lines(percentage);
	compute_preimage(start.get(), start_intermediate.get(), forwards_line_pairs);
	compute_preimage(end.get(), end_intermediate.get(), backwards_line_pairs);
	blend_images(reinterpret_cast<unsigned char*>(output_ptr), 1 - percentage);
}

int compute_morph_cpp::linearize(const index<2>& idx) const
{
	return (idx[0] * size[1] + idx[1]) * 4;
}

unsigned char compute_morph_cpp::saturate(float v) const
{
	if(v > UCHAR_MAX)
		return UCHAR_MAX;
	else if(v < 0)
		return 0;
	else
		return static_cast<unsigned char>(v);
}

void compute_morph_cpp::compute_preimage(const unsigned char* source, unsigned char* dest, const std::vector<line_pair>& line_pairs)
{
	if(!use_ppl)
	{
		for(int y = 0; y < size[0]; y++)
		{
			for(int x = 0; x < size[1]; x++)
			{
				index<2> idx(y, x);
				float_2 pf = get_preimage_location(idx, line_pairs.data(), line_pairs.size(), const_a, const_b, const_p);
				index<2> p = clamp_point(pf, size);
				int offset_idx = linearize(idx);
				int offset_p = linearize(p);
				dest[offset_idx + 0] = source[offset_p + 0];
				dest[offset_idx + 1] = source[offset_p + 1];
				dest[offset_idx + 2] = source[offset_p + 2];
				dest[offset_idx + 3] = source[offset_p + 3];
			}
		}
	}
	else
	{
		parallel_for(0, size[0], 1, [=](int y)
		{
			for(int x = 0; x < size[1]; x++)
			{
				index<2> idx(y, x);
				float_2 pf = get_preimage_location(idx, line_pairs.data(), line_pairs.size(), const_a, const_b, const_p);
				index<2> p = clamp_point(pf, size);
				int offset_idx = linearize(idx);
				int offset_p = linearize(p);
				dest[offset_idx + 0] = source[offset_p + 0];
				dest[offset_idx + 1] = source[offset_p + 1];
				dest[offset_idx + 2] = source[offset_p + 2];
				dest[offset_idx + 3] = source[offset_p + 3];
			}
		});
	}
}

void compute_morph_cpp::blend_images(unsigned char* output_ptr, float blend_factor)
{
	if(!use_ppl)
	{
		for(int y = 0; y < size[0]; y++)
		{
			for(int x = 0; x < size[1]; x++)
			{
				int offset = linearize(index<2>(y, x));
				output_ptr[offset + 0] = saturate(static_cast<float>(start_intermediate[offset + 0]) * blend_factor + static_cast<float>(end_intermediate[offset + 0]) * (1 - blend_factor));
				output_ptr[offset + 1] = saturate(static_cast<float>(start_intermediate[offset + 1]) * blend_factor + static_cast<float>(end_intermediate[offset + 1]) * (1 - blend_factor));
				output_ptr[offset + 2] = saturate(static_cast<float>(start_intermediate[offset + 2]) * blend_factor + static_cast<float>(end_intermediate[offset + 2]) * (1 - blend_factor));
				output_ptr[offset + 3] = saturate(static_cast<float>(start_intermediate[offset + 3]) * blend_factor + static_cast<float>(end_intermediate[offset + 3]) * (1 - blend_factor));
			}
		}
	}
	else
	{
		parallel_for(0, size[0], 1, [=](int y)
		{
			for(int x = 0; x < size[1]; x++)
			{
				int offset = linearize(index<2>(y, x));
				output_ptr[offset + 0] = saturate(static_cast<float>(start_intermediate[offset + 0]) * blend_factor + static_cast<float>(end_intermediate[offset + 0]) * (1 - blend_factor));
				output_ptr[offset + 1] = saturate(static_cast<float>(start_intermediate[offset + 1]) * blend_factor + static_cast<float>(end_intermediate[offset + 1]) * (1 - blend_factor));
				output_ptr[offset + 2] = saturate(static_cast<float>(start_intermediate[offset + 2]) * blend_factor + static_cast<float>(end_intermediate[offset + 2]) * (1 - blend_factor));
				output_ptr[offset + 3] = saturate(static_cast<float>(start_intermediate[offset + 3]) * blend_factor + static_cast<float>(end_intermediate[offset + 3]) * (1 - blend_factor));
			}
		});
	}
}

enum class Mode : int
{
	sequential = 0,
	ppl = 1,
	amp = 2
};

compute_morph* g_compute_morph_ptr;

} // namespace

// Begins enumerating available accelerators.
// Returns number of accelerators.
extern "C" __declspec(dllexport) unsigned _stdcall BeginEnumAccelerators(std::vector<accelerator>** handle_ptr)
{
	auto accs =  new std::vector<accelerator>(accelerator::get_all());
	accs->erase(std::find_if(accs->begin(), accs->end(), [](const accelerator& acc) { return acc.device_path == accelerator::cpu_accelerator; }));
	*handle_ptr = accs;
	return accs->size();
}

// Iterates over accelerator listing.
extern "C" __declspec(dllexport) bool _stdcall EnumAccelerator(std::vector<accelerator>* handle, unsigned index, wchar_t* description_buffer, unsigned description_size, wchar_t* device_path_buffer, unsigned device_path_size)
{
	if(!handle)
	{
		std::cout << "Null accelerator collection handle." << std::endl;
		return false;
	}

	if(index > handle->size())
	{
		std::cout << "Enumerating accelerator out of bounds." << std::endl;
		return false;
	}

	auto& acc = (*handle)[index];
	auto description = acc.description;
	auto device_path = acc.device_path;

	if(description_size == 0)
	{
		std::cout << "Description buffer too small." << std::endl;
		return false;
	}
	if(description.size() + 1 > description_size)
	{
		description = description.substr(0, description_size - 1);
		auto it = description.rbegin();
		for(int i = 0; it < description.rend() && i < 3; ++it, ++i)
		{
			*it = L'.';
		}
	}

	if(device_path.size() + 1 > device_path_size)
	{
		std::cout << "Device path buffer too small." << std::endl;
		return false;
	}

	std::copy(description.begin(), description.end(), stdext::checked_array_iterator<wchar_t*>(description_buffer, description_size));
	description_buffer[description.size()] = L'\0';

	std::copy(device_path.begin(), device_path.end(), stdext::checked_array_iterator<wchar_t*>(device_path_buffer, device_path_size));
	device_path_buffer[device_path.size()] = L'\0';

	return true;
}

// Ends enumerating available accelerators, cleans up data structures.
extern "C" __declspec(dllexport) void _stdcall EndEnumAccelerators(std::vector<accelerator>* handle)
{
	delete handle;
}

// Initializes the morph algorithm with provided parameters.
extern "C" __declspec(dllexport) bool _stdcall InitializeMorph(Mode mode, const wchar_t* amp_device_path,
															   char* start_image_ptr, unsigned start_image_row_stride, char* end_image_ptr, unsigned end_image_row_stride,
															   int width, int height, float* line_pairs_ptr, unsigned line_pairs_num, double const_a, double const_b, double const_p)
{
	try
	{
		assert(start_image_row_stride == width * 4U);
		assert(end_image_row_stride == width * 4U);
		assert(!g_compute_morph_ptr);

		if(mode == Mode::amp)
		{
			accelerator_view av = accelerator(amp_device_path).create_view();
			g_compute_morph_ptr = new compute_morph_amp(av, start_image_ptr, end_image_ptr, width, height,
				line_pairs_ptr, line_pairs_num, static_cast<float>(const_a), static_cast<float>(const_b), static_cast<float>(const_p));
		}
		else
		{
			bool use_ppl = mode == Mode::ppl;

			g_compute_morph_ptr = new compute_morph_cpp(use_ppl, start_image_ptr, end_image_ptr, width, height,
				line_pairs_ptr, line_pairs_num, static_cast<float>(const_a), static_cast<float>(const_b), static_cast<float>(const_p));
		}

		return g_compute_morph_ptr != nullptr;
	}
	catch(const std::exception& ex)
	{
		std::cout << "Morph_lib failed to initialize: " << typeid(ex).name() << " : " << ex.what() << std::endl;
		return false;
	}
	catch(...)
	{
		std::cout << "Morph_lib failed to initialize." << std::endl;
		return false;
	}
}

// Shuts down the morph algorithm, releasing all data.
extern "C" __declspec(dllexport) void _stdcall ShutdownMorph()
{
	delete g_compute_morph_ptr;
	g_compute_morph_ptr = nullptr;
}

// Computes a step of the morph algorithm.
extern "C" __declspec(dllexport) bool _stdcall ComputeMorphStep(char* output_ptr, double percentage_dbl)
{
	try
	{
		assert(g_compute_morph_ptr != nullptr);
		g_compute_morph_ptr->compute(output_ptr, static_cast<float>(percentage_dbl));
		return true;
	}
	catch(const std::exception& ex)
	{
		std::cout << "Morph_lib failed to compute morph step: " << typeid(ex).name() << " : " << ex.what() << std::endl;
		return false;
	}
	catch(...)
	{
		std::cout << "Morph_lib failed to compute morph step." << std::endl;
		return false;
	}
}