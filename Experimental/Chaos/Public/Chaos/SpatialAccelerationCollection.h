// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Chaos/ISpatialAccelerationCollection.h"
#include "Chaos/Box.h"
#include "Chaos/Collision/SpatialAccelerationBroadPhase.h"
#include "Chaos/Collision/StatsData.h"
#include "GeometryParticlesfwd.h"

#include <tuple>

namespace Chaos
{

template <typename T>
void FreeObjHelper(T*& RawPtr)
{
	RawPtr = nullptr;
}

template <typename TPayloadType, typename T, int d>
struct CHAOS_API TSpatialAccelerationBucketEntry
{
	TUniquePtr<ISpatialAcceleration<TPayloadType, T, d>> Acceleration;
	uint16 TypeInnerIdx;

	void CopyFrom(TSpatialAccelerationBucketEntry<TPayloadType, T, d>& Src)
	{
		Acceleration = Src.Acceleration->Copy();
		TypeInnerIdx = Src.TypeInnerIdx;
	}

	TSpatialAccelerationBucketEntry() = default;
	TSpatialAccelerationBucketEntry(const TSpatialAccelerationBucketEntry<TPayloadType, T, d>& Other) = delete;
	TSpatialAccelerationBucketEntry(TSpatialAccelerationBucketEntry<TPayloadType, T, d>&& Other) = default;
	TSpatialAccelerationBucketEntry<TPayloadType, T, d>& operator=(TSpatialAccelerationBucketEntry<TPayloadType, T, d>&& Other) = default;
	TSpatialAccelerationBucketEntry<TPayloadType, T, d>& operator=(TSpatialAccelerationBucketEntry<TPayloadType, T, d>& Other) = delete;
};

template <typename TPayloadType, typename T, int d>
FChaosArchive& operator<<(FChaosArchive& Ar, TSpatialAccelerationBucketEntry<TPayloadType, T, d>& BucketEntry)
{
	Ar << BucketEntry.Acceleration;
	Ar << BucketEntry.TypeInnerIdx;
	return Ar;
}


template <typename TPayloadType, typename T, int d>
void FreeObjHelper(TSpatialAccelerationBucketEntry<TPayloadType, T, d >& BucketEntry)
{
	BucketEntry.Acceleration = nullptr;
}

template <typename T>
T CopyFromHelper(const T& Src)
{
	return Src;
}

template <typename TPayloadType, typename T, int d>
TSpatialAccelerationBucketEntry<TPayloadType, T, d> CopyFromHelper(const TSpatialAccelerationBucketEntry<TPayloadType, T, d >& Src)
{
	TSpatialAccelerationBucketEntry<TPayloadType, T, d> Copy;
	Copy.Acceleration = Src.Acceleration ? Src.Acceleration->Copy() : nullptr;
	Copy.TypeInnerIdx = Src.TypeInnerIdx;
	return Copy;
}


template <typename TObj>
struct CHAOS_API TSpatialCollectionBucket
{
	TSpatialCollectionBucket() = default;
	TSpatialCollectionBucket(const TSpatialCollectionBucket<TObj>& Other) = delete;
	TSpatialCollectionBucket(TSpatialCollectionBucket<TObj>&& Other) = default;
	TSpatialCollectionBucket<TObj>& operator=(const TSpatialCollectionBucket<TObj>& Other) = delete;
	TSpatialCollectionBucket<TObj>& operator=(TSpatialCollectionBucket<TObj>&& Other) = default;

	TArray<TObj> Objects;
	TArray<uint16> FreeIndices;

	int32 Add(TObj&& Obj)
	{
		uint16 Idx;
		if (FreeIndices.Num())
		{
			Idx = FreeIndices.Pop();
			Objects[Idx] = MoveTemp(Obj);
		}
		else
		{
			Idx = Objects.Add(MoveTemp(Obj));
		}

		return Idx;
	}

	void Remove(uint16 Idx)
	{
		if (Objects.Num() == Idx + 1)
		{
			Objects.Pop(false);
		}
		else
		{
			FreeObjHelper(Objects[Idx]);
			FreeIndices.Add(Idx);
		}
	}

	void CopyFrom(const TSpatialCollectionBucket& Src)
	{
		Objects.SetNum(Src.Objects.Num());
		for (int32 ObjIdx = 0; ObjIdx < Objects.Num(); ++ObjIdx)
		{
			Objects[ObjIdx] = CopyFromHelper(Src.Objects[ObjIdx]);
		}
		FreeIndices = Src.FreeIndices;
	}

};

template <typename TObj>
FChaosArchive& operator<<(FChaosArchive& Ar, TSpatialCollectionBucket<TObj>& Bucket)
{
	Ar << Bucket.Objects;
	Ar << Bucket.FreeIndices;
	return Ar;
}

template <typename... TRemaining>
struct CHAOS_API TSpatialTypeTuple
{
};

template <typename TAcceleration, typename... TRemaining>
struct CHAOS_API TSpatialTypeTuple<TAcceleration, TRemaining...>
{
	using FirstType = TAcceleration;
	TSpatialCollectionBucket<TAcceleration*> First;
	TSpatialTypeTuple<TRemaining...> Remaining;

	TSpatialTypeTuple() = default;

	TSpatialTypeTuple(const TSpatialTypeTuple<TAcceleration, TRemaining...>& Other) = delete;	//raw pointers need updating because buckets use a deep copy
};

template<int Idx, typename ... Rest>
struct CHAOS_API TSpatialTypeTupleGetter
{

};

template<int Idx, typename First, typename... Rest>
struct CHAOS_API TSpatialTypeTupleGetter<Idx, First, Rest...>
{
	static auto& Get(TSpatialTypeTuple<First, Rest...>& Types) { return TSpatialTypeTupleGetter<Idx - 1, Rest...>::Get(Types.Remaining); }
	static const auto& Get(const TSpatialTypeTuple<First, Rest...>& Types) { return TSpatialTypeTupleGetter<Idx - 1, Rest...>::Get(Types.Remaining); }
};

template<typename First, typename... Rest>
struct CHAOS_API TSpatialTypeTupleGetter<0, First, Rest...>
{
	static auto& Get(TSpatialTypeTuple<First, Rest...>& Types) { return Types.First; }
	static const auto& Get(const TSpatialTypeTuple<First, Rest...>& Types) { return Types.First; }
};

template <int Idx, typename First, typename... Rest>
auto& GetAccelerationsPerType(TSpatialTypeTuple<First, Rest...>& Types)
{
	return TSpatialTypeTupleGetter<Idx, First, Rest...>::Get(Types);
}

template <int Idx, typename First, typename... Rest>
const auto& GetAccelerationsPerType(const TSpatialTypeTuple<First, Rest...>& Types)
{
	return TSpatialTypeTupleGetter<Idx, First, Rest...>::Get(Types);
}

template <int TypeIdx, int NumTypes, typename Tuple, typename TPayloadType, typename T, int d>
struct TSpatialAccelerationCollectionHelper
{
	template <typename SQVisitor>
	static bool RaycastFast(const Tuple& Types, const TVector<T, d>& Start, FQueryFastData& CurData, SQVisitor& Visitor)
	{
		const auto& Accelerations = GetAccelerationsPerType<TypeIdx>(Types).Objects;
		for (const auto& Accelerator : Accelerations)
		{
			if (Accelerator && !Accelerator->RaycastFast(Start, CurData, Visitor))
			{
				return false;
			}
		}
		
		constexpr int NextType = TypeIdx + 1;
		if (NextType < NumTypes)
		{
			return TSpatialAccelerationCollectionHelper<NextType < NumTypes ? NextType : 0, NumTypes, Tuple, TPayloadType, T, d>::RaycastFast(Types, Start, CurData, Visitor);
		}

		return true;
	}

	template <typename SQVisitor>
	static bool SweepFast(const Tuple& Types, const TVector<T, d>& Start, FQueryFastData& CurData, const TVector<T, d> QueryHalfExtents, SQVisitor& Visitor)
	{
		const auto& Accelerations = GetAccelerationsPerType<TypeIdx>(Types).Objects;
		for (const auto& Accelerator : Accelerations)
		{
			if (Accelerator && !Accelerator->SweepFast(Start, CurData, QueryHalfExtents, Visitor))
			{
				return false;
			}
		}

		constexpr int NextType = TypeIdx + 1;
		if (NextType < NumTypes)
		{
			return TSpatialAccelerationCollectionHelper < NextType < NumTypes ? NextType : 0, NumTypes, Tuple, TPayloadType, T, d>::SweepFast(Types, Start, CurData, QueryHalfExtents, Visitor);
		}

		return true;
	}

	template <typename SQVisitor>
	static bool OverlapFast(const Tuple& Types, const TAABB<T, d> QueryBounds, SQVisitor& Visitor)
	{
		const auto& Accelerations = GetAccelerationsPerType<TypeIdx>(Types).Objects;
		for (const auto& Accelerator : Accelerations)
		{
			if (Accelerator && !Accelerator->OverlapFast(QueryBounds, Visitor))
			{
				return false;
			}
		}

		constexpr int NextType = TypeIdx + 1;
		if (NextType < NumTypes)
		{
			return TSpatialAccelerationCollectionHelper < NextType < NumTypes ? NextType : 0, NumTypes, Tuple, TPayloadType, T, d>::OverlapFast(Types, QueryBounds, Visitor);
		}

		return true;
	}

	static void GlobalObjects(const Tuple& Types, TArray<TPayloadBoundsElement<TPayloadType, T>>& ObjList)
	{
		const auto& Accelerations = GetAccelerationsPerType<TypeIdx>(Types).Objects;
		for (const auto& Accelerator : Accelerations)
		{
			if (Accelerator)
			{
				//todo: handle early out
				ObjList.Append(Accelerator->GlobalObjects());
			}
		}

		constexpr int NextType = TypeIdx + 1;
		if (NextType < NumTypes)
		{
			TSpatialAccelerationCollectionHelper < NextType < NumTypes ? NextType : 0, NumTypes, Tuple, TPayloadType, T, d>::GlobalObjects(Types, ObjList);
		}
	}

	static void SetNumFrom(const Tuple& TypesSrc, Tuple& TypesDest)
	{
		const auto& SrcAccelerations = GetAccelerationsPerType<TypeIdx>(TypesSrc).Objects;
		auto& DestAccelerations = GetAccelerationsPerType<TypeIdx>(TypesDest).Objects;
		DestAccelerations.SetNum(SrcAccelerations.Num());

		constexpr int NextType = TypeIdx + 1;
		if (NextType < NumTypes)
		{
			TSpatialAccelerationCollectionHelper < NextType < NumTypes ? NextType : 0, NumTypes, Tuple, TPayloadType, T, d>::SetNumFrom(TypesSrc, TypesDest);
		}
	}

	static uint16 FindTypeIdx(const Tuple& Types, SpatialAccelerationType Type)
	{
		using AccelType = typename std::remove_pointer<typename decltype(GetAccelerationsPerType<TypeIdx>(Types).Objects)::ElementType>::type;
		if (AccelType::StaticType == Type)
		{
			return TypeIdx;
		}

		constexpr int NextType = TypeIdx + 1;
		if (NextType < NumTypes)
		{
			return TSpatialAccelerationCollectionHelper < NextType < NumTypes ? NextType : 0, NumTypes, Tuple, TPayloadType, T, d>::FindTypeIdx(Types, Type);
		}
		else
		{
			return NumTypes;
		}
	}
};

template <typename SpatialAccelerationCollection>
typename TEnableIf<TIsSame<typename SpatialAccelerationCollection::TPayloadType, TAccelerationStructureHandle<FReal, 3>>::Value, void>::Type PBDComputeConstraintsLowLevel_Helper(FReal Dt, const SpatialAccelerationCollection& Accel, FSpatialAccelerationBroadPhase& BroadPhase, FNarrowPhase& NarrowPhase, FAsyncCollisionReceiver& Receiver, CollisionStats::FStatData& StatData)
{
	BroadPhase.ProduceOverlaps(Dt, Accel, NarrowPhase, Receiver, StatData);
}

template <typename SpatialAccelerationCollection>
typename TEnableIf<!TIsSame<typename SpatialAccelerationCollection::TPayloadType, TAccelerationStructureHandle<FReal, 3>>::Value, void>::Type PBDComputeConstraintsLowLevel_Helper(FReal Dt, const SpatialAccelerationCollection& Accel, FSpatialAccelerationBroadPhase& BroadPhase, FNarrowPhase& NarrowPhase, FAsyncCollisionReceiver& Receiver, CollisionStats::FStatData& StatData)
{
}

template<typename T_SPATIALACCELERATION>
void MoveToTOIHackImpl(FReal Dt, TTransientPBDRigidParticleHandle<FReal, 3>& Particle1, const T_SPATIALACCELERATION* SpatialAcceleration);

template <typename SpatialAccelerationCollection>
typename TEnableIf<TIsSame<typename SpatialAccelerationCollection::TPayloadType, TAccelerationStructureHandle<FReal, 3>>::Value, void>::Type CallMoveToTOIHack_Helper(FReal Dt, const SpatialAccelerationCollection& Accel, TTransientPBDRigidParticleHandle<FReal, 3>& Particle)
{
	MoveToTOIHackImpl(Dt, Particle, &Accel);
}

template <typename SpatialAccelerationCollection>
typename TEnableIf<!TIsSame<typename SpatialAccelerationCollection::TPayloadType, TAccelerationStructureHandle<FReal, 3>>::Value, void>::Type CallMoveToTOIHack_Helper(FReal Dt, const SpatialAccelerationCollection& Accel, TTransientPBDRigidParticleHandle<FReal, 3>& Particle)
{
}



template <typename ... TSpatialAccelerationTypes>
class CHAOS_API TSpatialAccelerationCollection : public
	ISpatialAccelerationCollection<typename std::tuple_element<0, std::tuple< TSpatialAccelerationTypes...>>::type::PayloadType,
	typename std::tuple_element<0, std::tuple< TSpatialAccelerationTypes...>>::type::TType,
	std::tuple_element<0, std::tuple< TSpatialAccelerationTypes...>>::type::D>
{
public:
	using FirstAccelerationType = typename std::tuple_element<0, std::tuple< TSpatialAccelerationTypes...>>::type;
	using TPayloadType = typename FirstAccelerationType::PayloadType;
	using T = typename FirstAccelerationType::TType;
	static constexpr int d = FirstAccelerationType::D;

	TSpatialAccelerationCollection()
	{
	}

	virtual FSpatialAccelerationIdx AddSubstructure(TUniquePtr<ISpatialAcceleration<TPayloadType, T, d>>&& Substructure, uint16 BucketIdx) override
	{
		check(BucketIdx < MaxBuckets);
		FSpatialAccelerationIdx Result;
		Result.Bucket = BucketIdx;
		
		ISpatialAcceleration<TPayloadType, T, d>* AccelPtr = Substructure.Get();
		TSpatialAccelerationBucketEntry<TPayloadType, T, d> BucketEntry;
		BucketEntry.Acceleration = MoveTemp(Substructure);
		
		const int32 TypeIdx = GetTypeIdx(AccelPtr);
		switch (TypeIdx)
		{
		case 0: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<0>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<0, std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		case 1: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<ClampedIdx(1)>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<ClampedIdx(1), std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		case 2: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<ClampedIdx(2)>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<ClampedIdx(2), std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		case 3: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<ClampedIdx(3)>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<ClampedIdx(3), std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		case 4: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<ClampedIdx(4)>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<ClampedIdx(4), std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		case 5: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<ClampedIdx(5)>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<ClampedIdx(5), std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		case 6: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<ClampedIdx(6)>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<ClampedIdx(6), std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		case 7: BucketEntry.TypeInnerIdx = GetAccelerationsPerType<ClampedIdx(7)>(Types).Add(&AccelPtr->template AsChecked<typename std::tuple_element<ClampedIdx(7), std::tuple<TSpatialAccelerationTypes...>>::type>()); break;
		}

		this->ActiveBucketsMask |= (1 << BucketIdx);

		Result.InnerIdx = Buckets[BucketIdx].Add(MoveTemp(BucketEntry));
		return Result;
	}

	virtual TUniquePtr <ISpatialAcceleration<TPayloadType, T, d>> RemoveSubstructure(FSpatialAccelerationIdx Idx) override
	{
		TUniquePtr <ISpatialAcceleration<TPayloadType, T, d>> Removed;
		{
			auto& BucketEntry = Buckets[Idx.Bucket].Objects[Idx.InnerIdx];
			const int32 TypeIdx = GetTypeIdx(BucketEntry.Acceleration.Get());
			switch (TypeIdx)
			{
			case 0: GetAccelerationsPerType<0>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			case 1: GetAccelerationsPerType<ClampedIdx(1)>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			case 2: GetAccelerationsPerType<ClampedIdx(2)>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			case 3: GetAccelerationsPerType<ClampedIdx(3)>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			case 4: GetAccelerationsPerType<ClampedIdx(4)>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			case 5: GetAccelerationsPerType<ClampedIdx(5)>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			case 6: GetAccelerationsPerType<ClampedIdx(6)>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			case 7: GetAccelerationsPerType<ClampedIdx(7)>(Types).Remove(BucketEntry.TypeInnerIdx); break;
			}
			Removed = MoveTemp(BucketEntry.Acceleration);
		}
		Buckets[Idx.Bucket].Remove(Idx.InnerIdx);
		if (Buckets[Idx.Bucket].Objects.Num() == 0)
		{
			this->ActiveBucketsMask &= ~(1 << Idx.Bucket);
		}
		return Removed;
	}

	virtual ISpatialAcceleration<TPayloadType, T, d>* GetSubstructure(FSpatialAccelerationIdx Idx) override
	{
		check(Idx.Bucket < MaxBuckets);
		if (Buckets[Idx.Bucket].Objects.Num() > 0)
		{
			return Buckets[Idx.Bucket].Objects[Idx.InnerIdx].Acceleration.Get();
		}

		return nullptr;
	}

	virtual void Raycast(const TVector<T, d>& Start, const TVector<T, d>& Dir, const T Length, ISpatialVisitor<TPayloadType, T>& Visitor) const override
	{
		TSpatialVisitor<TPayloadType, T> ProxyVisitor(Visitor);
		Raycast(Start, Dir, Length, ProxyVisitor);
	}

	template <typename SQVisitor>
	void Raycast(const TVector<T, d>& Start, const TVector<T, d>& Dir, const T Length, SQVisitor& Visitor) const
	{
		FQueryFastData QueryFastData(Dir, Length);
		TSpatialAccelerationCollectionHelper<0, NumTypes, decltype(Types), TPayloadType, T, d>::RaycastFast(Types, Start, QueryFastData, Visitor);
	}

	void Sweep(const TVector<T, d>& Start, const TVector<T, d>& Dir, const T Length, const TVector<T, d> QueryHalfExtents, ISpatialVisitor<TPayloadType, T>& Visitor) const override
	{
		TSpatialVisitor<TPayloadType, T> ProxyVisitor(Visitor);
		Sweep(Start, Dir, Length, QueryHalfExtents, ProxyVisitor);
	}

	template <typename SQVisitor>
	void Sweep(const TVector<T, d>& Start, const TVector<T, d>& Dir, const T Length, const TVector<T, d> QueryHalfExtents, SQVisitor& Visitor) const
	{
		FQueryFastData QueryFastData(Dir, Length);
		TSpatialAccelerationCollectionHelper<0, NumTypes, decltype(Types), TPayloadType, T, d>::SweepFast(Types, Start, QueryFastData, QueryHalfExtents, Visitor);
	}

	virtual void Overlap(const TAABB<T, d>& QueryBounds, ISpatialVisitor<TPayloadType, T>& Visitor) const override
	{
		TSpatialVisitor<TPayloadType, T> ProxyVisitor(Visitor);
		Overlap(QueryBounds, ProxyVisitor);
	}

	template <typename SQVisitor>
	void Overlap(const TAABB<T, 3>& QueryBounds, SQVisitor& Visitor) const
	{
		TSpatialAccelerationCollectionHelper<0, NumTypes, decltype(Types), TPayloadType, T, d>::OverlapFast(Types, QueryBounds, Visitor);
	}

	TArray<TPayloadBoundsElement<TPayloadType, T>> GlobalObjects() const
	{
		TArray<TPayloadBoundsElement<TPayloadType, T>> ObjList;
		TSpatialAccelerationCollectionHelper<0, NumTypes, decltype(Types), TPayloadType, T, d>::GlobalObjects(Types, ObjList);
		return ObjList;
	}

	virtual TArray<FSpatialAccelerationIdx> GetAllSpatialIndices() const override
	{
		uint16 BucketIdx = 0;
		TArray<FSpatialAccelerationIdx> Indices;
		for (const auto& Bucket : Buckets)
		{
			uint16 InnerIdx = 0;
			for (const auto& Entry : Bucket.Objects)
			{
				if (Entry.Acceleration)
				{
					Indices.Add(FSpatialAccelerationIdx{ BucketIdx, InnerIdx });
				}
				++InnerIdx;
			}
			++BucketIdx;
		}
		return Indices;
	}

	virtual void RemoveElementFrom(const TPayloadType& Payload, FSpatialAccelerationIdx SpatialIdx) override
	{
		const uint16 UseBucket = ((1 << SpatialIdx.Bucket) & this->ActiveBucketsMask) ? SpatialIdx.Bucket : 0;
		Buckets[UseBucket].Objects[SpatialIdx.InnerIdx].Acceleration->RemoveElement(Payload);
	}

	virtual void UpdateElementIn(const TPayloadType& Payload, const TAABB<T, d>& NewBounds, bool bHasBounds, FSpatialAccelerationIdx SpatialIdx)
	{
		const uint16 UseBucket = ((1 << SpatialIdx.Bucket) & this->ActiveBucketsMask) ? SpatialIdx.Bucket : 0;
		Buckets[UseBucket].Objects[SpatialIdx.InnerIdx].Acceleration->UpdateElement(Payload, NewBounds, bHasBounds);
	}

	virtual TUniquePtr<ISpatialAcceleration<TPayloadType, T, d>> Copy() const
	{
		return TUniquePtr<ISpatialAcceleration<TPayloadType, T, d>>(new TSpatialAccelerationCollection<TSpatialAccelerationTypes...>(*this));
	}

	virtual void PBDComputeConstraintsLowLevel(T Dt, FSpatialAccelerationBroadPhase& BroadPhase, FNarrowPhase& NarrowPhase, FAsyncCollisionReceiver& Receiver, CollisionStats::FStatData& StatData) const override
	{
		PBDComputeConstraintsLowLevel_Helper(Dt, *this, BroadPhase, NarrowPhase, Receiver, StatData);
	}

	virtual void CallMoveToTOIHack(FReal Dt, TTransientPBDRigidParticleHandle<FReal, 3>& Particle) const override
	{
		CallMoveToTOIHack_Helper(Dt, *this, Particle);
	}

	virtual void Serialize(FChaosArchive& Ar)
	{
		//todo: let user serialize out bucket params

		//serialize out sub structures
		for (int BucketIdx = 0; BucketIdx < MaxBuckets; ++BucketIdx)
		{
			Ar << Buckets[BucketIdx];

			if (Ar.IsLoading())
			{
				for (const TSpatialAccelerationBucketEntry<TPayloadType, T, d>& Entry : Buckets[BucketIdx].Objects)
				{
					ISpatialAcceleration<TPayloadType, T, d>* RawPtr = Entry.Acceleration.Get();
					const int32 TypeIdx = GetTypeIdx(RawPtr);
					switch (TypeIdx)
					{
					case 0: GetAccelerationsPerType<0>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<0, std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					case 1: GetAccelerationsPerType<ClampedIdx(1)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(1), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					case 2: GetAccelerationsPerType<ClampedIdx(2)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(2), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					case 3: GetAccelerationsPerType<ClampedIdx(3)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(3), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					case 4: GetAccelerationsPerType<ClampedIdx(4)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(4), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					case 5: GetAccelerationsPerType<ClampedIdx(5)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(5), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					case 6: GetAccelerationsPerType<ClampedIdx(6)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(6), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					case 7: GetAccelerationsPerType<ClampedIdx(7)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(7), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
					}
				}
			}
		}
	}

private:

	TSpatialAccelerationCollection(const TSpatialAccelerationCollection<TSpatialAccelerationTypes...>& Other)
	{
		//need to make a deep copy of the unique ptrs and then update raw ptrs
		TSpatialAccelerationCollectionHelper<0, NumTypes, decltype(Types), TPayloadType, T, d>::SetNumFrom(Other.Types, Types);
		this->ActiveBucketsMask = Other.ActiveBucketsMask;

		for (int BucketIdx = 0; BucketIdx < MaxBuckets; ++BucketIdx)
		{
			Buckets[BucketIdx].CopyFrom(Other.Buckets[BucketIdx]);
			bool bFirst = true;
			for (const TSpatialAccelerationBucketEntry<TPayloadType, T, d>& Entry : Buckets[BucketIdx].Objects)
			{
				ISpatialAcceleration<TPayloadType, T, d>* RawPtr = Entry.Acceleration.Get();
				const int32 TypeIdx = GetTypeIdx(RawPtr);
				switch (TypeIdx)
				{
				case 0: GetAccelerationsPerType<0>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<0, std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				case 1: GetAccelerationsPerType<ClampedIdx(1)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(1), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				case 2: GetAccelerationsPerType<ClampedIdx(2)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(2), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				case 3: GetAccelerationsPerType<ClampedIdx(3)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(3), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				case 4: GetAccelerationsPerType<ClampedIdx(4)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(4), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				case 5: GetAccelerationsPerType<ClampedIdx(5)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(5), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				case 6: GetAccelerationsPerType<ClampedIdx(6)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(6), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				case 7: GetAccelerationsPerType<ClampedIdx(7)>(Types).Objects[Entry.TypeInnerIdx] = &RawPtr->template AsChecked<typename std::tuple_element<ClampedIdx(7), std::tuple<TSpatialAccelerationTypes...>>::type>(); break;
				}
			}
		}
	}
	
	static constexpr uint32 ClampedIdx(uint32 Idx)
	{
		return Idx < NumTypes ? Idx : 0;
	}

	static uint8 GetTypeIdx(const ISpatialAcceleration<TPayloadType, T, d>* Accel)
	{
		if (Accel->template As<typename std::tuple_element<0, std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 0;
		}
		
		if (Accel->template As<typename std::tuple_element<ClampedIdx(1), std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 1;
		}

		if (Accel->template As<typename std::tuple_element<ClampedIdx(2), std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 2;
		}

		if (Accel->template As<typename std::tuple_element<ClampedIdx(3), std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 3;
		}

		if (Accel->template As<typename std::tuple_element<ClampedIdx(4), std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 4;
		}

		if (Accel->template As<typename std::tuple_element<ClampedIdx(5), std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 5;
		}

		if (Accel->template As<typename std::tuple_element<ClampedIdx(6), std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 6;
		}

		if (Accel->template As<typename std::tuple_element<ClampedIdx(7), std::tuple< TSpatialAccelerationTypes...>>::type> ())
		{
			return 7;
		}

		check(false);	//Did you instantiate the collection with the right template arguments?
		return 0;
	}

	static constexpr uint16 MaxBuckets = 8;
	TSpatialCollectionBucket<TSpatialAccelerationBucketEntry<TPayloadType, T, d>> Buckets[MaxBuckets];
	TSpatialTypeTuple< TSpatialAccelerationTypes...> Types;
	static constexpr uint32 NumTypes = sizeof...(TSpatialAccelerationTypes);
};


}
