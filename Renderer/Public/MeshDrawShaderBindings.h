// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshDrawShaderBindings.h: 
=============================================================================*/

#pragma once

// Whether to assert when mesh command shader bindings were not set by the pass processor.
// Enabled by default in debug
#define VALIDATE_MESH_COMMAND_BINDINGS DO_GUARD_SLOW

/** Stores the number of each resource type that will need to be bound to a single shader, computed during shader reflection. */
class FMeshDrawShaderBindingsLayout
{
protected:
	const FShaderParameterMapInfo& ParameterMapInfo;

public:

	FMeshDrawShaderBindingsLayout(const TShaderRef<FShader>& Shader) :
		ParameterMapInfo(Shader->ParameterMapInfo)
	{
		check(Shader.IsValid());
	}

#if DO_GUARD_SLOW
	const FShaderParameterMapInfo& GetParameterMapInfo()
	{
		return ParameterMapInfo;
	}
#endif

	bool operator==(const FMeshDrawShaderBindingsLayout& Rhs) const
	{
		return ParameterMapInfo == Rhs.ParameterMapInfo;
	}

	inline uint32 GetLooseDataSizeBytes() const
	{
		uint32 DataSize = 0;
		for (int32 LooseBufferIndex = 0; LooseBufferIndex < ParameterMapInfo.LooseParameterBuffers.Num(); LooseBufferIndex++)
		{
			DataSize += ParameterMapInfo.LooseParameterBuffers[LooseBufferIndex].Size;
		}
		return DataSize;
	}

	inline uint32 GetDataSizeBytes() const
	{
		uint32 DataSize = sizeof(void*) * 
			(ParameterMapInfo.UniformBuffers.Num() 
			+ ParameterMapInfo.TextureSamplers.Num() 
			+ ParameterMapInfo.SRVs.Num());

		// Allocate a bit for each SRV tracking whether it is a FRHITexture* or FRHIShaderResourceView*
		DataSize += FMath::DivideAndRoundUp(ParameterMapInfo.SRVs.Num(), 8);

		DataSize += GetLooseDataSizeBytes();

		// Align to pointer size so subsequent packed shader bindings will have their pointers aligned
		return Align(DataSize, sizeof(void*));
	}

protected:

	// Note: pointers first in layout, so they stay aligned
	inline uint32 GetUniformBufferOffset() const
	{
		return 0;
	}

	inline uint32 GetSamplerOffset() const
	{
		return ParameterMapInfo.UniformBuffers.Num() * sizeof(FRHIUniformBuffer*);
	}

	inline uint32 GetSRVOffset() const
	{
		return GetSamplerOffset() 
			+ ParameterMapInfo.TextureSamplers.Num() * sizeof(FRHISamplerState*);
	}

	inline uint32 GetSRVTypeOffset() const
	{
		return GetSRVOffset() 
			+ ParameterMapInfo.SRVs.Num() * sizeof(FRHIShaderResourceView*);
	}

	inline uint32 GetLooseDataOffset() const
	{
		return GetSRVTypeOffset()
			+ FMath::DivideAndRoundUp(ParameterMapInfo.SRVs.Num(), 8);
	}

	friend class FMeshDrawShaderBindings;
};

class FMeshDrawSingleShaderBindings : public FMeshDrawShaderBindingsLayout
{
public:
	FMeshDrawSingleShaderBindings(const FMeshDrawShaderBindingsLayout& InLayout, uint8* InData) :
		FMeshDrawShaderBindingsLayout(InLayout)
	{
		Data = InData;
	}

	template<typename UniformBufferStructType>
	void Add(const TShaderUniformBufferParameter<UniformBufferStructType>& Parameter, const TUniformBufferRef<UniformBufferStructType>& Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value.GetReference(), TEXT("Attempted to set null uniform buffer for type %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName());
			checkfSlow(Value.GetReference()->IsValid(), TEXT("Attempted to set already deleted uniform buffer for type %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName());
			WriteBindingUniformBuffer(Value.GetReference(), Parameter.GetBaseIndex());
		}
	}

	template<typename UniformBufferStructType>
	void Add(const TShaderUniformBufferParameter<UniformBufferStructType>& Parameter, const TUniformBuffer<UniformBufferStructType>& Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value.GetUniformBufferRHI(), TEXT("Attempted to set null uniform buffer for type %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName());
			checkfSlow(Value.GetUniformBufferRHI()->IsValid(), TEXT("Attempted to set already deleted uniform buffer for type %s"), UniformBufferStructType::StaticStructMetadata.GetStructTypeName());
			WriteBindingUniformBuffer(Value.GetUniformBufferRHI(), Parameter.GetBaseIndex());
		}
	}

	void Add(const FShaderUniformBufferParameter& Parameter, const FRHIUniformBuffer* Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value, TEXT("Attempted to set null uniform buffer"));
			checkfSlow(Value->IsValid(), TEXT("Attempted to set already deleted uniform buffer of type %s"), *Value->GetLayout().GetDebugName());
			WriteBindingUniformBuffer(Value, Parameter.GetBaseIndex());
		}
	}

	void Add(FShaderResourceParameter Parameter, FRHIShaderResourceView* Value)
	{
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			checkf(Value, TEXT("Attempted to set null SRV on slot %u"), Parameter.GetBaseIndex());
			checkfSlow(Value->IsValid(), TEXT("Attempted to set already deleted SRV on slot %u"), Parameter.GetBaseIndex());
			WriteBindingSRV(Value, Parameter.GetBaseIndex());
		}
	}

	void AddTexture(
		FShaderResourceParameter TextureParameter,
		FShaderResourceParameter SamplerParameter,
		FRHISamplerState* SamplerStateRHI,
		FRHITexture* TextureRHI)
	{
		checkfSlow(TextureParameter.IsInitialized(), TEXT("Parameter was not serialized"));
		checkfSlow(SamplerParameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (TextureParameter.IsBound())
		{
			checkf(TextureRHI, TEXT("Attempted to set null Texture on slot %u"), TextureParameter.GetBaseIndex());
			WriteBindingTexture(TextureRHI, TextureParameter.GetBaseIndex());
		}

		if (SamplerParameter.IsBound())
		{
			checkf(SamplerStateRHI, TEXT("Attempted to set null Sampler on slot %u"), SamplerParameter.GetBaseIndex());
			WriteBindingSampler(SamplerStateRHI, SamplerParameter.GetBaseIndex());
		}
	}

	template<class ParameterType>
	void Add(FShaderParameter Parameter, const ParameterType& Value)
	{
		static_assert(!TIsPointer<ParameterType>::Value, "Passing by pointer is not valid.");
		checkfSlow(Parameter.IsInitialized(), TEXT("Parameter was not serialized"));

		if (Parameter.IsBound())
		{
			bool bFoundParameter = false;
			uint8* LooseDataOffset = GetLooseDataStart();

			for (int32 LooseBufferIndex = 0; LooseBufferIndex < ParameterMapInfo.LooseParameterBuffers.Num(); LooseBufferIndex++)
			{
				const FShaderLooseParameterBufferInfo& LooseParameterBuffer = ParameterMapInfo.LooseParameterBuffers[LooseBufferIndex];

				if (LooseParameterBuffer.BaseIndex == Parameter.GetBufferIndex())
				{
					for (int32 LooseParameterIndex = 0; LooseParameterIndex < LooseParameterBuffer.Parameters.Num(); LooseParameterIndex++)
					{
						FShaderParameterInfo LooseParameter = LooseParameterBuffer.Parameters[LooseParameterIndex];

						if (Parameter.GetBaseIndex() == LooseParameter.BaseIndex)
						{
							checkSlow(Parameter.GetNumBytes() == LooseParameter.Size);
							ensureMsgf(sizeof(ParameterType) == Parameter.GetNumBytes(), TEXT("Attempted to set fewer bytes than the shader required.  Setting %u bytes on loose parameter at BaseIndex %u, Size %u.  This can cause GPU hangs, depending on usage."), sizeof(ParameterType), Parameter.GetBaseIndex(), Parameter.GetNumBytes());
							const int32 NumBytesToSet = FMath::Min<int32>(sizeof(ParameterType), Parameter.GetNumBytes());
							FMemory::Memcpy(LooseDataOffset, &Value, NumBytesToSet);
							const int32 NumBytesToClear = FMath::Min<int32>(0, Parameter.GetNumBytes() - NumBytesToSet);
							FMemory::Memset(LooseDataOffset + NumBytesToSet, 0x00, NumBytesToClear);
							bFoundParameter = true;
							break;
						}

						LooseDataOffset += LooseParameter.Size;
					}
					break;
				}

				LooseDataOffset += LooseParameterBuffer.Size;
			}

			checkfSlow(bFoundParameter, TEXT("Attempted to set loose parameter at BaseIndex %u, Size %u which was never in the shader's parameter map."), Parameter.GetBaseIndex(), Parameter.GetNumBytes());
		}
	}

private:
	uint8* Data;

	inline const FRHIUniformBuffer** GetUniformBufferStart() const
	{
		return (const FRHIUniformBuffer**)(Data + GetUniformBufferOffset());
	}

	inline FRHISamplerState** GetSamplerStart() const
	{
		uint8* SamplerDataStart = Data + GetSamplerOffset();
		return (FRHISamplerState**)SamplerDataStart;
	}

	inline FRHIResource** GetSRVStart() const
	{
		uint8* SRVDataStart = Data + GetSRVOffset();
		checkfSlow(Align(*SRVDataStart, sizeof(void*)) == *SRVDataStart, TEXT("FMeshDrawSingleShaderBindings should have been laid out so that stored pointers are aligned"));
		return (FRHIResource**)SRVDataStart;
	}

	inline uint8* GetSRVTypeStart() const
	{
		uint8* SRVTypeDataStart = Data + GetSRVTypeOffset();
		return SRVTypeDataStart;
	}

	inline uint8* GetLooseDataStart() const
	{
		uint8* LooseDataStart = Data + GetLooseDataOffset();
		return LooseDataStart;
	}
	
	template<typename ElementType>
	inline int FindSortedArrayBaseIndex(const TConstArrayView<ElementType>& Array, uint32 BaseIndex)
	{
		int S0 = 0;
		int S1 = Array.Num() - 1;
		while (S0 < S1)
		{
			int MED = (S0 + S1) / 2;
			if (Array[MED].BaseIndex >= BaseIndex)
			{
				S1 = MED;
			}
			else
			{
				S0 = (S0 == MED) ? S0 + 1 : MED;
			}
		}

		if (S0 == S1)
		{
			if (Array[S0].BaseIndex == BaseIndex)
			{
				return S0;
			}
		}

		return -1;
	}

	inline void WriteBindingUniformBuffer(const FRHIUniformBuffer* Value, uint32 BaseIndex)
	{
		int32 FoundIndex = FindSortedArrayBaseIndex(MakeArrayView(ParameterMapInfo.UniformBuffers), BaseIndex);
		if (FoundIndex >= 0)
		{
#if VALIDATE_UNIFORM_BUFFER_LIFETIME
			if (GetUniformBufferStart()[FoundIndex])
			{
				GetUniformBufferStart()[FoundIndex]->NumMeshCommandReferencesForDebugging--;
				check(GetUniformBufferStart()[FoundIndex]->NumMeshCommandReferencesForDebugging >= 0);
			}
			Value->NumMeshCommandReferencesForDebugging++;
#endif

			GetUniformBufferStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set a uniform buffer at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	inline void WriteBindingSampler(FRHISamplerState* Value, uint32 BaseIndex)
	{
		int32 FoundIndex = FindSortedArrayBaseIndex(MakeArrayView(ParameterMapInfo.TextureSamplers), BaseIndex);
		if (FoundIndex >= 0)
		{
			GetSamplerStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set a texture sampler at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	inline void WriteBindingSRV(FRHIShaderResourceView* Value, uint32 BaseIndex)
	{
		int32 FoundIndex = FindSortedArrayBaseIndex(MakeArrayView(ParameterMapInfo.SRVs), BaseIndex);
		if (FoundIndex >= 0)
		{
			uint32 TypeByteIndex = FoundIndex / 8;
			uint32 TypeBitIndex = FoundIndex % 8;
			GetSRVTypeStart()[TypeByteIndex] |= 1 << TypeBitIndex;
			GetSRVStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set SRV at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	inline void WriteBindingTexture(FRHITexture* Value, uint32 BaseIndex)
	{
		int32 FoundIndex = FindSortedArrayBaseIndex(MakeArrayView(ParameterMapInfo.SRVs), BaseIndex);
		if (FoundIndex >= 0)
		{
			GetSRVStart()[FoundIndex] = Value;
		}

		checkfSlow(FoundIndex >= 0, TEXT("Attempted to set Texture at BaseIndex %u which was never in the shader's parameter map."), BaseIndex);
	}

	friend class FMeshDrawShaderBindings;
};
