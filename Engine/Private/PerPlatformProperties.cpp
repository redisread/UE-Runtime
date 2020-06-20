// Copyright Epic Games, Inc. All Rights Reserved.

#include "PerPlatformProperties.h"
#include "Serialization/Archive.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "PlatformInfo.h"
#endif

IMPLEMENT_TYPE_LAYOUT(FFreezablePerPlatformFloat);
IMPLEMENT_TYPE_LAYOUT(FFreezablePerPlatformInt);

/** Serializer to cook out the most appropriate platform override */
template<typename StructType, typename ValueType, EName BasePropertyName>
ENGINE_API FArchive& operator<<(FArchive& Ar, TPerPlatformProperty<StructType, ValueType, BasePropertyName>& Property)
{
	bool bCooked = false;
#if WITH_EDITOR
	if (Ar.IsCooking())
	{
		bCooked = true;
		Ar << bCooked;
		// Save out platform override if it exists and Default otherwise
		const PlatformInfo::FPlatformInfo& PlatformInfo = Ar.CookingTarget()->GetPlatformInfo();
		ValueType Value = Property.GetValueForPlatformIdentifiers(PlatformInfo.PlatformGroupName, PlatformInfo.VanillaPlatformName);
		Ar << Value;
	}
	else
#endif
	{
		StructType* This = StaticCast<StructType*>(&Property);
		Ar << bCooked;
		Ar << This->Default;
#if WITH_EDITORONLY_DATA
		if (!bCooked)
		{
			Ar << This->PerPlatform;
		}
#endif
	}
	return Ar;
}

/** Serializer to cook out the most appropriate platform override */
template<typename StructType, typename ValueType, EName BasePropertyName>
ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<StructType, ValueType, BasePropertyName>& Property)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	bool bCooked = false;
#if WITH_EDITOR
	if (UnderlyingArchive.IsCooking())
	{
		bCooked = true;
		Record << SA_VALUE(TEXT("bCooked"), bCooked);
		// Save out platform override if it exists and Default otherwise
		ValueType Value = Property.GetValueForPlatformIdentifiers(UnderlyingArchive.CookingTarget()->GetPlatformInfo().PlatformGroupName);
		Record << SA_VALUE(TEXT("Value"), Value);
	}
	else
#endif
	{
		StructType* This = StaticCast<StructType*>(&Property);
		Record << SA_VALUE(TEXT("bCooked"), bCooked);
		Record << SA_VALUE(TEXT("Value"), This->Default);
#if WITH_EDITORONLY_DATA
		if (!bCooked)
		{
			Record << SA_VALUE(TEXT("PerPlatform"), This->PerPlatform);
		}
#endif
	}
}

template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FFreezablePerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FFreezablePerPlatformFloat, float, NAME_FloatProperty>&);

FString FPerPlatformInt::ToString() const
{
	FString Result = FString::FromInt(Default);

#if WITH_EDITORONLY_DATA
	TArray<FName> SortedPlatforms;
	PerPlatform.GetKeys(/*out*/ SortedPlatforms);
	SortedPlatforms.Sort(FNameLexicalLess());

	for (FName Platform : SortedPlatforms)
	{
		Result = FString::Printf(TEXT("%s, %s=%d"), *Result, *Platform.ToString(), PerPlatform.FindChecked(Platform));
	}
#endif

	return Result;
}

FString FFreezablePerPlatformInt::ToString() const
{
	return FPerPlatformInt(*this).ToString();
}
