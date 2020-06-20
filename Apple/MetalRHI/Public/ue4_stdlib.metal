// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <metal_stdlib>
using namespace metal;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

#pragma mark -- Common Functions --

#if __METAL_VERSION__ >= 120 && __METAL_DEVICE_CONSTANT_INDEX__
constant uint GMetalDeviceManufacturer [[ function_constant(33) ]];
#endif

#if __METAL_VERSION__ >= 210
#define POS_INVARIANT , invariant
#else
#define POS_INVARIANT
#endif

namespace ue4
{
	enum manufacturer
	{
		Undefined = 0,
		Apple_Imagination = 1,
		Apple_Internal = 2,
		AMD = 0x1002,
		Intel = 0x8086,
		Nvidia = 0x10DE,
	};
	
#if __METAL_VERSION__ < 120
    static inline __attribute__((always_inline)) uint reverse_bits(uint x)
    {
        x = ((x & uint(0x55555555)) << 1) | ((x & uint(0xAAAAAAAA)) >> 1);
        x = ((x & uint(0x33333333)) << 2) | ((x & uint(0xCCCCCCCC)) >> 2);
        x = ((x & uint(0x0F0F0F0F)) << 4) | ((x & uint(0xF0F0F0F0)) >> 4);
        x = ((x & uint(0x00FF00FF)) << 8) | ((x & uint(0xFF00FF00)) >> 8);
        // Work around an old driver gotcha with newer AIR by type-casting not shifting
        ushort2 t = as_type<ushort2>(x);
        t = ushort2(t.y, t.x);
        return as_type<uint>(t);
    }
#endif
    
#if __METAL_VERSION__ <= 120
	static inline __attribute__((always_inline)) uint vector_array_deref(uint i)
	{
#if __METAL_DEVICE_CONSTANT_INDEX__ && __METAL_VERSION__ == 120
		if (GMetalDeviceManufacturer != Intel)
		{
			return i;
		}
		else
		{
			uint Indices[] = {0,1,2,3};
			return Indices[i];
		}
#else
		switch(i)
		{
			case 3: return 3;
			case 2: return 2;
			case 1: return 1;
			case 0: default: return 0;
		}
#endif
	}
#endif

    namespace accurate
    {
        template<typename T> static inline __attribute__((always_inline)) T cross(T x, T y) 
        { 
            float3 fx = float3(x); 
            float3 fy = float3(y); 
            return T(fma(fx[1], fy[2], -fma(fy[1], fx[2], 0.0)), fma(fx[2], fy[0], -fma(fy[2], fx[0], 0.0)), fma(fx[0], fy[1], -fma(fy[0], fx[1], 0.0))); 
        }

        template<typename T> static inline __attribute__((always_inline)) T rsqrt(T x)
        {
#if __METAL_MACOS__
            return fast::rsqrt(x);
#else
            return select(x, fast::rsqrt(x), isfinite(x));
#endif
        }

        template<typename T> static inline __attribute__((always_inline)) T sqrt(T x)
        {
#if __METAL_MACOS__
            return fast::sqrt(x);
#else
            return select(x, fast::sqrt(x), isfinite(x));
#endif
        }

		template<typename T> static inline __attribute__((always_inline)) T sincos(T x, thread T &cosval)
		{
#if __METAL_VERSION__ < 120 || __METAL_VERSION__ >= 210
#if __METAL_MACOS__
			cosval = fast::cos(x);
			return fast::sin(x);
#else
			cosval = select(x, fast::cos(x), isfinite(x));
			return select(x, fast::sin(x), isfinite(x));
#endif
#else
			return fast::sincos(x, cosval);
#endif
		}
		
#if __METAL_VERSION__ >= 120 && __METAL_VERSION__ < 210
		template<> inline __attribute__((always_inline)) float sincos<float>(float x, thread float &cosval)
		{
			uint xabs = as_type<uint>(x) & ~0x80000000;
			const float __nan = as_type<float>(0x7fc00000);
			if (xabs >= 0x7f800000)
			{
				cosval = __nan;
				return __nan;
			}
			
			if (xabs <= as_type<uint>(0x1p-12f))
			{
				cosval = 1.0f;
				return x;
			}
			return fast::sincos(x, cosval);
		}
		template<> inline __attribute__((always_inline)) float2 sincos<float2>(float2 x, thread float2 &cosval)
		{
			float c[2];
			float2 result = float2(accurate::sincos(x.x, c[0]), accurate::sincos(x.y, c[1]));
			cosval.x = c[0];
			cosval.y = c[1];
			return result;
		}
		template<> inline __attribute__((always_inline)) float3 sincos<float3>(float3 x, thread float3 &cosval)
		{
			float c[3];
			float3 result =float3(accurate::sincos(x.x, c[0]), accurate::sincos(x.y, c[1]), accurate::sincos(x.z, c[2]));
			cosval.x = c[0];
			cosval.y = c[1];
			cosval.z = c[2];
			return result;
		}
		template<> inline __attribute__((always_inline)) float4 sincos<float4>(float4 x, thread float4 &cosval)
		{
			float c[4];
			float4 result =float4(accurate::sincos(x.x, c[0]), accurate::sincos(x.y, c[1]), accurate::sincos(x.z, c[2]), accurate::sincos(x.w, c[3]));
			cosval.x = c[0];
			cosval.y = c[1];
			cosval.z = c[2];
			cosval.w = c[3];
			return result;
		}
#endif

        template<typename T> static inline __attribute__((always_inline)) T saturate(T x)
        {
#if __METAL_MACOS__
            return fast::saturate(x);
#else
            return select(T(0), fast::saturate(x), !isnan(x));
#endif
        }

        template<typename T, typename U> static inline __attribute__((always_inline)) T fmin(T x, U y)
        {
#if __METAL_MACOS__
            return fast::fmin(x, y);
#else
			return precise::fmin(x,y);
#endif
        }

        template<typename T, typename U> static inline __attribute__((always_inline)) T fmax(T x, U y)
        {
#if __METAL_MACOS__
            return fast::fmax(x, y);
#else
			return precise::fmax(x,y);
#endif
        }

        template<typename T, typename U, typename V> static inline __attribute__((always_inline)) T clamp(T x, U minval, V maxval)
        {
#if __METAL_MACOS__
            return fast::clamp(x, minval, maxval);
#else
			return precise::clamp(x, minval, maxval);
#endif
        }
		
#if __METAL_VERSION__ >= 210
#define GENERATE_MINMAX3_FUNC(Name, Func, Type)		\
		static inline __attribute__((always_inline)) Type Name( Type A, Type B, Type C ) 		\
		{											\
			return Name##3(A, B, C);			    \
		}
#else
#define GENERATE_MINMAX3_FUNC(Name, Func, Type)		\
		static inline __attribute__((always_inline)) Type Name( Type A, Type B, Type C ) 		\
		{											\
			return metal::Func(A, metal::Func(B, C));				\
		}
#endif
	
		GENERATE_MINMAX3_FUNC(min, fmin, float)
		GENERATE_MINMAX3_FUNC(min, fmin, float2)
		GENERATE_MINMAX3_FUNC(min, fmin, float3)
		GENERATE_MINMAX3_FUNC(min, fmin, float4)
		GENERATE_MINMAX3_FUNC(min, fmin, half)
		GENERATE_MINMAX3_FUNC(min, fmin, half2)
		GENERATE_MINMAX3_FUNC(min, fmin, half3)
		GENERATE_MINMAX3_FUNC(min, fmin, half4)
		GENERATE_MINMAX3_FUNC(min, min, int)
		GENERATE_MINMAX3_FUNC(min, min, int2)
		GENERATE_MINMAX3_FUNC(min, min, int3)
		GENERATE_MINMAX3_FUNC(min, min, int4)
		GENERATE_MINMAX3_FUNC(min, min, uint)
		GENERATE_MINMAX3_FUNC(min, min, uint2)
		GENERATE_MINMAX3_FUNC(min, min, uint3)
		GENERATE_MINMAX3_FUNC(min, min, uint4)

		GENERATE_MINMAX3_FUNC(max, fmax, float)
		GENERATE_MINMAX3_FUNC(max, fmax, float2)
		GENERATE_MINMAX3_FUNC(max, fmax, float3)
		GENERATE_MINMAX3_FUNC(max, fmax, float4)
		GENERATE_MINMAX3_FUNC(max, fmax, half)
		GENERATE_MINMAX3_FUNC(max, fmax, half2)
		GENERATE_MINMAX3_FUNC(max, fmax, half3)
		GENERATE_MINMAX3_FUNC(max, fmax, half4)
		GENERATE_MINMAX3_FUNC(max, max, int)
		GENERATE_MINMAX3_FUNC(max, max, int2)
		GENERATE_MINMAX3_FUNC(max, max, int3)
		GENERATE_MINMAX3_FUNC(max, max, int4)
		GENERATE_MINMAX3_FUNC(max, max, uint)
		GENERATE_MINMAX3_FUNC(max, max, uint2)
		GENERATE_MINMAX3_FUNC(max, max, uint3)
		GENERATE_MINMAX3_FUNC(max, max, uint4)
		
#define min3(A, B, C) ue4::accurate::min(A, B, C)
#define max3(A, B, C) ue4::accurate::max(A, B, C)
    }
}

#pragma mark -- Memory Barrier Functions --
namespace ue4
{
    static inline __attribute__((always_inline)) void SIMDGroupMemoryBarrier()
    {
#if __HAVE_SIMDGROUP_BARRIER__
        simdgroup_barrier(mem_flags::mem_threadgroup);
#else
        threadgroup_barrier(mem_flags::mem_threadgroup);
#endif
    }
    static inline __attribute__((always_inline)) void GroupMemoryBarrier()
    {
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    static inline __attribute__((always_inline)) void GroupMemoryBarrierWithGroupSync()
    {
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    static inline __attribute__((always_inline)) void DeviceMemoryBarrier()
    {
        threadgroup_barrier(mem_flags::mem_device);
    }
    static inline __attribute__((always_inline)) void DeviceMemoryBarrierWithGroupSync()
    {
        threadgroup_barrier(mem_flags::mem_device);
    }
    static inline __attribute__((always_inline)) void AllMemoryBarrier()
    {
#if __METAL_VERSION__ < 120
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        threadgroup_barrier(mem_flags::mem_device_and_threadgroup);
#pragma clang diagnostic pop
#else
        threadgroup_barrier(mem_flags(mem_flags::mem_device | mem_flags::mem_threadgroup));
#endif
    }
    static inline __attribute__((always_inline)) void AllMemoryBarrierWithGroupSync()
    {
#if __METAL_VERSION__ < 120
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        threadgroup_barrier(mem_flags(mem_flags::mem_device_and_threadgroup));
#pragma clang diagnostic pop
#else
        threadgroup_barrier(mem_flags(mem_flags::mem_device | mem_flags::mem_threadgroup));
#endif
    }
}

#pragma mark -- Extended Type Traits --
namespace ue4
{
	template<typename T>
	struct type_traits
	{
		enum { num_components = 0 };
		typedef T scalar_type;
		typedef T packed_type;
	};
	
#define DEFINE_TYPE_TRAITS(Type, Scalar, Val) template<> struct type_traits<Type> { enum { num_components = Val }; typedef Scalar scalar_type; typedef packed_##Type packed_type; }
#define DEFINE_SCALAR_TYPE_TRAITS(Type, Scalar, Val) template<> struct type_traits<Type> { enum { num_components = Val }; typedef Scalar scalar_type; typedef Type packed_type; }
#define DEFINE_PACKED_TYPE_TRAITS(Type, Scalar, Val) template<> struct type_traits<Type> { enum { num_components = Val }; typedef Scalar scalar_type; typedef Scalar##Val packed_type; }
    DEFINE_SCALAR_TYPE_TRAITS(char, char, 1);
    DEFINE_SCALAR_TYPE_TRAITS(uchar, uchar, 1);
    DEFINE_SCALAR_TYPE_TRAITS(short, short, 1);
    DEFINE_SCALAR_TYPE_TRAITS(ushort, ushort, 1);
    DEFINE_SCALAR_TYPE_TRAITS(int, int, 1);
    DEFINE_SCALAR_TYPE_TRAITS(uint, uint, 1);
    DEFINE_SCALAR_TYPE_TRAITS(half, half, 1);
    DEFINE_SCALAR_TYPE_TRAITS(float, float, 1);
    
    DEFINE_TYPE_TRAITS(char2, char, 2);
    DEFINE_TYPE_TRAITS(uchar2, uchar, 2);
    DEFINE_TYPE_TRAITS(short2, short, 2);
    DEFINE_TYPE_TRAITS(ushort2, ushort, 2);
    DEFINE_TYPE_TRAITS(int2, int, 2);
    DEFINE_TYPE_TRAITS(uint2, uint, 2);
    DEFINE_TYPE_TRAITS(half2, half, 2);
    DEFINE_TYPE_TRAITS(float2, float, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_char2, char, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_uchar2, uchar, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_short2, short, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_ushort2, ushort, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_int2, int, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_uint2, uint, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_half2, half, 2);
    DEFINE_PACKED_TYPE_TRAITS(packed_float2, float, 2);
    
    DEFINE_TYPE_TRAITS(char3, char, 3);
    DEFINE_TYPE_TRAITS(uchar3, uchar, 3);
    DEFINE_TYPE_TRAITS(short3, short, 3);
    DEFINE_TYPE_TRAITS(ushort3, ushort, 3);
    DEFINE_TYPE_TRAITS(int3, int, 3);
    DEFINE_TYPE_TRAITS(uint3, uint, 3);
    DEFINE_TYPE_TRAITS(half3, half, 3);
    DEFINE_TYPE_TRAITS(float3, float, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_char3, char, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_uchar3, uchar, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_short3, short, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_ushort3, ushort, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_int3, int, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_uint3, uint, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_half3, half, 3);
    DEFINE_PACKED_TYPE_TRAITS(packed_float3, float, 3);
    
    DEFINE_TYPE_TRAITS(char4, char, 4);
    DEFINE_TYPE_TRAITS(uchar4, uchar, 4);
    DEFINE_TYPE_TRAITS(short4, short, 4);
    DEFINE_TYPE_TRAITS(ushort4, ushort, 4);
    DEFINE_TYPE_TRAITS(int4, int, 4);
    DEFINE_TYPE_TRAITS(uint4, uint, 4);
    DEFINE_TYPE_TRAITS(half4, half, 4);
    DEFINE_TYPE_TRAITS(float4, float, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_char4, char, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_uchar4, uchar, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_short4, short, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_ushort4, ushort, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_int4, int, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_uint4, uint, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_half4, half, 4);
    DEFINE_PACKED_TYPE_TRAITS(packed_float4, float, 4);
#undef DEFINE_TYPE_TRAITS
	
	template <typename T>
	using make_scalar_t = typename type_traits<T>::scalar_type;
	
	template <typename T>
	using make_packed_t = typename type_traits<T>::packed_type;
}

#pragma mark -- Buffer Data Format Conversion --

namespace ue4
{
    enum format
    {
        Unknown =0,
        
        R8Sint =1,
        R8Uint =2,
        R8Snorm =3,
        R8Unorm =4,
        
        R16Sint =5,
        R16Uint =6,
        R16Snorm =7,
        R16Unorm =8,
        R16Half =9,
        
        R32Sint =10,
        R32Uint =11,
        R32Float =12,
        
        RG8Sint =13,
        RG8Uint =14,
        RG8Snorm =15,
        RG8Unorm =16,
        
        RG16Sint =17,
        RG16Uint =18,
        RG16Snorm =19,
        RG16Unorm =20,
        RG16Half =21,
        
        RG32Sint =22,
        RG32Uint =23,
        RG32Float =24,
        
        RGB8Sint =25,
        RGB8Uint =26,
        RGB8Snorm =27,
        RGB8Unorm =28,
        
        RGB16Sint =29,
        RGB16Uint =30,
        RGB16Snorm =31,
        RGB16Unorm =32,
        RGB16Half =33,
        
        RGB32Sint =34,
        RGB32Uint =35,
        RGB32Float =36,
        
        RGBA8Sint =37,
        RGBA8Uint =38,
        RGBA8Snorm =39,
        RGBA8Unorm =40,
        
        BGRA8Unorm =41,
        
        RGBA16Sint =42,
        RGBA16Uint =43,
        RGBA16Snorm =44,
        RGBA16Unorm =45,
        RGBA16Half =46,
        
        RGBA32Sint =47,
        RGBA32Uint =48,
        RGBA32Float =49,
        
        RGB10A2Unorm =50,
        
        RG11B10Half =51,
        
        R5G6B5Unorm =52,
        
        Max =53,
    };
		
	template <typename SrcType>
	static inline __attribute__((always_inline))
	SrcType swizzle_sample(SrcType value, const constant uint& swizzle)
	{
		uchar4 components = as_type<uchar4>(swizzle);
		return select(SrcType(value[components[0]-1], value[components[1]-1], value[components[2]-1], value[components[3]-1]), value, bool4(swizzle == 0));
	}
}
		
namespace ue4
{
    template<typename SrcType, typename FormatType, uint Components>
    struct format_texture
    {
        template <access A>
        static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i, const constant uint& l)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width)).x;
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), src.read(ushort2(i % width, y)).x, bool(y < height));
#endif
        }
        
        template <access A>
        static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, const constant uint& l, SrcType v)
        {
            ushort2 width_height = ushort2(src.get_width(), src.get_height());
            uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
            src.write(vec<FormatType, 4>(v), ushort2(index % width_height.x, index / width_height.x));
        }
		
#if defined(__HAVE_TEXTURE_BUFFER__)
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture_buffer<FormatType, A>& src, uint i)
		{
			return src.read(i).x;
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture_buffer<FormatType, A>& src, uint i, SrcType v)
		{
			src.write(vec<FormatType, 4>(v), i);
		}
#else
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width)).x;
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), src.read(ushort2(i % width, y)).x, bool(y < height));
#endif
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, SrcType v)
		{
			ushort2 width_height = ushort2(src.get_width(), src.get_height());
			uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
			src.write(vec<FormatType, 4>(v), ushort2(index % width_height.x, index / width_height.x));
		}
#endif
    };
    
    template<typename SrcType, typename FormatType>
    struct format_texture<SrcType, FormatType, 2>
    {
        template <access A>
        static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i, const constant uint& l)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width)).xy;
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), src.read(ushort2(i % width, y)).xy, bool(y < height));
#endif
        }
        
        template <access A>
        static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, const constant uint& l, SrcType v)
        {
            ushort2 width_height = ushort2(src.get_width(), src.get_height());
            uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
            src.write(vec<FormatType, 4>(v.xyxy), ushort2(index % width_height.x, index / width_height.x));
        }
		
#if defined(__HAVE_TEXTURE_BUFFER__)
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture_buffer<FormatType, A>& src, uint i)
		{
			return src.read(i).xy;
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture_buffer<FormatType, A>& src, uint i, SrcType v)
		{
			src.write(vec<FormatType, 4>(v.xyxy), i);
		}
#else
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width)).xy;
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), src.read(ushort2(i % width, y)).xy, bool(y < height));
#endif
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, SrcType v)
		{
			ushort2 width_height = ushort2(src.get_width(), src.get_height());
			uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
			src.write(vec<FormatType, 4>(v.xyxy), ushort2(index % width_height.x, index / width_height.x));
		}
#endif
    };
    
    template<typename SrcType, typename FormatType>
    struct format_texture<SrcType, FormatType, 3>
    {
#if defined(__HAVE_TEXTURE_BUFFER__)
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture_buffer<FormatType, A>& src, uint i)
		{
			return src.read(i).xyz;
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture_buffer<FormatType, A>& src, uint i, SrcType v)
		{
			src.write(vec<FormatType, 4>(v.xyzx), i);
		}
#else
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i, const constant uint& l)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width)).xyz;
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), src.read(ushort2(i % width, y)).xyz, bool(y < height));
#endif
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, const constant uint& l, SrcType v)
		{
			ushort2 width_height = ushort2(src.get_width(), src.get_height());
			uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
			src.write(vec<FormatType, 4>(v.xyzx), ushort2(index % width_height.x, index / width_height.x));
		}
		
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width)).xyz;
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), src.read(ushort2(i % width, y)).xyz, bool(y < height));
#endif
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, SrcType v)
		{
			ushort2 width_height = ushort2(src.get_width(), src.get_height());
			uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
			src.write(vec<FormatType, 4>(v.xyzx), ushort2(index % width_height.x, index / width_height.x));
		}
#endif
    };
    
    template<typename SrcType, typename FormatType>
    struct format_texture<SrcType, FormatType, 4>
    {
#if defined(__HAVE_TEXTURE_BUFFER__)
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture_buffer<FormatType, A>& src, uint i)
		{
			return src.read(i);
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture_buffer<FormatType, A>& src, uint i, SrcType v)
		{
			src.write(vec<FormatType, 4>(v), i);
		}
#else
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i, const constant uint& l)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width));
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), SrcType(src.read(ushort2(i % width, y))), bool(y < height));
#endif
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, const constant uint& l, SrcType v)
		{
			ushort2 width_height = ushort2(src.get_width(), src.get_height());
			uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
			src.write(vec<FormatType, 4>(v), ushort2(index % width_height.x, index / width_height.x));
		}
		
		template <access A>
		static inline __attribute__((always_inline)) SrcType load(thread texture2d<FormatType, A>& src, uint i)
        {
#if __METAL_MACOS__
            ushort width = src.get_width();
            return src.read(ushort2(i % width, i / width));
#else
            ushort width = src.get_width();
            ushort height = src.get_height();
            ushort y = i / width;
            return select(SrcType(0), SrcType(src.read(ushort2(i % width, y))), bool(y < height));
#endif
		}
		
		template <access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<FormatType, A>& src, uint i, SrcType v)
		{
			ushort2 width_height = ushort2(src.get_width(), src.get_height());
			uint index = min(i, (uint(width_height.x) * uint(width_height.y)));
			src.write(vec<FormatType, 4>(v), ushort2(index % width_height.x, index / width_height.x));
		}
#endif
    };
    
    template<typename SrcType, typename FormatType>
    struct format_type
    {
        static inline __attribute__((always_inline)) SrcType load(const device SrcType* src, uint i, const constant uint& l)
        {
            uint index = min(i, (l / sizeof(FormatType)) - 1);
            return select(SrcType(0), SrcType(((const device FormatType*)src)[index]), bool(i < (l / sizeof(FormatType))));
        }
        
        static inline __attribute__((always_inline)) void store(device SrcType* src, uint i, const constant uint& l, SrcType v)
        {
            uint index = min(i, (l / sizeof(FormatType)));
            ((device FormatType*)src)[index] = FormatType(v);
        }
    };
    
    template<typename SrcType, typename FormatType>
    struct format_type_pack
    {
        static inline __attribute__((always_inline)) FormatType load(const device SrcType* src, uint i, const constant uint& l)
        {
            uint index = min(i, (l / sizeof(FormatType)) - 1);
            return select(FormatType(0), ((const device FormatType*)src)[index], (i < (l / sizeof(FormatType))));
        }
        
        static inline __attribute__((always_inline)) void store(device SrcType* src, uint i, const constant uint& l, FormatType v)
        {
            uint index = min(i, (l / sizeof(FormatType)));
            ((device FormatType*)src)[index] = v;
        }
    };
    
    template<typename SrcType, typename FormatType>
    struct format_type_unorm
    {
        static inline __attribute__((always_inline)) SrcType load(const device SrcType* src, uint i, const constant uint& l)
        {
            return (format_type<SrcType, FormatType>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device SrcType* src, uint i, const constant uint& l, SrcType v)
        {
            format_type<SrcType, FormatType>::store(src, i, l, v);
        }
    };
    
    template<>
    struct format_type_unorm<half, uchar>
    {
        static inline __attribute__((always_inline)) half load(const device half* src, uint i, const constant uint& l)
        {
            return (format_type<half, uchar>::load(src, i, l) / half(255.f));
        }
        
        static inline __attribute__((always_inline)) void store(device half* src, uint i, const constant uint& l, half v)
        {
            format_type<half, uchar>::store(src, i, l, (v * half(255.f)));
        }
    };
    
    template<>
    struct format_type_unorm<half2, uchar2>
    {
        static inline __attribute__((always_inline)) half2 load(const device half2* src, uint i, const constant uint& l)
        {
            return (format_type<half2, uchar2>::load(src, i, l) / half(255.f));
        }
        
        static inline __attribute__((always_inline)) void store(device half2* src, uint i, const constant uint& l, half2 v)
        {
            format_type<half2, uchar2>::store(src, i, l, (v * half(255.f)));
        }
    };
    
    template<>
    struct format_type_unorm<half3, uchar3>
    {
        static inline __attribute__((always_inline)) half3 load(const device half3* src, uint i, const constant uint& l)
        {
            return (format_type<half3, uchar3>::load(src, i, l) / half(255.f));
        }
        
        static inline __attribute__((always_inline)) void store(device half3* src, uint i, const constant uint& l, half3 v)
        {
            format_type<half3, uchar3>::store(src, i, l, (v * half(255.f)));
        }
    };
    
    template<>
    struct format_type_unorm<half4, uchar4>
    {
        static inline __attribute__((always_inline)) half4 load(const device half4* src, uint i, const constant uint& l)
        {
            return unpack_unorm4x8_to_half(format_type_pack<half4, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device half4* src, uint i, const constant uint& l, half4 v)
        {
            format_type_pack<half4, uint>::store(src, i, l, pack_half_to_unorm4x8(v));
        }
    };
    
    template<>
    struct format_type_unorm<half, ushort>
    {
        static inline __attribute__((always_inline)) half load(const device half* src, uint i, const constant uint& l)
        {
            return (format_type<half, ushort>::load(src, i, l) / half(65536.f));
        }
        
        static inline __attribute__((always_inline)) void store(device half* src, uint i, const constant uint& l, half v)
        {
            format_type<half, ushort>::store(src, i, l, (v * half(65536.f)));
        }
    };
    
    template<>
    struct format_type_unorm<half2, ushort2>
    {
        static inline __attribute__((always_inline)) half2 load(const device half2* src, uint i, const constant uint& l)
        {
            return unpack_unorm2x16_to_half(format_type_pack<half2, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device half2* src, uint i, const constant uint& l, half2 v)
        {
            format_type_pack<half2, uint>::store(src, i, l, pack_half_to_unorm2x16(v));
        }
    };
    
    template<>
    struct format_type_unorm<half3, ushort3>
    {
        static inline __attribute__((always_inline)) half3 load(const device half3* src, uint i, const constant uint& l)
        {
            return (format_type<half3, ushort3>::load(src, i, l) / half(65536.f));
        }
        
        static inline __attribute__((always_inline)) void store(device half3* src, uint i, const constant uint& l, half3 v)
        {
            format_type<half3, ushort3>::store(src, i, l, (v * half(65536.f)));
        }
    };
    
    template<>
    struct format_type_unorm<half4, ushort4>
    {
        static inline __attribute__((always_inline)) half4 load(const device half4* src, uint i, const constant uint& l)
        {
            return (format_type<half4, ushort4>::load(src, i, l) / half(65536.f));
        }
        
        static inline __attribute__((always_inline)) void store(device half4* src, uint i, const constant uint& l, half4 v)
        {
            format_type<half4, ushort4>::store(src, i, l, (v * half(65536.f)));
        }
    };
    
    template<>
    struct format_type_unorm<float, uchar>
    {
        static inline __attribute__((always_inline)) float load(const device float* src, uint i, const constant uint& l)
        {
            return (format_type<float, uchar>::load(src, i, l) / 255.f);
        }
        
        static inline __attribute__((always_inline)) void store(device float* src, uint i, const constant uint& l, float v)
        {
            format_type<float, uchar>::store(src, i, l, (v * 255.f));
        }
    };
    
    template<>
    struct format_type_unorm<float2, uchar2>
    {
        static inline __attribute__((always_inline)) float2 load(const device float2* src, uint i, const constant uint& l)
        {
            return (format_type<float2, uchar2>::load(src, i, l) / 255.f);
        }
        
        static inline __attribute__((always_inline)) void store(device float2* src, uint i, const constant uint& l, float2 v)
        {
            format_type<float2, uchar2>::store(src, i, l, (v * 255.f));
        }
    };
    
    template<>
    struct format_type_unorm<float3, uchar3>
    {
        static inline __attribute__((always_inline)) float3 load(const device float3* src, uint i, const constant uint& l)
        {
            return (format_type<float3, uchar3>::load(src, i, l) / 255.f);
        }
        
        static inline __attribute__((always_inline)) void store(device float3* src, uint i, const constant uint& l, float3 v)
        {
            format_type<float3, uchar3>::store(src, i, l, (v * 255.f));
        }
    };
    
    template<>
    struct format_type_unorm<float4, uchar4>
    {
        static inline __attribute__((always_inline)) float4 load(const device float4* src, uint i, const constant uint& l)
        {
            return unpack_unorm4x8_to_float(format_type_pack<float4, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device float4* src, uint i, const constant uint& l, float4 v)
        {
            format_type_pack<float4, uint>::store(src, i, l, pack_float_to_unorm4x8(v));
        }
    };
    
    template<>
    struct format_type_unorm<float, ushort>
    {
        static inline __attribute__((always_inline)) float load(const device float* src, uint i, const constant uint& l)
        {
            return (format_type<float, ushort>::load(src, i, l) / 65536.f);
        }
        
        static inline __attribute__((always_inline)) void store(device float* src, uint i, const constant uint& l, float v)
        {
            format_type<float, ushort>::store(src, i, l, (v * 65536.f));
        }
    };
    
    template<>
    struct format_type_unorm<float2, ushort2>
    {
        static inline __attribute__((always_inline)) float2 load(const device float2* src, uint i, const constant uint& l)
        {
            return unpack_unorm2x16_to_float(format_type_pack<float2, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device float2* src, uint i, const constant uint& l, float2 v)
        {
            format_type_pack<float2, uint>::store(src, i, l, pack_float_to_unorm2x16(v));
        }
    };
    
    template<>
    struct format_type_unorm<float3, ushort3>
    {
        static inline __attribute__((always_inline)) float3 load(const device float3* src, uint i, const constant uint& l)
        {
            return (format_type<float3, ushort3>::load(src, i, l) / 65536.f);
        }
        
        static inline __attribute__((always_inline)) void store(device float3* src, uint i, const constant uint& l, float3 v)
        {
            format_type<float3, ushort3>::store(src, i, l, (v * 65536.f));
        }
    };
    
    template<>
    struct format_type_unorm<float4, ushort4>
    {
        static inline __attribute__((always_inline)) float4 load(const device float4* src, uint i, const constant uint& l)
        {
            return (format_type<float4, ushort4>::load(src, i, l) / 65536.f);
        }
        
        static inline __attribute__((always_inline)) void store(device float4* src, uint i, const constant uint& l, float4 v)
        {
            format_type<float4, ushort4>::store(src, i, l, (v * 65536.f));
        }
    };
	
	/*
	 	snorm types are defined as mapping
	 	[-1.0, 1.0] to [-MAX, MAX]
	 	n = number of bits
	 	MAX = 2^(n-1) - 1
	 	0.0 == 0
	 
	 	snorm to float:
	 	int c
	 	float(c) * 1.0f / (2^(n-1) - 1.0f)
	 
	 	float to snorm:
	 	float c
	 	int(c * (2^(n-1) -1.0f))
	 */
    
    template<typename SrcType, typename FormatType>
    struct format_type_snorm
    {
        static inline __attribute__((always_inline)) SrcType load(const device SrcType* src, uint i, const constant uint& l)
        {
            return (format_type<SrcType, FormatType>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device SrcType* src, uint i, const constant uint& l, SrcType v)
        {
            format_type<SrcType, FormatType>::store(src, i, l, v);
        }
    };
    
    template<>
    struct format_type_snorm<half, char>
    {
        static inline __attribute__((always_inline)) half load(const device half* src, uint i, const constant uint& l)
        {
            return format_type<half, char>::load(src, i, l) / half(127.f);
        }
        
        static inline __attribute__((always_inline)) void store(device half* src, uint i, const constant uint& l, half v)
        {
            format_type<half, char>::store(src, i, l, v * half(127.f));
        }
    };
    
    template<>
    struct format_type_snorm<half2, char2>
    {
        static inline __attribute__((always_inline)) half2 load(const device half2* src, uint i, const constant uint& l)
        {
            return format_type<half2, char2>::load(src, i, l) / half(127.f);
        }
        
        static inline __attribute__((always_inline)) void store(device half2* src, uint i, const constant uint& l, half2 v)
        {
            format_type<half2, char2>::store(src, i, l, v * half(127.f));
        }
    };
    
    template<>
    struct format_type_snorm<half3, char3>
    {
        static inline __attribute__((always_inline)) half3 load(const device half3* src, uint i, const constant uint& l)
        {
            return format_type<half3, char3>::load(src, i, l) / half(127.f);
        }
        
        static inline __attribute__((always_inline)) void store(device half3* src, uint i, const constant uint& l, half3 v)
        {
            format_type<half3, char3>::store(src, i, l, v * half(127.f));
        }
    };
    
    template<>
    struct format_type_snorm<half4, char4>
    {
        static inline __attribute__((always_inline)) half4 load(const device half4* src, uint i, const constant uint& l)
        {
            return unpack_snorm4x8_to_half(format_type_pack<half4, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device half4* src, uint i, const constant uint& l, half4 v)
        {
            format_type_pack<half4, uint>::store(src, i, l, pack_half_to_snorm4x8(v));
        }
    };
    
    template<>
    struct format_type_snorm<half, short>
    {
        static inline __attribute__((always_inline)) half load(const device half* src, uint i, const constant uint& l)
        {
            return format_type<half, short>::load(src, i, l) / half(32767.f);
        }
        
        static inline __attribute__((always_inline)) void store(device half* src, uint i, const constant uint& l, half v)
        {
            format_type<half, short>::store(src, i, l, v * half(32767.f));
        }
    };
    
    template<>
    struct format_type_snorm<half2, short2>
    {
        static inline __attribute__((always_inline)) half2 load(const device half2* src, uint i, const constant uint& l)
        {
            return unpack_snorm2x16_to_half(format_type_pack<half2, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device half2* src, uint i, const constant uint& l, half2 v)
        {
            format_type_pack<half2, uint>::store(src, i, l, pack_half_to_snorm2x16(v));
        }
    };
    
    template<>
    struct format_type_snorm<half3, short3>
    {
        static inline __attribute__((always_inline)) half3 load(const device half3* src, uint i, const constant uint& l)
        {
            return format_type<half3, short3>::load(src, i, l) / half(32767.f);
        }
        
        static inline __attribute__((always_inline)) void store(device half3* src, uint i, const constant uint& l, half3 v)
        {
            format_type<half3, short3>::store(src, i, l, v * half(32767.f));
        }
    };
    
    template<>
    struct format_type_snorm<half4, short4>
    {
        static inline __attribute__((always_inline)) half4 load(const device half4* src, uint i, const constant uint& l)
        {
            return format_type<half4, short4>::load(src, i, l) / half(32767.f);
        }
        
        static inline __attribute__((always_inline)) void store(device half4* src, uint i, const constant uint& l, half4 v)
        {
            format_type<half4, short4>::store(src, i, l, v * half(32767.f));
        }
    };
    
    template<>
    struct format_type_snorm<float, char>
    {
        static inline __attribute__((always_inline)) float load(const device float* src, uint i, const constant uint& l)
        {
            return format_type<float, char>::load(src, i, l) / 127.f;
        }
        
        static inline __attribute__((always_inline)) void store(device float* src, uint i, const constant uint& l, float v)
        {
            format_type<float, char>::store(src, i, l, v * 127.f);
        }
    };
    
    template<>
    struct format_type_snorm<float2, char2>
    {
        static inline __attribute__((always_inline)) float2 load(const device float2* src, uint i, const constant uint& l)
        {
            return format_type<float2, char2>::load(src, i, l) / 127.f;
        }
        
        static inline __attribute__((always_inline)) void store(device float2* src, uint i, const constant uint& l, float2 v)
        {
            format_type<float2, char2>::store(src, i, l, v * 127.f);
        }
    };
    
    template<>
    struct format_type_snorm<float3, char3>
    {
        static inline __attribute__((always_inline)) float3 load(const device float3* src, uint i, const constant uint& l)
        {
            return format_type<float3, char3>::load(src, i, l) / 127.f;
        }
        
        static inline __attribute__((always_inline)) void store(device float3* src, uint i, const constant uint& l, float3 v)
        {
            format_type<float3, char3>::store(src, i, l, v * 127.f);
        }
    };
    
    template<>
    struct format_type_snorm<float4, char4>
    {
        static inline __attribute__((always_inline)) float4 load(const device float4* src, uint i, const constant uint& l)
        {
            return unpack_snorm4x8_to_float(format_type_pack<float4, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device float4* src, uint i, const constant uint& l, float4 v)
        {
            format_type_pack<float4, uint>::store(src, i, l, pack_float_to_snorm4x8(v));
        }
    };
    
    template<>
    struct format_type_snorm<float, short>
    {
        static inline __attribute__((always_inline)) float load(const device float* src, uint i, const constant uint& l)
        {
            return format_type<float, short>::load(src, i, l) / 32767.f;
        }
        
        static inline __attribute__((always_inline)) void store(device float* src, uint i, const constant uint& l, float v)
        {
            format_type<float, short>::store(src, i, l, v * 32767.f);
        }
    };
    
    template<>
    struct format_type_snorm<float2, short2>
    {
        static inline __attribute__((always_inline)) float2 load(const device float2* src, uint i, const constant uint& l)
        {
            return unpack_snorm2x16_to_float(format_type_pack<float2, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device float2* src, uint i, const constant uint& l, float2 v)
        {
            format_type_pack<float2, uint>::store(src, i, l, pack_float_to_snorm2x16(v));
        }
    };
    
    template<>
    struct format_type_snorm<float3, short3>
    {
        static inline __attribute__((always_inline)) float3 load(const device float3* src, uint i, const constant uint& l)
        {
            return format_type<float3, short3>::load(src, i, l) / 32767.f;
        }
        
        static inline __attribute__((always_inline)) void store(device float3* src, uint i, const constant uint& l, float3 v)
        {
            format_type<float3, short3>::store(src, i, l, v * 32767.f);
        }
    };
    
    template<>
    struct format_type_snorm<float4, short4>
    {
        static inline __attribute__((always_inline)) float4 load(const device float4* src, uint i, const constant uint& l)
        {
            return format_type<float4, short4>::load(src, i, l) / 32767.f;
        }
        
        static inline __attribute__((always_inline)) void store(device float4* src, uint i, const constant uint& l, float4 v)
        {
            format_type<float4, short4>::store(src, i, l, v * 32767.f);
        }
    };
    
    /**
     * 3 component vector corresponding to DXGI_FORMAT_R11G11B10_FLOAT.
     * Conversion code from XMFLOAT3PK in DirectXPackedVector.h
     */
    struct float_rg11b10
    {
        uint v;
    public:
        inline __attribute__((always_inline)) uint xm() const { return (v & 0x3F); } // x-mantissa
        inline __attribute__((always_inline)) uint xe() const { return ((v >> 6) & 0x1F); } // x-exponent
        inline __attribute__((always_inline)) uint ym() const { return ((v >> 11) & 0x3F); } // y-mantissa
        inline __attribute__((always_inline)) uint ye() const { return ((v >> 17) & 0x1F); } // y-exponent
        inline __attribute__((always_inline)) uint zm() const { return ((v >> 22) & 0x1F); } // z-mantissa
        inline __attribute__((always_inline)) uint ze() const { return ((v >> 27) & 0x1F); } // z-exponent
        
        inline __attribute__((always_inline)) float_rg11b10(uint In)
        {
            v = In;
        }
        
        inline __attribute__((always_inline)) float_rg11b10(float3 Src)
        {
            pack(Src);
        }
        
        uint inline __attribute__((always_inline)) value() const
        {
            return v;
        }
        
        void inline __attribute__((always_inline)) pack(float3 Src)
        {
            uint IValue[3] = {as_type<uint>(Src.x), as_type<uint>(Src.y), as_type<uint>(Src.z)};
            uint Result[3];
            
            // X & Y Channels (5-bit exponent, 6-bit mantissa)
            for (uint j=0; j < 2; ++j)
            {
                uint Sign = IValue[j] & 0x80000000;
                uint I = IValue[j] & 0x7FFFFFFF;
                
                if ((I & 0x7F800000) == 0x7F800000)
                {
                    // INF or NAN
                    Result[j] = 0x7c0;
                    if (( I & 0x7FFFFF ) != 0)
                    {
                        Result[j] = 0x7c0 | (((I>>17)|(I>>11)|(I>>6)|(I))&0x3f);
                    }
                    else if ( Sign )
                    {
                        // -INF is clamped to 0 since 3PK is positive only
                        Result[j] = 0;
                    }
                }
                else if ( Sign )
                {
                    // 3PK is positive only, so clamp to zero
                    Result[j] = 0;
                }
                else if (I > 0x477E0000U)
                {
                    // The number is too large to be represented as a float11, set to max
                    Result[j] = 0x7BF;
                }
                else
                {
                    if (I < 0x38800000U)
                    {
                        // The number is too small to be represented as a normalized float11
                        // Convert it to a denormalized value.
                        uint Shift = 113U - (I >> 23U);
                        I = (0x800000U | (I & 0x7FFFFFU)) >> Shift;
                    }
                    else
                    {
                        // Rebias the exponent to represent the value as a normalized float11
                        I += 0xC8000000U;
                    }
                    
                    Result[j] = ((I + 0xFFFFU + ((I >> 17U) & 1U)) >> 17U)&0x7ffU;
                }
            }
            
            // Z Channel (5-bit exponent, 5-bit mantissa)
            uint Sign = IValue[2] & 0x80000000;
            uint I = IValue[2] & 0x7FFFFFFF;
            
            if ((I & 0x7F800000) == 0x7F800000)
            {
                // INF or NAN
                Result[2] = 0x3e0;
                if ( I & 0x7FFFFF )
                {
                    Result[2] = 0x3e0 | (((I>>18)|(I>>13)|(I>>3)|(I))&0x1f);
                }
                else if ( Sign )
                {
                    // -INF is clamped to 0 since 3PK is positive only
                    Result[2] = 0;
                }
            }
            else if ( Sign )
            {
                // 3PK is positive only, so clamp to zero
                Result[2] = 0;
            }
            else if (I > 0x477C0000U)
            {
                // The number is too large to be represented as a float10, set to max
                Result[2] = 0x3df;
            }
            else
            {
                if (I < 0x38800000U)
                {
                    // The number is too small to be represented as a normalized float10
                    // Convert it to a denormalized value.
                    uint Shift = 113U - (I >> 23U);
                    I = (0x800000U | (I & 0x7FFFFFU)) >> Shift;
                }
                else
                {
                    // Rebias the exponent to represent the value as a normalized float10
                    I += 0xC8000000U;
                }
                
                Result[2] = ((I + 0x1FFFFU + ((I >> 18U) & 1U)) >> 18U)&0x3ffU;
            }
            
            // Pack Result into memory
            v = (Result[0] & 0x7ff)
            | ( (Result[1] & 0x7ff) << 11 )
            | ( (Result[2] & 0x3ff) << 22 );
        }
        
        float3 inline __attribute__((always_inline)) unpack() const
        {
            uint Result[3];
            uint Mantissa;
            uint Exponent;
            
            // X Channel (6-bit mantissa)
            Mantissa = xm();
            
            if ( xe() == 0x1f ) // INF or NAN
            {
                Result[0] = 0x7f800000 | (xm() << 17);
            }
            else
            {
                if ( xe() != 0 ) // The value is normalized
                {
                    Exponent = xe();
                }
                else if (Mantissa != 0) // The value is denormalized
                {
                    // Normalize the value in the resulting float
                    Exponent = 1;
                    
                    do
                    {
                        Exponent--;
                        Mantissa <<= 1;
                    } while ((Mantissa & 0x40) == 0);
                    
                    Mantissa &= 0x3F;
                }
                else // The value is zero
                {
                    Exponent = (uint)-112;
                }
                
                Result[0] = ((Exponent + 112) << 23) | (Mantissa << 17);
            }
            
            // Y Channel (6-bit mantissa)
            Mantissa = ym();
            
            if ( ye() == 0x1f ) // INF or NAN
            {
                Result[1] = 0x7f800000 | (ym() << 17);
            }
            else
            {
                if ( ye() != 0 ) // The value is normalized
                {
                    Exponent = ye();
                }
                else if (Mantissa != 0) // The value is denormalized
                {
                    // Normalize the value in the resulting float
                    Exponent = 1;
                    
                    do
                    {
                        Exponent--;
                        Mantissa <<= 1;
                    } while ((Mantissa & 0x40) == 0);
                    
                    Mantissa &= 0x3F;
                }
                else // The value is zero
                {
                    Exponent = (uint)-112;
                }
                
                Result[1] = ((Exponent + 112) << 23) | (Mantissa << 17);
            }
            
            // Z Channel (5-bit mantissa)
            Mantissa = zm();
            
            if ( ze() == 0x1f ) // INF or NAN
            {
                Result[2] = 0x7f800000 | (zm() << 17);
            }
            else
            {
                if ( ze() != 0 ) // The value is normalized
                {
                    Exponent = ze();
                }
                else if (Mantissa != 0) // The value is denormalized
                {
                    // Normalize the value in the resulting float
                    Exponent = 1;
                    
                    do
                    {
                        Exponent--;
                        Mantissa <<= 1;
                    } while ((Mantissa & 0x20) == 0);
                    
                    Mantissa &= 0x1F;
                }
                else // The value is zero
                {
                    Exponent = (uint)-112;
                }
                
                Result[2] = ((Exponent + 112) << 23) | (Mantissa << 18);
            }
            
            return float3(as_type<float>(Result[0]), as_type<float>(Result[1]), as_type<float>(Result[2]));
        }
    };
    
    template<typename T>
    struct format_type_rg11b10
    {
        static inline __attribute__((always_inline)) T load(const device T* src, uint i, const constant uint& l)
        {
            return T(float_rg11b10(format_type_pack<T, uint>::load(src, i, l)).unpack());
        }
        
        static inline __attribute__((always_inline)) void store(device T* src, uint i, const constant uint& l, T v)
        {
            format_type_pack<T, uint>::store(src, i, l, float_rg11b10(float3(v)).value());
        }
    };
    
    template<typename T>
    struct format_type_r5g6b5
    {
        static inline __attribute__((always_inline)) T load(const device T* src, uint i, const constant uint& l)
        {
            return T(pack_float_to_unorm565(format_type_pack<T, ushort>::load(src, i, l)));
        }
        
        static inline __attribute__((always_inline)) void store(device T* src, uint i, const constant uint& l, T v)
        {
            format_type_pack<T, ushort>::store(src, i, l, pack_float_to_unorm565(float3(v)));
        }
    };
    
    template<>
    struct format_type_r5g6b5<half3>
    {
        static inline __attribute__((always_inline)) half3 load(const device half3* src, uint i, const constant uint& l)
        {
            return unpack_unorm565_to_half(format_type_pack<half3, ushort>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device half3* src, uint i, const constant uint& l, half3 v)
        {
            format_type_pack<half3, ushort>::store(src, i, l, pack_half_to_unorm565(v));
        }
    };
    
    template<typename T>
    struct format_type_rgb10a2
    {
        static inline __attribute__((always_inline)) T load(const device T* src, uint i, const constant uint& l)
        {
            return T(unpack_unorm10a2_to_float(format_type_pack<T, uint>::load(src, i, l)));
        }
        
        static inline __attribute__((always_inline)) void store(device T* src, uint i, const constant uint& l, T v)
        {
            format_type_pack<T, uint>::store(src, i, l, pack_float_to_unorm10a2(float4(v)));
        }
    };
    
    template<>
    struct format_type_rgb10a2<half4>
    {
        static inline __attribute__((always_inline)) half4 load(const device half4* src, uint i, const constant uint& l)
        {
            return unpack_unorm10a2_to_half(format_type_pack<half4, uint>::load(src, i, l));
        }
        
        static inline __attribute__((always_inline)) void store(device half4* src, uint i, const constant uint& l, half4 v)
        {
            format_type_pack<half4, uint>::store(src, i, l, pack_half_to_unorm10a2(v));
        }
    };
    
    template<uint Index>
    struct format_type_access
    {
        static inline __attribute__((always_inline)) const constant uint& format(const constant uint& format, constant uint* t) { return format > Unknown ? format : t[(Index * 2) + 1]; };
    };
    
    template<typename T, uint Index, uint Components>
    struct format_access
    {
		static inline __attribute__((always_inline))  T load(const device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l)
		{
			return format_type<T, T>::load(src, i, l);
		}
		
		static inline __attribute__((always_inline))  void store(device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l, T v)
		{
			format_type<T, T>::store(src, i, l, v);
		}
    };

    template<typename T, uint Index>
    struct format_access<T, Index, 1>
    {
        static inline __attribute__((always_inline))  T load(const device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l)
        {
            switch(format_type_access<Index>::format(format, t))
            {
//              case R8Sint:
//                  return (format_type<T, char>::load(src, i, l));
//              case R8Snorm:
//                  return format_type_snorm<T, char>::load(src, i, l);
//              case R16Snorm:
//                  return format_type_snorm<T, short>::load(src, i, l);
//              case R16Half:
//                  return (format_type<T, half>::load(src, i, l));
//              case R32Sint:
//                  return (format_type<T, int>::load(src, i, l));
                case R8Uint:
                    return (format_type<T, uchar>::load(src, i, l));
                case R8Unorm:
                    return format_type_unorm<T, uchar>::load(src, i, l);
                case R16Sint:
                    return (format_type<T, short>::load(src, i, l));
                case R16Uint:
                    return (format_type<T, ushort>::load(src, i, l));
                case R16Unorm:
                    return format_type_unorm<T, ushort>::load(src, i, l);
                case R32Uint:
                    return (format_type<T, uint>::load(src, i, l));
                case R32Float:
                    return (format_type<T, float>::load(src, i, l));
                case Unknown:
                case Max:
                default:
                    return format_type<T, T>::load(src, i, l);
            }
        }
        
        static inline __attribute__((always_inline))  void store(device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l, T v)
        {
            switch(format_type_access<Index>::format(format, t))
            {
//              case R8Sint:
//                  (format_type<T, char>::store(src, i, l, v));
//                  break;
//              case R8Snorm:
//                  format_type_snorm<T, char>::store(src, i, l, v);
//                  break;
//              case R16Snorm:
//                  format_type_snorm<T, short>::store(src, i, l, v);
//                  break;
//              case R16Half:
//                  (format_type<T, half>::store(src, i, l, v));
//                  break;
//              case R32Sint:
//                  (format_type<T, int>::store(src, i, l, v));
//                  break;
                case R8Uint:
                    (format_type<T, uchar>::store(src, i, l, v));
                    break;
                case R8Unorm:
                    format_type_unorm<T, uchar>::store(src, i, l, v);
                    break;
                case R16Sint:
                    (format_type<T, short>::store(src, i, l, v));
                    break;
                case R16Uint:
                    (format_type<T, ushort>::store(src, i, l, v));
                    break;
                case R16Unorm:
                    format_type_unorm<T, ushort>::store(src, i, l, v);
                    break;
                case R32Uint:
                    (format_type<T, uint>::store(src, i, l, v));
                    break;
                case R32Float:
                    (format_type<T, float>::store(src, i, l, v));
                    break;
                case Unknown:
                case Max:
                default:
                    format_type<T, T>::store(src, i, l, v);
                    break;
            }
        }
    };
    
    template<typename T, uint Index>
    struct format_access<T, Index, 2>
    {
        static inline __attribute__((always_inline))  T load(const device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l)
        {
            switch(format_type_access<Index>::format(format, t))
            {
//              case RG8Sint:
//                  return format_type<T, char2>::load(src, i, l);
//              case RG8Uint:
//                  return format_type<T, uchar2>::load(src, i, l);
//              case RG8Snorm:
//                  return format_type_snorm<T, char2>::load(src, i, l);
//              case RG16Sint:
//                  return format_type<T, short2>::load(src, i, l);
//              case RG16Snorm:
//                  return format_type_snorm<T, short2>::load(src, i, l);
//              case RG32Sint:
//                  return format_type<T, int2>::load(src, i, l);
//              case RG32Uint:
//                  return format_type<T, uint2>::load(src, i, l);
                case RG8Unorm:
                    return format_type_unorm<T, uchar2>::load(src, i, l);
                case RG16Uint:
                    return format_type<T, ushort2>::load(src, i, l);
                case RG16Unorm:
                    return format_type_unorm<T, ushort2>::load(src, i, l);
                case RG16Half:
                    return format_type<T, half2>::load(src, i, l);
                case RG32Float:
                    return format_type<T, float2>::load(src, i, l);
                case Unknown:
                case Max:
                default:
                    return format_type<T, T>::load(src, i, l);
            }
        }
        
        static inline __attribute__((always_inline))  void store(device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l, T v)
        {
            switch(format_type_access<Index>::format(format, t))
            {
//              case RG8Sint:
//                  format_type<T, char2>::store(src, i, l, v);
//                  break;
//              case RG8Uint:
//                  format_type<T, uchar2>::store(src, i, l, v);
//                  break;
//              case RG8Snorm:
//                  format_type_snorm<T, char2>::store(src, i, l, v);
//                  break;
//              case RG16Sint:
//                  format_type<T, short2>::store(src, i, l, v);
//                  break;
//              case RG16Snorm:
//                  format_type_snorm<T, short2>::store(src, i, l, v);
//                  break;
//              case RG32Sint:
//                  format_type<T, int2>::store(src, i, l, v);
//                  break;
//              case RG32Uint:
//                  format_type<T, uint2>::store(src, i, l, v);
//                  break;
                case RG8Unorm:
                    format_type_unorm<T, uchar2>::store(src, i, l, v);
                    break;
                case RG16Uint:
                    format_type<T, ushort2>::store(src, i, l, v);
                    break;
                case RG16Unorm:
                    format_type_unorm<T, ushort2>::store(src, i, l, v);
                    break;
                case RG16Half:
                    format_type<T, half2>::store(src, i, l, v);
                    break;
                case RG32Float:
                    format_type<T, float2>::store(src, i, l, v);
                    break;
                case Unknown:
                case Max:
                default:
                    format_type<T, T>::store(src, i, l, v);
                    break;
            }
        }
    };
    
    template<typename T, uint Index>
    struct format_access<T, Index, 3>
    {
        static inline __attribute__((always_inline))  T load(const device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l)
        {
            switch(format_type_access<Index>::format(format, t))
            {
                case RGB16Half:
                    return format_type<T, packed_half3>::load(src, i, l);
                case RGB32Float:
                    return format_type<T, packed_float3>::load(src, i, l);
                case RG11B10Half:
                    return format_type_rg11b10<T>::load(src, i, l);
                case R5G6B5Unorm:
                    return format_type_r5g6b5<T>::load(src, i, l);
                case RGB8Sint:
                    return format_type<T, packed_char3>::load(src, i, l);
                case RGB8Uint:
                    return format_type<T, packed_uchar3>::load(src, i, l);
                case RGB8Snorm:
                    return format_type_snorm<T, packed_char3>::load(src, i, l);
                case RGB8Unorm:
                    return format_type_unorm<T, packed_uchar3>::load(src, i, l);
                case RGB16Sint:
                    return format_type<T, packed_short3>::load(src, i, l);
                case RGB16Uint:
                    return format_type<T, packed_ushort3>::load(src, i, l);
                case RGB16Snorm:
                    return format_type_snorm<T, packed_short3>::load(src, i, l);
                case RGB16Unorm:
                    return format_type_unorm<T, packed_ushort3>::load(src, i, l);
                case RGB32Sint:
                    return format_type<T, packed_int3>::load(src, i, l);
                case RGB32Uint:
                    return format_type<T, packed_uint3>::load(src, i, l);
                case Unknown:
                case Max:
                default:
                    return format_type<T, make_packed_t<T>>::load(src, i, l);
            }
        }
        
        static inline __attribute__((always_inline))  void store(device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l, T v)
        {
            switch(format_type_access<Index>::format(format, t))
            {
                case RGB16Half:
                    format_type<T, packed_half3>::store(src, i, l, v);
                    break;
                case RGB32Float:
                    format_type<T, packed_float3>::store(src, i, l, v);
                    break;
                case RG11B10Half:
                    format_type_rg11b10<T>::store(src, i, l, v);
                    break;
                case R5G6B5Unorm:
                    format_type_r5g6b5<T>::store(src, i, l, v);
                    break;
                case RGB8Sint:
                    format_type<T, packed_char3>::store(src, i, l, v);
                    break;
                case RGB8Uint:
                    format_type<T, packed_uchar3>::store(src, i, l, v);
                    break;
                case RGB8Snorm:
                    format_type_snorm<T, packed_char3>::store(src, i, l, v);
                    break;
                case RGB8Unorm:
                    format_type_unorm<T, packed_uchar3>::store(src, i, l, v);
                    break;
                case RGB16Sint:
                    format_type<T, packed_short3>::store(src, i, l, v);
                    break;
                case RGB16Uint:
                    format_type<T, packed_ushort3>::store(src, i, l, v);
                    break;
                case RGB16Snorm:
                    format_type_snorm<T, packed_short3>::store(src, i, l, v);
                    break;
                case RGB16Unorm:
                    format_type_unorm<T, packed_ushort3>::store(src, i, l, v);
                    break;
                case RGB32Sint:
                    format_type<T, packed_int3>::store(src, i, l, v);
                    break;
                case RGB32Uint:
                    format_type<T, packed_uint3>::store(src, i, l, v);
                    break;
                case Unknown:
                case Max:
                default:
                    format_type<T, make_packed_t<T>>::store(src, i, l, v);
                    break;
            }
        }
    };
    
    template<typename T, uint Index>
    struct format_access<T, Index, 4>
    {
        static inline __attribute__((always_inline))  T load(const device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l)
        {
            switch(format_type_access<Index>::format(format, t))
            {
//              case RGBA8Sint:
//                  return format_type<T, char4>::load(src, i, l);
//              case RGBA32Sint:
//                  return format_type<T, int4>::load(src, i, l);
                case RGBA8Uint:
                    return format_type<T, uchar4>::load(src, i, l);
                case RGBA8Snorm:
                    return format_type_snorm<T, char4>::load(src, i, l);
                case RGBA8Unorm:
                    return format_type_unorm<T, uchar4>::load(src, i, l);
                case BGRA8Unorm:
                    return format_type_unorm<T, uchar4>::load(src, i, l).zyxw;
                case RGBA16Sint:
                    return format_type<T, short4>::load(src, i, l);
                case RGBA16Uint:
                    return format_type<T, ushort4>::load(src, i, l);
                case RGBA16Snorm:
                    return format_type_snorm<T, short4>::load(src, i, l);
                case RGBA16Unorm:
                    return format_type_unorm<T, ushort4>::load(src, i, l);
                case RGBA16Half:
                    return format_type<T, half4>::load(src, i, l);
                case RGBA32Uint:
                    return format_type<T, uint4>::load(src, i, l);
                case RGBA32Float:
                    return format_type<T, float4>::load(src, i, l);
                case RGB10A2Unorm:
                    return format_type_rgb10a2<T>::load(src, i, l);
                case Unknown:
                case Max:
                default:
                    return format_type<T, T>::load(src, i, l);
            }
        }
        
        static inline __attribute__((always_inline))  void store(device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l, T v)
        {
            switch(format_type_access<Index>::format(format, t))
            {
//              case RGBA8Sint:
//                  format_type<T, char4>::store(src, i, l, v);
//                  break;
//              case RGBA32Sint:
//                  format_type<T, int4>::store(src, i, l, v);
//                  break;
                case RGBA8Uint:
                    format_type<T, uchar4>::store(src, i, l, v);
                    break;
                case RGBA8Snorm:
                    format_type_snorm<T, char4>::store(src, i, l, v);
                    break;
                case RGBA8Unorm:
                    format_type_unorm<T, uchar4>::store(src, i, l, v);
                    break;
                case BGRA8Unorm:
                    format_type_unorm<T, uchar4>::store(src, i, l, v.zyxw);
                    break;
                case RGBA16Sint:
                    format_type<T, short4>::store(src, i, l, v);
                    break;
                case RGBA16Uint:
                    format_type<T, ushort4>::store(src, i, l, v);
                    break;
                case RGBA16Snorm:
                    format_type_snorm<T, short4>::store(src, i, l, v);
                    break;
                case RGBA16Unorm:
                    format_type_unorm<T, ushort4>::store(src, i, l, v);
                    break;
                case RGBA16Half:
                    format_type<T, half4>::store(src, i, l, v);
                    break;
                case RGBA32Uint:
                    format_type<T, uint4>::store(src, i, l, v);
                    break;
                case RGBA32Float:
                    format_type<T, float4>::store(src, i, l, v);
                    break;
                case RGB10A2Unorm:
                    format_type_rgb10a2<T>::store(src, i, l, v);
                    break;
                case Unknown:
                case Max:
                default:
                    format_type<T, T>::store(src, i, l, v);
                    break;
            }
        }
    };
    
    template<typename T, uint Index>
    T inline __attribute__((always_inline)) load_format(const device T* src, constant uint* t, uint i, const constant uint& format, const constant uint& l)
    {
        return format_access<T, Index, type_traits<T>::num_components>::load(src, t, i, format, l);
    }
    
    template<typename T, uint Index>
    void inline __attribute__((always_inline)) store_format(device T* p, constant uint* t, uint i, const constant uint& format, const constant uint& l, T v)
    {
        format_access<T, Index, type_traits<T>::num_components>::store(p, t, i, format, l, v);
    }
    
	template<typename T>
	static inline __attribute__((always_inline)) uint packHalf2x16(vec<T, 2> h)
	{
#if METAL_RUNTIME_COMPILER && __METAL_VERSION__ <= 120
		return (uint(as_type<ushort>(half(h.x))) | (uint(as_type<ushort>(half(h.y))) << 16u));
#else
		return as_type<uint>(half2(h));
#endif
	}
	
	static inline __attribute__((always_inline)) float2 unpackHalf2x16(uint i)
	{
#if METAL_RUNTIME_COMPILER && __METAL_VERSION__ <= 120
		return float2(as_type<half>(ushort(i & 0xffff)), as_type<half>(ushort((i >> 16) & 0xffff)));
#else
		return float2(as_type<half2>(i));
#endif
	}
}

#pragma mark -- Typed Buffer Class  --

namespace ue4
{
    template<typename T>
    struct typed_buffer
    {
        friend struct buffer;
    public:
        typed_buffer(const device typed_buffer& other) device = default;
        
    private:
        T storage;
    };
	
	template<>
    struct typed_buffer<int>
    {
        friend struct buffer;
        template<memory_order> friend struct buffer_atomic;
    public:
        typed_buffer(const device typed_buffer& other) device = default;
        
    private:
        
        static inline __attribute__((always_inline)) int load_atomic(device typed_buffer* p, uint i)
        {
            return atomic_load_explicit(&((volatile device atomic_int*)p)[i], memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) void store_atomic(device typed_buffer* p, uint i,int v)
        {
            atomic_store_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int exchange_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_exchange_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) bool compare_exchange_weak_atomic(device typed_buffer* p, uint i,thread int *expected, int v)
        {
            return atomic_compare_exchange_weak_explicit(&((volatile device atomic_int*)p)[i], expected, v, memory_order_relaxed, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int fetch_add_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_fetch_add_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int fetch_sub_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_fetch_sub_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int fetch_or_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_fetch_or_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int fetch_xor_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_fetch_xor_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int fetch_and_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_fetch_and_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int fetch_min_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_fetch_min_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) int fetch_max_atomic(device typed_buffer* p, uint i,int v)
        {
            return atomic_fetch_max_explicit(&((volatile device atomic_int*)p)[i], v, memory_order_relaxed);
        }
        
    private:
        int storage;
    };
	
	template<>
    struct typed_buffer<uint>
    {
        friend struct buffer;
        template<memory_order> friend struct buffer_atomic;
    public:
        typed_buffer(const device typed_buffer& other) device = default;
		
    private:
        
        static inline __attribute__((always_inline)) uint load_atomic(device typed_buffer* p, uint i)
        {
            return atomic_load_explicit(&((volatile device atomic_uint*)p)[i], memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) void store_atomic(device typed_buffer* p, uint i, uint v)
        {
            atomic_store_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint exchange_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_exchange_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) bool compare_exchange_weak_atomic(device typed_buffer* p, uint i, thread uint *expected, uint v)
        {
            return atomic_compare_exchange_weak_explicit(&((volatile device atomic_uint*)p)[i], expected, v, memory_order_relaxed, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint fetch_add_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_fetch_add_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint fetch_sub_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_fetch_sub_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint fetch_or_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_fetch_or_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint fetch_xor_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_fetch_xor_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint fetch_and_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_fetch_and_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint fetch_min_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_fetch_min_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
        static inline __attribute__((always_inline)) uint fetch_max_atomic(device typed_buffer* p, uint i, uint v)
        {
            return atomic_fetch_max_explicit(&((volatile device atomic_uint*)p)[i], v, memory_order_relaxed);
        }
        
    private:
        uint storage;
    };
	
	
#if __METAL_VERSION__ >= 200
	template<typename T, metal::access A = access::read>
	struct buffer_argument
	{
#if defined(__HAVE_TEXTURE_BUFFER__)
		texture_buffer<ue4::make_scalar_t<T>, A> TextureBuffer;
#else
		texture2d<ue4::make_scalar_t<T>, A> TextureBuffer;
#endif
		device typed_buffer<T> const* Pointer;
	};
	
	template<typename T>
	struct buffer_argument<T, access::read_write>
	{
#if defined(__HAVE_TEXTURE_BUFFER__)
		texture_buffer<ue4::make_scalar_t<T>, access::read_write> TextureBuffer;
#else
		texture2d<ue4::make_scalar_t<T>, access::read_write> TextureBuffer;
#endif
		device typed_buffer<T>* Pointer;
	};
#endif
	
    struct buffer
    {
    public:
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T load(device const typed_buffer<T>* p, uint i, constant uint* t)
        {
			return load_format<T, I>((device const T*)p, t, i, t[(I*2)+1], t[I*2]);
		}
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) void store(device typed_buffer<T>* p, uint i, constant uint* t, T v)
        {
			store_format<T, I>((device T*)p, t, i, t[(I*2)+1], t[I*2], v);
		}
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T load(device const T* p, uint i, constant uint* t)
        {
            return format_type<T, T>::load(p, i, t[I*2]);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) void store(device T* p, uint i, constant uint* t, T v)
        {
            format_type<T, T>::store(p, i, t[I*2], v);
        }

#if defined(__HAVE_TEXTURE_BUFFER__)
        template<typename T, uint I, typename F, access A>
        static inline __attribute__((always_inline)) T load(thread texture_buffer<F, A>& p, uint i, constant uint* t)
        {
			return format_texture<T, F, type_traits<T>::num_components>::load(p, i);
        }
        
        template<typename T, uint I, typename F, access A>
        static inline __attribute__((always_inline)) void store(thread texture_buffer<F, A>& p, uint i, constant uint* t, T v)
        {
			format_texture<T, F, type_traits<T>::num_components>::store(p, i, v);
        }
        
        template<typename T, uint I, typename F, access A>
        static inline __attribute__((always_inline)) T load(thread texture_buffer<F, A>& p, uint i)
        {
			return format_texture<T, F, type_traits<T>::num_components>::load(p, i);
        }
        
        template<typename T, uint I, typename F, access A>
        static inline __attribute__((always_inline)) void store(thread texture_buffer<F, A>& p, uint i, T v)
        {
			format_texture<T, F, type_traits<T>::num_components>::store(p, i, v);
        }
#else
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T load(thread texture2d<F, A>& p, uint i, constant uint* t)
		{
			return format_texture<T, F, type_traits<T>::num_components>::load(p, i, t[I*2]);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<F, A>& p, uint i, constant uint* t, T v)
		{
			format_texture<T, F, type_traits<T>::num_components>::store(p, i, t[I*2], v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T load(thread texture2d<F, A>& p, uint i)
		{
			return format_texture<T, F, type_traits<T>::num_components>::load(p, i);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) void store(thread texture2d<F, A>& p, uint i, T v)
		{
			format_texture<T, F, type_traits<T>::num_components>::store(p, i, v);
		}
#endif
		
#if __METAL_VERSION__ >= 200
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T load(thread buffer_argument<F, A>& p, uint i, constant uint* t)
		{
			if (type_traits<T>::num_components != 3)
			{
				return format_texture<T, ue4::make_scalar_t<F>, type_traits<T>::num_components>::load(p.TextureBuffer, i);
			}
			else
			{
				return buffer::load<T, I>(p.Pointer, i, t);
			}
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) void store(thread buffer_argument<F, A>& p, uint i, constant uint* t, T v)
		{
			if (type_traits<T>::num_components != 3)
			{
				format_texture<T, ue4::make_scalar_t<F>, type_traits<T>::num_components>::store(p.TextureBuffer, i, v);
			}
			else
			{
				buffer::store<T, I>(p.Pointer, i, t, v);
			}
		}
#endif
    };
    
    template<memory_order order>
    struct buffer_atomic
    {
    };
    
    template<>
    struct buffer_atomic<memory_order_relaxed>
    {
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T load_atomic(device typed_buffer<T>* p, constant uint* t, uint i)
        {
            return typed_buffer<T>::load_atomic(p, min(i, t[I*2] / sizeof(T)));
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) void store_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            typed_buffer<T>::store_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T exchange_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::exchange_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) bool compare_exchange_weak_atomic(device typed_buffer<T>* p, constant uint* t, uint i, thread T *expected, T v)
        {
            return typed_buffer<T>::compare_exchange_weak_atomic(p, min(i, t[I*2] / sizeof(T)), expected, v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T fetch_add_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::fetch_add_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T fetch_sub_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::fetch_sub_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T fetch_or_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::fetch_or_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T fetch_xor_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::fetch_xor_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T fetch_and_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::fetch_and_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T fetch_min_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::fetch_min_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
        
        template<typename T, uint I>
        static inline __attribute__((always_inline)) T fetch_max_atomic(device typed_buffer<T>* p, constant uint* t, uint i, T v)
        {
            return typed_buffer<T>::fetch_max_atomic(p, min(i, t[I*2] / sizeof(T)), v);
        }
		
#if __METAL_VERSION__ >= 200
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T load_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i)
		{
			return buffer_atomic::load_atomic<T, I>(p.Pointer, t, i);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) void store_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			buffer_atomic::store_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T exchange_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::exchange_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) bool compare_exchange_weak_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, thread T *expected, T v)
		{
			return buffer_atomic::compare_exchange_weak_atomic<T, I>(p.Pointer, t, i, expected, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T fetch_add_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::fetch_add_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T fetch_sub_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::fetch_sub_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T fetch_or_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::fetch_or_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T fetch_xor_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::fetch_xor_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T fetch_and_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::fetch_and_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T fetch_min_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::fetch_min_atomic<T, I>(p.Pointer, t, i, v);
		}
		
		template<typename T, uint I, typename F, access A>
		static inline __attribute__((always_inline)) T fetch_max_atomic(thread buffer_argument<F, A>& p, constant uint* t, uint i, T v)
		{
			return buffer_atomic::fetch_max_atomic<T, I>(p.Pointer, t, i, v);
		}
#endif
    };

// No Typed-buffer implementation - buffers are accessed "raw"
#define __METAL_TYPED_BUFFER_RAW__ 0
// Typed-buffer emulation using texture2d
#define __METAL_TYPED_BUFFER_2D__ 1
// Typed-buffer implementation using a texture-buffer
#define __METAL_TYPED_BUFFER_TB__ 3

// Typed-buffer implementation for read-only buffers
#ifndef __METAL_TYPED_BUFFER_READ_IMPL__
#define __METAL_TYPED_BUFFER_READ_IMPL__ __METAL_TYPED_BUFFER_RAW__
#endif

// Typed-buffer implementation for read-write buffers
#ifndef __METAL_TYPED_BUFFER_RW_IMPL__
#define __METAL_TYPED_BUFFER_RW_IMPL__ __METAL_TYPED_BUFFER_RAW__
#endif
    
#if defined(__HAVE_TEXTURE_BUFFER__) && __METAL_TYPED_BUFFER_READ_IMPL__ == __METAL_TYPED_BUFFER_TB__
#define typedBuffer1_read(T, N, I) texture_buffer<ue4::make_scalar_t<T>> N [[ texture(I) ]]
#define typedBuffer2_read(T, N, I) texture_buffer<ue4::make_scalar_t<T>> N [[ texture(I) ]]
#define typedBuffer4_read(T, N, I) texture_buffer<ue4::make_scalar_t<T>> N [[ texture(I) ]]
#elif __METAL_TYPED_BUFFER_READ_IMPL__ == __METAL_TYPED_BUFFER_2D__
#define typedBuffer1_read(T, N, I) texture2d<ue4::make_scalar_t<T>> N [[ texture(I) ]]
#define typedBuffer2_read(T, N, I) texture2d<ue4::make_scalar_t<T>> N [[ texture(I) ]]
#define typedBuffer4_read(T, N, I) texture2d<ue4::make_scalar_t<T>> N [[ texture(I) ]]
#else
#define typedBuffer1_read(T, N, I) const device typed_buffer<T>* N [[ buffer(I) ]]
#define typedBuffer2_read(T, N, I) const device typed_buffer<T>* N [[ buffer(I) ]]
#define typedBuffer4_read(T, N, I) const device typed_buffer<T>* N [[ buffer(I) ]]
#endif

#if defined(__HAVE_TEXTURE_BUFFER__) && __METAL_TYPED_BUFFER_RW_IMPL__ == __METAL_TYPED_BUFFER_TB__
#define typedBuffer1_rw(T, N, I) texture_buffer<ue4::make_scalar_t<T>, access::read_write> N [[ texture(I) ]]
#define typedBuffer2_rw(T, N, I) texture_buffer<ue4::make_scalar_t<T>, access::read_write> N [[ texture(I) ]]
#define typedBuffer4_rw(T, N, I) texture_buffer<ue4::make_scalar_t<T>, access::read_write> N [[ texture(I) ]]
#elif __METAL_TYPED_BUFFER_RW_IMPL__ == __METAL_TYPED_BUFFER_2D__
#define typedBuffer1_rw(T, N, I) texture2d<ue4::make_scalar_t<T>, access::read_write> N [[ texture(I) ]]
#define typedBuffer2_rw(T, N, I) texture2d<ue4::make_scalar_t<T>, access::read_write> N [[ texture(I) ]]
#define typedBuffer4_rw(T, N, I) texture2d<ue4::make_scalar_t<T>, access::read_write> N [[ texture(I) ]]
#else
#define typedBuffer1_rw(T, N, I) device typed_buffer<T>* N [[ buffer(I) ]]
#define typedBuffer2_rw(T, N, I) device typed_buffer<T>* N [[ buffer(I) ]]
#define typedBuffer4_rw(T, N, I) device typed_buffer<T>* N [[ buffer(I) ]]
#endif

// Due to pixel-format limitations vec<T,3> types will always use the typed-buffer implementation.
#define typedBuffer3_read(T, N, I) const device typed_buffer<T>* N [[ buffer(I) ]]
#define typedBuffer3_rw(T, N, I) device typed_buffer<T>* N [[ buffer(I) ]]

// Similarly all atomics must go through the typed-buffer implementation.
#define typedBuffer_atomic(T, N, I) const device typed_buffer<T>* N [[ buffer(I) ]]
}

#pragma mark -- Cube Array Texture --
#if defined(__HAVE_TEXTURE_CUBE_ARRAY__) && defined(__METAL_USE_TEXTURE_CUBE_ARRAY__) && __HAVE_TEXTURE_CUBE_ARRAY__ == 1 && __METAL_USE_TEXTURE_CUBE_ARRAY__ == 1
#define HAVE_TEXTURE_CUBE_ARRAY 1
#else
#define HAVE_TEXTURE_CUBE_ARRAY 0
#endif
#if HAVE_TEXTURE_CUBE_ARRAY
#define texturecube_array texturecube_array
#define depthcube_array depthcube_array
#else
#define texturecube_array texture2d_array
#define depthcube_array depth2d_array
#endif

namespace ue4
{
#if !HAVE_TEXTURE_CUBE_ARRAY
    // CubeFaces as laid out in UE4 as a flat array seem to be
    // Right (x+), Left(x-), Forward(y+), Back(y-), Up (z+), Down (z-)
    // Largest vector component of the vector chooses a face, and is used to project the other two
    // into a 0-1 UV space on that face.
    static inline __attribute__((always_inline)) float3 CubemapTo2DArrayFace(float3 P, uint ArrayIndex)
    {
        //take abs of incoming vector to make face selection simpler
        float3 Coords = abs(P.xyz);
        float CubeFace = 0;
        float ProjectionAxis = 0;
        float u = 0;
        float v = 0;
        if(Coords.x >= Coords.y && Coords.x >= Coords.z)
        {
            //here we are +-X face
            CubeFace = P.x >= 0 ? 0 : 1;
            ProjectionAxis = Coords.x;
            u = P.x >= 0 ? -P.z : P.z;
            v = -P.y;
        }
        //here we are +-Y face
        else if(Coords.y >= Coords.x && Coords.y >= Coords.z)
        {
            CubeFace = P.y >= 0 ? 2 : 3;
            ProjectionAxis = Coords.y;
            u = P.x;
            v = P.y >= 0 ? P.z : -P.z;
        }
        //here we are +-Z face
        else
        {
            CubeFace = P.z >= 0 ? 4 : 5;
            ProjectionAxis = Coords.z;
            u = P.z >= 0 ? P.x : -P.x;
            v = -P.y;
        }
        u = 0.5 * (u/ProjectionAxis + 1);
        v = 0.5 * (v/ProjectionAxis + 1);
        return float3(u, v, CubeFace + ArrayIndex * 6);
    }
#endif
    
    struct texture_cube_array
    {
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(texturecube_array<T, a> tex, sampler s, float3 coord, uint array)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z));
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(texturecube_array<T, a> tex, sampler s, float3 coord, uint array, bias options)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array, options);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z), options);
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(texturecube_array<T, a> tex, sampler s, float3 coord, uint array, level options)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array, options);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z), options);
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(texturecube_array<T, a> tex, sampler s, float3 coord, uint array, gradientcube options)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array, options);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z), options);
#endif
        }
        
#if defined(__HAVE_16B_COORDS__)
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> read(texturecube_array<T, a> tex, ushort2 coord, ushort face, ushort array, ushort lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.read(face, array, lod);
#else
            return tex.read(coord, ((array * 6) + face), lod);
#endif
        }
#endif
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> read(texturecube_array<T, a> tex, uint2 coord, uint face, uint array, uint lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.read(face, array, lod);
#else
            return tex.read(coord, ((array * 6) + face), lod);
#endif
        }
        
#if defined(__HAVE_16B_COORDS__)
        template<typename T, access a>
        static inline __attribute__((always_inline)) void write(texturecube_array<T, a> tex, vec<T, 4> color, ushort2 coord, ushort face, ushort array, ushort lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            tex.write(color, coord, face, array, lod);
#else
            tex.write(color, coord, ((array * 6) + face), lod);
#endif
        }
#if defined(__HAVE_IMAGEBLOCKS__)
        template<typename T, access a, typename E, typename L>
        static inline __attribute__((always_inline)) void write(texturecube_array<T, a> tex, imageblock_slice<E, L> slice, ushort2 coord, ushort face, ushort array, ushort lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            tex.write(slice, coord, face, array, lod);
#else
            tex.write(slice, coord, ((array * 6) + face), lod);
#endif
        }
#endif
#endif
        template<typename T, access a>
        static inline __attribute__((always_inline)) void write(texturecube_array<T, a> tex, vec<T, 4> color, uint2 coord, uint face, uint array, uint lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            tex.write(color, coord, face, array, lod);
#else
            tex.write(color, coord, ((array * 6) + face), lod);
#endif
        }
#if defined(__HAVE_IMAGEBLOCKS__)
        template<typename T, access a, typename E, typename L>
        static inline __attribute__((always_inline)) void write(texturecube_array<T, a> tex, imageblock_slice<E, L> slice, uint2 coord, uint face, uint array, uint lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            tex.write(slice, coord, face, array, lod);
#else
            tex.write(slice, coord, ((array * 6) + face), lod);
#endif
        }
#endif
        
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> gather(texturecube_array<T, a> tex, sampler s, float3 coord, uint array, component c = component::x)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.gather(s, coord, array, c);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.gather(s, coords.xy, uint(coords.z), c);
#endif
        }
        
#if defined(__HAVE_TEXTURE_READWRITE__)
        template<typename T, access a>
        static inline __attribute__((always_inline)) void fence(texturecube_array<T, a> tex)
        {
            tex.fence();
        }
#endif
        
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_width(texturecube_array<T, a> tex, uint lod = 0)
        {
            return tex.get_width(lod);
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_height(texturecube_array<T, a> tex,uint lod = 0)
        {
            return tex.get_height(lod);
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_array_size(texturecube_array<T, a> tex)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.get_array_size();
#else
            return (tex.get_array_size() / 6);
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_num_mip_levels(texturecube_array<T, a> tex)
        {
            return tex.get_num_mip_levels();
        }
    };
    
    struct depth_cube_array
    {
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(depthcube_array<T, a> tex, sampler s, float3 coord, uint array)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z));
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(depthcube_array<T, a> tex, sampler s, float3 coord, uint array, bias options)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array, options);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z), options);
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(depthcube_array<T, a> tex, sampler s, float3 coord, uint array, level options)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array, options);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z), options);
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> sample(depthcube_array<T, a> tex, sampler s, float3 coord, uint array, gradientcube options)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample(s, coord, array, options);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample(s, coords.xy, uint(coords.z), options);
#endif
        }
        
        template<typename T, access a>
        static inline __attribute__((always_inline)) T sample_compare(depthcube_array<T, a> tex, sampler s, float3 coord, uint array, float compare_value)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample_compare(s, coord, array, compare_value);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample_compare(s, coords.xy, uint(coords.z), compare_value);
#endif
        }
        
        template<typename T, access a>
        static inline __attribute__((always_inline)) T sample_compare(depthcube_array<T, a> tex, sampler s, float3 coord, uint array, float compare_value, level options)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.sample_compare(s, coord, array, compare_value, options);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.sample_compare(s, coords.xy, uint(coords.z), compare_value, options);
#endif
        }
        
#if defined(__HAVE_16B_COORDS__)
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> read(depthcube_array<T, a> tex, ushort2 coord, ushort face, ushort array, ushort lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.read(face, array, lod);
#else
            return tex.read(coord, ((array * 6) + face), lod);
#endif
        }
#endif
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> read(depthcube_array<T, a> tex, uint2 coord, uint face, uint array, uint lod = 0)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.read(face, array, lod);
#else
            return tex.read(coord, ((array * 6) + face), lod);
#endif
        }
        
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> gather(depthcube_array<T, a> tex, sampler s, float3 coord, uint array, component c = component::x)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.gather(s, coord, array, c);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.gather(s, coords.xy, uint(coords.z), c);
#endif
        }
        
        template<typename T, access a>
        static inline __attribute__((always_inline)) vec<T, 4> gather_compare(depthcube_array<T, a> tex, sampler s, float3 coord, uint array, float compare_value)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.gather_compare(s, coord, array, compare_value);
#else
            float3 coords = CubemapTo2DArrayFace(coord, array);
            return tex.gather_compare(s, coords.xy, uint(coords.z), compare_value);
#endif
        }
        
#if defined(__HAVE_TEXTURE_READWRITE__)
        template<typename T, access a>
        static inline __attribute__((always_inline)) void fence(depthcube_array<T, a> tex)
        {
            tex.fence();
        }
#endif
        
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_width(depthcube_array<T, a> tex, uint lod = 0)
        {
            return tex.get_width(lod);
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_height(depthcube_array<T, a> tex,uint lod = 0)
        {
            return tex.get_height(lod);
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_array_size(depthcube_array<T, a> tex)
        {
#if HAVE_TEXTURE_CUBE_ARRAY
            return tex.get_array_size();
#else
            return (tex.get_array_size() / 6);
#endif
        }
        template<typename T, access a>
        static inline __attribute__((always_inline)) uint get_num_mip_levels(depthcube_array<T, a> tex)
        {
            return tex.get_num_mip_levels();
        }
    };
}

#pragma mark -- Wave Functions --
#if defined(__HAVE_QUADGROUP__) || defined(__HAVE_SIMDGROUP__)
#if defined(__HAVE_SIMDGROUP__)
#define decl_wave_index_vars    ushort num_simd [[ threads_per_simdgroup ]], \
ushort simd_id [[ thread_index_in_simdgroup ]]
#define get_threads_per_wave() num_simd
#define get_thread_index_in_wave() simd_id
#elif defined(__HAVE_QUADGROUP__)
#define decl_wave_index_vars ushort simd_id [[ thread_index_in_quadgroup ]]
#define get_threads_per_wave() 4
#define get_thread_index_in_wave() simd_id
#endif

namespace ue4
{
    static inline __attribute__((always_inline)) bool wave_once()
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_is_first();
#else
        return false;
#endif
    }
	
	static inline __attribute__((always_inline)) uint wave_get_lane_count()
	{
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		simd_vote::vote_t value = (simd_vote::vote_t)simd_active_threads_mask();
		uint x = value;
		uint y = (value >> 32);
        return (popcount(x) + popcount(y));
#else
		return (uint)0;
#endif
	}
	
	static inline __attribute__((always_inline)) uint wave_get_lane_index()
	{
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_prefix_exclusive_sum(1);
#else
		return (uint)0;
#endif        
	}
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_broadcast(T data, ushort simd_lane_id)
    {
#if defined(__HAVE_QUADGROUP__)
        return quad_broadcast(data, simd_lane_id);
#elif defined(__HAVE_SIMDGROUP__)
        return simd_broadcast(data, simd_lane_id);
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_shuffle(T data, ushort simd_lane_id)
    {
#if defined(__HAVE_QUADGROUP__)
        return quad_shuffle(data, simd_lane_id);
#elif defined(__HAVE_SIMDGROUP__)
        return simd_shuffle(data, simd_lane_id);
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_read_first(T data)
    {
        return wave_shuffle(data, 0);
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_read(T data, ushort simd_lane_id)
    {
        return wave_shuffle(data, simd_lane_id);
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_shuffle_down(T data, ushort delta)
    {
#if defined(__HAVE_QUADGROUP__)
        return quad_shuffle_down(data, delta);
#elif defined(__HAVE_SIMDGROUP__)
        return simd_shuffle_down(data, delta);
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_shuffle_up(T data, ushort delta)
    {
#if defined(__HAVE_QUADGROUP__)
        return quad_shuffle_up(data, delta);
#elif defined(__HAVE_SIMDGROUP__)
        return simd_shuffle_up(data, delta);
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_shuffle_xor(T data, ushort mask)
    {
#if defined(__HAVE_QUADGROUP__)
        return quad_shuffle_xor(data, mask);
#elif defined(__HAVE_SIMDGROUP__)
        return simd_shuffle_xor(data, mask);
#endif
    }
    
    static inline __attribute__((always_inline)) bool wave_any(bool Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_any(Data);
#else
        bool Any = false;
        for (ushort i = 0; Any == false && i < num_simd_lanes; i++)
        {
            Any |= ((bool)wave_shuffle((uint)Data, i) == true);
        }
        return Any;
#endif
    }
    
    static inline __attribute__((always_inline)) bool wave_all(bool Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_all(Data);
#else
        bool Any = true;
        for (ushort i = 0; Any == true && i < num_simd_lanes; i++)
        {
            Any &= ((bool)wave_shuffle((uint)Data, i) == true);
        }
        return Any;
#endif
    }
    
    static inline __attribute__((always_inline)) bool wave_equal(bool Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return (simd_any(Data) == simd_all(Data));
#else
        bool Any = true;
        for (ushort i = 0; Any == true && i < num_simd_lanes; i++)
        {
            Any &= ((bool)wave_shuffle((uint)Data, i) == Data);
        }
        return Any;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_sum(T Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_sum(Data);
#else
        T Result = T(0);
        for (ushort i = 0; i < num_simd_lanes; i++)
        {
            Result += wave_shuffle(Data, i);
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_product(T Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_product(Data);
#else
        T Result = wave_shuffle(Data, 0);
        for (ushort i = 1; i < num_simd_lanes; i++)
        {
            Result *= wave_shuffle(Data, i);
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_prefix_sum(T Data, ushort lane_index, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_prefix_exclusive_sum(Data);
#else
        T Result = T(0);
        for (ushort i = 0; i < num_simd_lanes && i < lane_index; i++)
        {
            Result += wave_shuffle(Data, i);
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_prefix_product(T Data, ushort lane_index, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_prefix_exclusive_product(Data);
#else
        T Result = T(1);
        for (ushort i = 0; i < num_simd_lanes && i < lane_index; i++)
        {
            Result *= wave_shuffle(Data, i);
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_and(T Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_and(Data);
#else
        T Result = wave_shuffle(Data, 0);
        for (ushort i = 1; i < num_simd_lanes; i++)
        {
            Result &= wave_shuffle(Data, i);
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_or(T Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_or(Data);
#else
        T Result = wave_shuffle(Data, 0);
        for (ushort i = 1; i < num_simd_lanes; i++)
        {
            Result |= wave_shuffle(Data, i);
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_xor(T Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_xor(Data);
#else
        T Result = wave_shuffle(Data, 0);
        for (ushort i = 1; i < num_simd_lanes; i++)
        {
            Result ^= wave_shuffle(Data, i);
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_min(T Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_min(Data);
#else
        T Result = wave_shuffle(Data, 0);
        for (ushort i = 1; i < num_simd_lanes; i++)
        {
            Result = min(Result, wave_shuffle(Data, i));
        }
        return Result;
#endif
    }
    
    template<typename T>
    static inline __attribute__((always_inline)) T wave_max(T Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		return simd_max(Data);
#else
        T Result = wave_shuffle(Data, 0);
        for (ushort i = 1; i < num_simd_lanes; i++)
        {
            Result = max(Result, wave_shuffle(Data, i));
        }
        return Result;
#endif
    }
    
    static inline __attribute__((always_inline)) uint4 wave_ballot(bool Data, ushort num_simd_lanes)
    {
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
		uint4 Result(0);
		simd_vote::vote_t value = (simd_vote::vote_t)simd_ballot(Data);
		Result.x = value;
		Result.y = (value >> 32);
		return Result;
#else
        uint4 Result(0);
        for (ushort i = 0; i < 32 && i < num_simd_lanes; i++)
        {
            Result.x |= wave_shuffle((uint)Data, i) << i;
        }
		for (ushort i = 32; i < 64 && i < num_simd_lanes; i++)
		{
			Result.y |= wave_shuffle((uint)Data, i) << i;
		}
        return Result;
#endif
    }
    
// Defines that match the expected signatures
#if __METAL_VERSION__ >= 210 && __METAL_MACOS__
    #define WaveOnce() ue4::wave_once()
    #define WaveGetLaneCount() ue4::wave_get_lane_count()
    #define WaveGetLaneIndex() ue4::wave_get_lane_index()
    #define WaveAnyTrue(Expr) ue4::wave_any(Expr, 0)
    #define WaveAllTrue(Expr) ue4::wave_all(Expr, 0)
    #define WaveAllEqual(Expr) ue4::wave_equal(Expr, 0)
    #define WaveBallot(Expr) ue4::wave_ballot64(Expr, 0)
    #define WaveAllSum(Expr) ue4::wave_sum(Expr, 0)
    #define WaveAllProduct(Expr) ue4::wave_product(Expr, 0)
    #define WaveAllBitAnd(Expr) ue4::wave_and(Expr, 0)
    #define WaveAllBitOr(Expr) ue4::wave_or(Expr, 0)
    #define WaveAllBitXor(Expr) ue4::wave_xor(Expr, 0)
    #define WaveAllMin(Expr) ue4::wave_min(Expr, 0)
    #define WaveAllMax(Expr) ue4::wave_max(Expr, 0)
    #define WavePrefixSum(Expr) ue4::wave_prefix_sum(Expr, 0, 0)
    #define WavePrefixProduct(Expr) ue4::wave_prefix_product(Expr, 0, 0)
#else
    #define WaveOnce() (get_thread_index_in_wave() == 0)
    #define WaveGetLaneCount() get_threads_per_wave()
    #define WaveGetLaneIndex() get_thread_index_in_wave()
    #define WaveAnyTrue(Expr) ue4::wave_any(Expr, get_threads_per_wave())
    #define WaveAllTrue(Expr) ue4::wave_all(Expr, get_threads_per_wave())
    #define WaveAllEqual(Expr) ue4::wave_equal(Expr, get_threads_per_wave())
    #define WaveBallot(Expr) ue4::wave_ballot64(Expr, get_threads_per_wave())
    #define WaveAllSum(Expr) ue4::wave_sum(Expr, get_threads_per_wave())
    #define WaveAllProduct(Expr) ue4::wave_product(Expr, get_threads_per_wave())
    #define WaveAllBitAnd(Expr) ue4::wave_and(Expr, get_threads_per_wave())
    #define WaveAllBitOr(Expr) ue4::wave_or(Expr, get_threads_per_wave())
    #define WaveAllBitXor(Expr) ue4::wave_xor(Expr, get_threads_per_wave())
    #define WaveAllMin(Expr) ue4::wave_min(Expr, get_threads_per_wave())
    #define WaveAllMax(Expr) ue4::wave_max(Expr, get_threads_per_wave())
    #define WavePrefixSum(Expr) ue4::wave_prefix_sum(Expr, get_thread_index_in_wave(), get_threads_per_wave())
    #define WavePrefixProduct(Expr) ue4::wave_prefix_product(Expr, get_thread_index_in_wave(), get_threads_per_wave())
#endif
#define WaveReadLaneAt(Expr, i) ue4::wave_read(Expr, i)
#define WaveReadFirstLane(Expr, i) ue4::wave_read_first(Expr)

}
#endif
	
namespace ue4
{
	template <typename T, size_t N>
	struct safe_array
	{
		inline thread T & __attribute__((always_inline)) operator[](size_t pos) thread {
			return __elems[min((uint)pos, (uint)N)];
		}
		inline constexpr const thread T & __attribute__((always_inline)) operator[](size_t pos) const thread {
			return __elems[min((uint)pos, (uint)N)];
		}
		
		inline device T & __attribute__((always_inline))  operator[](size_t pos) device {
			return __elems[min((uint)pos, (uint)N)];
		}
		inline constexpr const device T & __attribute__((always_inline)) operator[](size_t pos) const device {
			return __elems[min((uint)pos, (uint)N)];
		}
		
		inline constexpr const constant T & __attribute__((always_inline)) operator[](size_t pos) const constant {
			return __elems[min((uint)pos, (uint)N)];
		}
		
		inline threadgroup T & __attribute__((always_inline)) operator[](size_t pos) threadgroup {
			return __elems[min((uint)pos, (uint)N)];
		}
		inline constexpr const threadgroup T & __attribute__((always_inline)) operator[](size_t pos) const threadgroup {
			return __elems[min((uint)pos, (uint)N)];
		}
		
		T __elems[N + 1];
	};
}
	
#pragma clang diagnostic pop
