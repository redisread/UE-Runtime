// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
#include "PhysXPublic.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsPhysX/ImmediatePhysicsLinearBlockAllocator_PhysX.h"

namespace ImmediatePhysics_PhysX
{

	/** TODO: Use a smarter memory allocator */
	struct FCacheAllocator : public PxCacheAllocator
	{
		FCacheAllocator() : External(0) {}
		PxU8* allocateCacheData(const PxU32 ByteSize) override
		{
			return BlockAllocator[External].Alloc(ByteSize);
		}

		void Reset()
		{
#if PERSISTENT_CONTACT_PAIRS
			External = 1 - External;	//flip buffer so we maintain cache for 1 extra step
#endif
			BlockAllocator[External].Reset();
		}

		FLinearBlockAllocator BlockAllocator[2];
		int32 External;
	};

}

#endif // WITH_PHYSX