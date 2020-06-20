// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"

#define LOCTEXT_NAMESPACE "AnimNode_Inertialization"


TAutoConsoleVariable<int32> CVarAnimInertializationEnable(TEXT("a.AnimNode.Inertialization.Enable"), 1, TEXT("Enable / Disable Inertialization"));
TAutoConsoleVariable<int32> CVarAnimInertializationIgnoreVelocity(TEXT("a.AnimNode.Inertialization.IgnoreVelocity"), 0, TEXT("Ignore velocity information during Inertialization (effectively reverting to a quintic diff blend)"));
TAutoConsoleVariable<int32> CVarAnimInertializationIgnoreDeficit(TEXT("a.AnimNode.Inertialization.IgnoreDeficit"), 0, TEXT("Ignore inertialization time deficit caused by interruptions"));


static constexpr int32 INERTIALIZATION_MAX_POSE_SNAPSHOTS = 2;
static constexpr float INERTIALIZATION_TIME_EPSILON = 1.0e-7f;


FAnimNode_Inertialization::FAnimNode_Inertialization()
	: DeltaTime(0.0f)
	, RequestedDuration(-1.0f)
	, TeleportType(ETeleportType::None)
	, InertializationState(EInertializationState::Inactive)
	, InertializationElapsedTime(0.0f)
	, InertializationDuration(0.0f)
	, InertializationDeficit(0.0f)
{
}


void FAnimNode_Inertialization::RequestInertialization(float Duration)
{
	if (RequestedDuration < 0.0f || Duration < RequestedDuration)
	{
		RequestedDuration = Duration;
	}
}

/*static*/ void FAnimNode_Inertialization::LogRequestError(const FAnimationUpdateContext& Context, const FPoseLinkBase& RequesterPoseLink)
{
#if WITH_EDITORONLY_DATA	
	UAnimBlueprint* AnimBlueprint = Context.AnimInstanceProxy->GetAnimBlueprint();
	UAnimBlueprintGeneratedClass* AnimClass = AnimBlueprint ? AnimBlueprint->GetAnimBlueprintGeneratedClass() : nullptr;
	const UObject* RequesterNode = AnimClass ? AnimClass->GetVisualNodeFromNodePropertyIndex(RequesterPoseLink.SourceLinkID) : nullptr;

	FText Message = FText::Format(LOCTEXT("InertializationRequestError", "No Inertialization node found for request from '{0}'. Add an Inertialization node after this request."),
		FText::FromString(GetPathNameSafe(RequesterNode)));
	Context.LogMessage(EMessageSeverity::Error, Message);
#endif
}

void FAnimNode_Inertialization::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);

	PoseSnapshots.Empty(INERTIALIZATION_MAX_POSE_SNAPSHOTS);

	DeltaTime = 0.0f;

	TeleportType = ETeleportType::None;

	Deactivate();

	InertializationPoseDiff.Reset();
}


void FAnimNode_Inertialization::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread);

	FAnimNode_Base::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);
}


void FAnimNode_Inertialization::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);

	FScopedAnimNodeTracker Tracked = Context.TrackAncestor(this);

	Source.Update(Context);

	// Accumulate delta time between calls to Evaluate_AnyThread
	DeltaTime += Context.GetDeltaTime();
}


void FAnimNode_Inertialization::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

	Source.Evaluate(Output);

	// Extract any pending inertialization request
	const float Duration = ConsumeInertializationRequest(Output);

	// Disable inertialization if requested (for testing / debugging)
	if (!CVarAnimInertializationEnable.GetValueOnAnyThread())
	{
		Deactivate();

		// Clear the pose history
		PoseSnapshots.Reset();

		// Reset the cached time accumulator and teleport state
		DeltaTime = 0.0f;
		TeleportType = ETeleportType::None;

		return;
	}

	// Update the inertialization state if a new inertialization request is pending
	if (Duration > SMALL_NUMBER && PoseSnapshots.Num() > 0)
	{
		float AppliedDeficit = 0.0f;
		if (InertializationState == EInertializationState::Active)
		{
			// An active inertialization is being interrupted. Keep track of the lost inertialization time
			// and reduce future durations if interruptions continue. Without this mitigation, 
			// repeated interruptions will lead to a degenerate pose because the pose target is unstable.
			bool bApplyDeficit = InertializationDeficit > 0.0f && !CVarAnimInertializationIgnoreDeficit.GetValueOnAnyThread();
			InertializationDeficit = InertializationDuration - InertializationElapsedTime;
			AppliedDeficit = bApplyDeficit ? FMath::Min(Duration, InertializationDeficit) : 0.0f;
		}
		InertializationState = EInertializationState::Pending;
		InertializationElapsedTime = 0.0f;
		InertializationDuration = Duration - AppliedDeficit;
	}

	// Update the inertialization timer
	if (InertializationState != EInertializationState::Inactive)
	{
		InertializationElapsedTime += DeltaTime;
		if (InertializationElapsedTime >= InertializationDuration)
		{
			Deactivate();
		}
		else
		{
			// Pay down the accumulated deficit caused by interruptions
			InertializationDeficit -= FMath::Min(InertializationDeficit, DeltaTime);
		}
	}

	const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	// Automatically detect teleports... note that we do the teleport distance check against the root bone's location (world space) rather
	// than the mesh component's location because we still want to inertialize instances where the skeletal mesh component has been moved
	// while simultaneously counter-moving the root bone (as is the case when mounting and dismounting vehicles for example)

	// bool bTeleported = (TeleportType != ETeleportType::None);
	// Ignore TeleportType for now. See UE-78594.
	bool bTeleported = false;
	const float TeleportDistanceThreshold = Output.AnimInstanceProxy->GetSkelMeshComponent()->GetTeleportDistanceThreshold();
	if (!bTeleported && PoseSnapshots.Num() > 0 && TeleportDistanceThreshold > 0.0f)
	{
		const FInertializationPose& Prev = PoseSnapshots[PoseSnapshots.Num() - 1];

		const FVector RootWorldSpaceLocation = ComponentTransform.TransformPosition(Output.Pose[FCompactPoseBoneIndex(0)].GetTranslation());
		const FVector PrevRootWorldSpaceLocation = Prev.ComponentTransform.TransformPosition(Prev.BoneTransforms[0].GetTranslation());
		if (FVector::DistSquared(RootWorldSpaceLocation, PrevRootWorldSpaceLocation) > FMath::Square(TeleportDistanceThreshold))
		{
			bTeleported = true;
		}
	}
	if (bTeleported)
	{
		// Cancel inertialization requests during teleports
		if (InertializationState == EInertializationState::Pending)
		{
			Deactivate();
		}

		// Clear the time accumulator during teleports (in order to invalidate any recorded velocities during the teleport)
		DeltaTime = 0.0f;
	}

	// Ignore the inertialization velocities if requested (for testing / debugging)
	if (CVarAnimInertializationIgnoreVelocity.GetValueOnAnyThread())
	{
		// Clear the time accumulator (so as to invalidate any recorded velocities)
		DeltaTime = 0.0f;
	}

	// Inertialize the pose
	if (InertializationState == EInertializationState::Pending)
	{
		StartInertialization(Output,
			PoseSnapshots[PoseSnapshots.Num() - 1],
			PoseSnapshots[FMath::Max(PoseSnapshots.Num() - 2, 0)],
			InertializationDuration,
			InertializationPoseDiff);
		InertializationState = EInertializationState::Active;
	}
	if (InertializationState == EInertializationState::Active)
	{
		ApplyInertialization(Output, InertializationPoseDiff, InertializationElapsedTime, InertializationDuration);
	}

	// Get the parent actor attachment information (to detect and counteract discontinuities when changing parents)
	FName AttachParentName = NAME_None;
	if (AActor* Owner = Output.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner())
	{
		if (AActor* AttachParentActor = Owner->GetAttachParentActor())
		{
			AttachParentName = AttachParentActor->GetFName();
		}
	}

	// Capture the pose snapshot for this frame
	if (PoseSnapshots.Num() < INERTIALIZATION_MAX_POSE_SNAPSHOTS)
	{
		// Add the pose to the end of the buffer
		FInertializationPose& Snapshot = PoseSnapshots.AddDefaulted_GetRef();
		Snapshot.InitFrom(Output.Pose, Output.Curve, ComponentTransform, AttachParentName, DeltaTime);
	}
	else
	{
		// Bubble the old poses forward in the buffer (using swaps to avoid allocations and copies)
		for (int32 i = 0; i < INERTIALIZATION_MAX_POSE_SNAPSHOTS - 1; ++i)
		{
			PoseSnapshots.Swap(i, i + 1);
		}

		// Overwrite the (now irrelevant) pose in the last slot with the new post snapshot
		// (thereby avoiding the reallocation costs we would have incurred had we simply added a new pose at the end)
		FInertializationPose& Snapshot = PoseSnapshots[INERTIALIZATION_MAX_POSE_SNAPSHOTS - 1];
		Snapshot.InitFrom(Output.Pose, Output.Curve, ComponentTransform, AttachParentName, DeltaTime);
	}

	// Reset the time accumulator and teleport state
	DeltaTime = 0.0f;
	TeleportType = ETeleportType::None;

	TRACE_ANIM_NODE_VALUE(Output, TEXT("State"), *UEnum::GetValueAsString(InertializationState));
	TRACE_ANIM_NODE_VALUE(Output, TEXT("Elapsed Time"), InertializationElapsedTime);
	TRACE_ANIM_NODE_VALUE(Output, TEXT("Duration"), InertializationDuration);
	TRACE_ANIM_NODE_VALUE(Output, TEXT("Normalized Time"), InertializationDuration > KINDA_SMALL_NUMBER ? (InertializationElapsedTime / InertializationDuration) : 0.0f);
}


void FAnimNode_Inertialization::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData);

	FString DebugLine = DebugData.GetNodeName(this);

	if (InertializationDuration > KINDA_SMALL_NUMBER)
	{
		DebugLine += FString::Printf(TEXT("('%s' Time: %.3f / %.3f (%.0f%%) [%.3f])"),
			*UEnum::GetValueAsString(InertializationState),
			InertializationElapsedTime,
			InertializationDuration,
			100.0f * InertializationElapsedTime / InertializationDuration,
			InertializationDeficit);
	}
	else
	{
		DebugLine += FString::Printf(TEXT("('%s' Time: %.3f / %.3f [%.3f])"),
			*UEnum::GetValueAsString(InertializationState),
			InertializationElapsedTime,
			InertializationDuration,
			InertializationDeficit);
	}
	DebugData.AddDebugItem(DebugLine);

	Source.GatherDebugData(DebugData);
}


bool FAnimNode_Inertialization::NeedsDynamicReset() const
{
	return true;
}


void FAnimNode_Inertialization::ResetDynamics(ETeleportType InTeleportType)
{
	if (InTeleportType > TeleportType)
	{
		TeleportType = InTeleportType;
	}
}


bool FAnimNode_Inertialization::WantsSkippedUpdates() const
{
	// We want to receive an OnUpdatesSkipped callback if we have a pending request.
	// This will give us a chance to forward the request to other Inertialization nodes
	// that were skipped due to pose caching.
	return RequestedDuration >= 0.0f;
}


void FAnimNode_Inertialization::OnUpdatesSkipped(TArrayView<const FAnimationUpdateContext*> SkippedUpdateContexts)
{
	// Cached poses have their Update function called once even though there may be multiple UseCachedPose nodes for the same pose.
	// Because of this, there may be Inertialization ancestors of the UseCachedPose nodes that missed out on requests.
	// So here we forward 'this' node's requests to the ancestors of those skipped UseCachedPose nodes.
	for (const FAnimationUpdateContext* Update : SkippedUpdateContexts)
	{
		if (FAnimNode_Inertialization* OtherIneritalizationNode = Update->GetAncestor<FAnimNode_Inertialization>())
		{
			OtherIneritalizationNode->RequestInertialization(RequestedDuration);
		}
	}
}


float FAnimNode_Inertialization::ConsumeInertializationRequest(FPoseContext& Context)
{
	const float Duration = RequestedDuration;
	RequestedDuration = -1.0f;
	return Duration;
}


void FAnimNode_Inertialization::StartInertialization(FPoseContext& Context, FInertializationPose& PreviousPose1, FInertializationPose& PreviousPose2, float Duration, /*OUT*/ FInertializationPoseDiff& OutPoseDiff)
{
	// Determine if this skeletal mesh's actor is attached to another actor
	FName AttachParentName = NAME_None;
	if (AActor* Owner = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner())
	{
		if (AActor* AttachParentActor = Owner->GetAttachParentActor())
		{
			AttachParentName = AttachParentActor->GetFName();
		}
	}

	OutPoseDiff.InitFrom(Context.Pose, Context.Curve, Context.AnimInstanceProxy->GetComponentTransform(), AttachParentName, PreviousPose1, PreviousPose2);
}


void FAnimNode_Inertialization::ApplyInertialization(FPoseContext& Context, const FInertializationPoseDiff& PoseDiff, float ElapsedTime, float Duration)
{
	PoseDiff.ApplyTo(Context.Pose, Context.Curve, ElapsedTime, Duration);
}

void FAnimNode_Inertialization::Deactivate()
{
	InertializationState = EInertializationState::Inactive;
	InertializationElapsedTime = 0.0f;
	InertializationDuration = 0.0f;
	InertializationDeficit = 0.0f;
}


void FInertializationPose::InitFrom(const FCompactPose& Pose, const FBlendedCurve& InCurves, const FTransform& InComponentTransform, const FName& InAttachParentName, float InDeltaTime)
{
	const FBoneContainer& BoneContainer = Pose.GetBoneContainer();

	const int32 NumSkeletonBones = BoneContainer.GetSkeletonAsset()->GetReferenceSkeleton().GetNum();
	BoneTransforms.Reset(NumSkeletonBones);
	BoneTransforms.AddZeroed(NumSkeletonBones);
	BoneStates.Reset(NumSkeletonBones);
	BoneStates.AddZeroed(NumSkeletonBones);
	for (FCompactPoseBoneIndex BoneIndex : Pose.ForEachBoneIndex())
	{
		const int32 SkeletonPoseBoneIndex = BoneContainer.GetSkeletonIndex(BoneIndex);
		if (SkeletonPoseBoneIndex != INDEX_NONE)
		{
			BoneTransforms[SkeletonPoseBoneIndex] = Pose[BoneIndex];
			BoneStates[SkeletonPoseBoneIndex] = EInertializationBoneState::Valid;
		}
	}

	// Copy curves and make a new copy of the curve lookup table as it may change from frame to frame
	Curves.CopyFrom(InCurves);
	CurveUIDToArrayIndexLUT.Reset();
	if (InCurves.UIDToArrayIndexLUT)
	{
		CurveUIDToArrayIndexLUT = *InCurves.UIDToArrayIndexLUT;
		Curves.UIDToArrayIndexLUT = &CurveUIDToArrayIndexLUT;
	}

	ComponentTransform = InComponentTransform;
	AttachParentName = InAttachParentName;
	DeltaTime = InDeltaTime;
}


// Initialize the pose difference from the current pose and the two previous snapshots
//
// Pose			the current frame's pose
// Curves       the current frame's curves
// Prev1		the previous frame's pose and curves
// Prev2		the pose and curves from two frames before
// DeltaTime	the time elapsed between Prev1 and Pose
//
void FInertializationPoseDiff::InitFrom(const FCompactPose& Pose, const FBlendedCurve& Curves, const FTransform& ComponentTransform, const FName& AttachParentName, const FInertializationPose& Prev1, const FInertializationPose& Prev2)
{
	const FBoneContainer& BoneContainer = Pose.GetBoneContainer();

	const FQuat ComponentTransform_GetRotation_Inverse = ComponentTransform.GetRotation().Inverse();

	// Determine if we should initialize in local space (the default) or in world space (for situations where we wish to correct
	// a world-space discontinuity such as an abrupt orientation change)
	InertializationSpace = EInertializationSpace::Default;
	if (AttachParentName != Prev1.AttachParentName || AttachParentName != Prev2.AttachParentName)
	{
		// If the parent space has changed, then inertialize in world space
		InertializationSpace = EInertializationSpace::WorldSpace;
	}
	else if (AttachParentName == NAME_None)
	{
		// If there was a discontinuity in ComponentTransform orientation, then correct for that by inertializing the orientation in world space
		// (but only if the mesh is not attached to another actor, because we don't want to dampen the connection between attached actors)
		if ((FMath::Abs((Prev1.ComponentTransform.GetRotation() * ComponentTransform_GetRotation_Inverse).W) < 0.999f) ||	// (W < 0.999f --> angle > 5 degrees)
			(FMath::Abs((Prev2.ComponentTransform.GetRotation() * ComponentTransform_GetRotation_Inverse).W) < 0.999f))		// (W < 0.999f --> angle > 5 degrees)
		{
			InertializationSpace = EInertializationSpace::WorldRotation;
		}
	}

	// Compute the inertialization differences for each bone
	const FReferenceSkeleton& RefSkeleton = BoneContainer.GetSkeletonAsset()->GetReferenceSkeleton();
	const int32 NumSkeletonBones = RefSkeleton.GetNum();
	BoneDiffs.Empty(NumSkeletonBones);
	BoneDiffs.AddZeroed(NumSkeletonBones);
	for (FCompactPoseBoneIndex BoneIndex : Pose.ForEachBoneIndex())
	{
		const int32 SkeletonPoseBoneIndex = BoneContainer.GetSkeletonIndex(BoneIndex);

		if (SkeletonPoseBoneIndex != INDEX_NONE && Prev1.BoneStates[SkeletonPoseBoneIndex] == EInertializationBoneState::Valid)
		{
			const FTransform PoseTransform = Pose[BoneIndex];
			FTransform Prev1Transform = Prev1.BoneTransforms[SkeletonPoseBoneIndex];
			FTransform Prev2Transform = Prev2.BoneTransforms[SkeletonPoseBoneIndex];
			const bool Prev2IsValid = Prev2.BoneStates[SkeletonPoseBoneIndex] == EInertializationBoneState::Valid;

			if (BoneIndex.IsRootBone())
			{
				// If we are inertializing in world space, then adjust the historical root bones to be in a consistent reference frame
				if (InertializationSpace == EInertializationSpace::WorldSpace)
				{
					Prev1Transform *= Prev1.ComponentTransform.GetRelativeTransform(ComponentTransform);
					Prev2Transform *= Prev2.ComponentTransform.GetRelativeTransform(ComponentTransform);
				}
				else if (InertializationSpace == EInertializationSpace::WorldRotation)
				{
					Prev1Transform.SetRotation(ComponentTransform_GetRotation_Inverse * Prev1.ComponentTransform.GetRotation() * Prev1Transform.GetRotation());
					Prev2Transform.SetRotation(ComponentTransform_GetRotation_Inverse * Prev2.ComponentTransform.GetRotation() * Prev2Transform.GetRotation());
				}
			}
			else
			{
				// If this bone is a child of an excluded bone, then adjust the previous transforms to be relative to the excluded parent's
				// new transform so that the children maintain their original component space transform even though the parent will pop
				FCompactPoseBoneIndex ParentBoneIndex = BoneContainer.GetParentBoneIndex(BoneIndex);
				int32 ParentSkeletonPoseBoneIndex = (ParentBoneIndex != INDEX_NONE) ? BoneContainer.GetSkeletonIndex(ParentBoneIndex) : INDEX_NONE;
				if (ParentBoneIndex != INDEX_NONE && ParentSkeletonPoseBoneIndex != INDEX_NONE &&
					(Prev1.BoneStates[ParentSkeletonPoseBoneIndex] == EInertializationBoneState::Excluded || Prev2.BoneStates[ParentSkeletonPoseBoneIndex] == EInertializationBoneState::Excluded))
				{
					FTransform ParentPrev1Transform = Prev1.BoneTransforms[ParentSkeletonPoseBoneIndex];
					FTransform ParentPrev2Transform = Prev2.BoneTransforms[ParentSkeletonPoseBoneIndex];
					FTransform ParentPoseTransform = Pose[ParentBoneIndex];

					// Continue walking up the skeleton hierarchy in case the parent's parent etc is also excluded
					ParentBoneIndex = BoneContainer.GetParentBoneIndex(ParentBoneIndex);
					ParentSkeletonPoseBoneIndex = (ParentBoneIndex != INDEX_NONE) ? BoneContainer.GetSkeletonIndex(ParentBoneIndex) : INDEX_NONE;
					while (ParentBoneIndex != INDEX_NONE && ParentSkeletonPoseBoneIndex != INDEX_NONE &&
						(Prev1.BoneStates[ParentSkeletonPoseBoneIndex] == EInertializationBoneState::Excluded || Prev2.BoneStates[ParentSkeletonPoseBoneIndex] == EInertializationBoneState::Excluded))
					{
						ParentPrev1Transform *= Prev1.BoneTransforms[ParentSkeletonPoseBoneIndex];
						ParentPrev2Transform *= Prev2.BoneTransforms[ParentSkeletonPoseBoneIndex];
						ParentPoseTransform *= Pose[ParentBoneIndex];

						ParentBoneIndex = BoneContainer.GetParentBoneIndex(ParentBoneIndex);
						ParentSkeletonPoseBoneIndex = (ParentBoneIndex != INDEX_NONE) ? BoneContainer.GetSkeletonIndex(ParentBoneIndex) : INDEX_NONE;
					}

					// Adjust the transforms so that they behave as though the excluded parent has been in its new location all along
					Prev1Transform *= ParentPrev1Transform.GetRelativeTransform(ParentPoseTransform);
					Prev2Transform *= ParentPrev2Transform.GetRelativeTransform(ParentPoseTransform);
				}
			}

			FInertializationBoneDiff& BoneDiff = BoneDiffs[SkeletonPoseBoneIndex];

			// Compute the bone translation difference
			{
				FVector TranslationDirection = FVector::ZeroVector;
				float TranslationMagnitude = 0.0f;
				float TranslationSpeed = 0.0f;

				const FVector T = Prev1Transform.GetTranslation() - PoseTransform.GetTranslation();
				TranslationMagnitude = T.Size();
				if (TranslationMagnitude > KINDA_SMALL_NUMBER)
				{
					TranslationDirection = T / TranslationMagnitude;
				}

				if (Prev2IsValid && Prev1.DeltaTime > KINDA_SMALL_NUMBER && TranslationMagnitude > KINDA_SMALL_NUMBER)
				{
					const FVector PrevT = Prev2Transform.GetTranslation() - PoseTransform.GetTranslation();
					const float PrevMagnitude = FVector::DotProduct(PrevT, TranslationDirection);
					TranslationSpeed = (TranslationMagnitude - PrevMagnitude) / Prev1.DeltaTime;
				}

				BoneDiff.TranslationDirection = TranslationDirection;
				BoneDiff.TranslationMagnitude = TranslationMagnitude;
				BoneDiff.TranslationSpeed = TranslationSpeed;
			}

			// Compute the bone rotation difference
			{
				FVector RotationAxis = FVector::ZeroVector;
				float RotationAngle = 0.0f;
				float RotationSpeed = 0.0f;

				const FQuat Q = Prev1Transform.GetRotation() * PoseTransform.GetRotation().Inverse();
				Q.ToAxisAndAngle(RotationAxis, RotationAngle);
				RotationAngle = FMath::UnwindRadians(RotationAngle);
				if (RotationAngle < 0.0f)
				{
					RotationAxis = -RotationAxis;
					RotationAngle = -RotationAngle;
				}

				if (Prev2IsValid && Prev1.DeltaTime > KINDA_SMALL_NUMBER && RotationAngle > KINDA_SMALL_NUMBER)
				{
					const FQuat PrevQ = Prev2Transform.GetRotation() * PoseTransform.GetRotation().Inverse();
					const float PrevAngle = PrevQ.GetTwistAngle(RotationAxis);
					RotationSpeed = FMath::UnwindRadians(RotationAngle - PrevAngle) / Prev1.DeltaTime;
				}

				BoneDiff.RotationAxis = RotationAxis;
				BoneDiff.RotationAngle = RotationAngle;
				BoneDiff.RotationSpeed = RotationSpeed;
			}

			// Compute the bone scale difference
			{
				FVector ScaleAxis = FVector::ZeroVector;
				float ScaleMagnitude = 0.0f;
				float ScaleSpeed = 0.0f;

				const FVector S = Prev1Transform.GetScale3D() - PoseTransform.GetScale3D();
				ScaleMagnitude = S.Size();
				if (ScaleMagnitude > KINDA_SMALL_NUMBER)
				{
					ScaleAxis = S / ScaleMagnitude;
				}

				if (Prev2IsValid && Prev1.DeltaTime > KINDA_SMALL_NUMBER && ScaleMagnitude > KINDA_SMALL_NUMBER)
				{
					const FVector PrevS = Prev2Transform.GetScale3D() - PoseTransform.GetScale3D();
					const float PrevMagnitude = FVector::DotProduct(PrevS, ScaleAxis);
					ScaleSpeed = (ScaleMagnitude - PrevMagnitude) / Prev1.DeltaTime;
				}

				BoneDiff.ScaleAxis = ScaleAxis;
				BoneDiff.ScaleMagnitude = ScaleMagnitude;
				BoneDiff.ScaleSpeed = ScaleSpeed;
			}
		}
	}

	// Compute the curve differences
	const int32 CurveNum = Curves.IsValid() ? Curves.UIDToArrayIndexLUT->Num() : 0;
	CurveDiffs.Empty(CurveNum);
	CurveDiffs.AddZeroed(CurveNum);
	for (int32 CurveUID = 0; CurveUID != CurveNum; ++CurveUID)
	{
		const int32 CurrIdx = Curves.GetArrayIndexByUID(CurveUID);
		const int32 Prev1Idx = Prev1.Curves.GetArrayIndexByUID(CurveUID);
		if (CurrIdx == INDEX_NONE || Prev1Idx == INDEX_NONE)
		{
			// CurveDiff is zeroed
			continue;
		}

		const FCurveElement& CurrElement = Curves.Elements[CurrIdx];
		const FCurveElement& Prev1Element = Prev1.Curves.Elements[Prev1Idx];
		FInertializationCurveDiff& CurveDiff = CurveDiffs[CurveUID];

		// Note we intentionally ignore FCurveElement::bValid. We want to ease in/out when only one
		// curve is valid, and we'll compute a zero delta and derivative when both are invalid.
		CurveDiff.Delta = Prev1Element.Value - CurrElement.Value;

		const int32 Prev2Idx = Prev2.Curves.GetArrayIndexByUID(CurveUID);
		if (Prev2Idx != INDEX_NONE && Prev1.DeltaTime > KINDA_SMALL_NUMBER)
		{
			const FCurveElement& Prev2Element = Prev2.Curves.Elements[Prev2Idx];
			CurveDiff.Derivative = (Prev1Element.Value - Prev2Element.Value) / Prev1.DeltaTime;
		}
	}
}


// Apply this difference to a pose and a set of curves, decaying over time as InertializationElapsedTime approaches InertializationDuration
//
void FInertializationPoseDiff::ApplyTo(FCompactPose& Pose, FBlendedCurve& Curves, float InertializationElapsedTime, float InertializationDuration) const
{
	const FBoneContainer& BoneContainer = Pose.GetBoneContainer();

	// Apply pose difference
	for (FCompactPoseBoneIndex BoneIndex : Pose.ForEachBoneIndex())
	{
		const int32 SkeletonPoseBoneIndex = BoneContainer.GetSkeletonIndex(BoneIndex);

		if (SkeletonPoseBoneIndex != INDEX_NONE)
		{
			const FInertializationBoneDiff& BoneDiff = BoneDiffs[SkeletonPoseBoneIndex];

			// Apply the bone translation difference
			const FVector T = BoneDiff.TranslationDirection *
				CalcInertialFloat(BoneDiff.TranslationMagnitude, BoneDiff.TranslationSpeed, InertializationElapsedTime, InertializationDuration);
			Pose[BoneIndex].AddToTranslation(T);

			// Apply the bone rotation difference
			const FQuat Q = FQuat(BoneDiff.RotationAxis,
				CalcInertialFloat(BoneDiff.RotationAngle, BoneDiff.RotationSpeed, InertializationElapsedTime, InertializationDuration));
			Pose[BoneIndex].SetRotation(Q * Pose[BoneIndex].GetRotation());

			// Apply the bone scale difference
			const FVector S = BoneDiff.ScaleAxis *
				CalcInertialFloat(BoneDiff.ScaleMagnitude, BoneDiff.ScaleSpeed, InertializationElapsedTime, InertializationDuration);
			Pose[BoneIndex].SetScale3D(S + Pose[BoneIndex].GetScale3D());
		}
	}

	Pose.NormalizeRotations();

	// Apply curve differences
	if (Curves.IsValid())
	{
		// Note Curves.Elements is indexed indirectly via lookup table while CurveDiffs is indexed directly by curve ID
		const int32 CurveNum = FMath::Min(Curves.UIDToArrayIndexLUT->Num(), CurveDiffs.Num());
		for (int32 CurveUID = 0; CurveUID != CurveNum; ++CurveUID)
		{
			const int32 CurrIdx = Curves.GetArrayIndexByUID(CurveUID);
			if (CurrIdx == INDEX_NONE)
			{
				continue;
			}

			FCurveElement& CurrElement = Curves.Elements[CurrIdx];
			const FInertializationCurveDiff& CurveDiff = CurveDiffs[CurveUID];
			const float C = CalcInertialFloat(CurveDiff.Delta, CurveDiff.Derivative, InertializationElapsedTime, InertializationDuration);
			if (C != 0.0f)
			{
				CurrElement.Value += C;
				CurrElement.bValid = true;
			}
		}
	}
}


// Calculate the "inertialized" value of a single float at time t
//
// @param x0	Initial value of the float (at time 0)
// @param v0	Initial "velocity" (first derivative) of the float (at time 0)
// @param t		Time at which to evaluate the float
// @param t1	Ending inertialization time (ie: the time at which the curve must be zero)
//
// Evaluates a quintic polynomial curve with the specified initial conditions (x0, v0) which hits zero at time t1.  As well,
// the curve is designed so that the first and second derivatives are also zero at time t1.
//
// The initial second derivative (a0) is chosen such that it is as close to zero as possible, but large enough to prevent any
// overshoot (ie: enforce x >= 0 for t between 0 and t1).  If necessary, the ending time t1 will be adjusted (shortened) to
// guarantee that there is no overshoot, even for very large initial velocities.
//
float FInertializationPoseDiff::CalcInertialFloat(float x0, float v0, float t, float t1)
{
	static_assert(INERTIALIZATION_TIME_EPSILON * INERTIALIZATION_TIME_EPSILON * INERTIALIZATION_TIME_EPSILON * INERTIALIZATION_TIME_EPSILON * INERTIALIZATION_TIME_EPSILON > FLT_MIN,
		"INERTIALIZATION_TIME_EPSILON^5 must be greater than FLT_MIN to avoid denormalization (and potential division by zero) for very small values of t1");

	if (t < 0.0f)
	{
		t = 0.0f;
	}

	if (t >= t1 - INERTIALIZATION_TIME_EPSILON)
	{
		return 0.0f;
	}

	// Assume that x0 >= 0... if this is not the case, then simply invert everything (both input and output)
	float sign = 1.0f;
	if (x0 < 0.0f)
	{
		x0 = -x0;
		v0 = -v0;
		sign = -1.0f;
	}

	// If v0 > 0, then the curve will overshoot away from zero, so clamp v0 here to guarantee that there is no overshoot
	if (v0 > 0.0f)
	{
		v0 = 0.0f;
	}

	check(x0 >= 0.0f);
	check(v0 <= 0.0f);
	check(t >= 0.0f);
	check(t1 >= 0.0f);

	// Limit t1 such that the curve does not overshoot below zero (ensuring that x >= 0 for all t between 0 and t1).
	//
	// We observe that if the curve does overshoot below zero, it must have an inflection point somewhere between 0 and t1
	// (since we know that both x0 and x1 are >= 0).  Therefore, we can prevent overshoot by adjusting t1 such that any
	// inflection point is at t >= t1.
	//
	// Assuming that we are using the zero jerk version of the curve (see below) whose velocity has a triple root at t1, then
	// we can prevent overshoot by forcing the remaining root to be at time t >= t1, or equivalently, we can set t1 to be the
	// lesser of the original t1 or the value that gives us a solution with a quadruple velocity root at t1.
	//
	// The following Mathematica expression solves for t1 that gives us the quadruple velocity root:
	//
	//		v := q * (t-t1)^4
	//		x := Integrate[Expand[v], t] + x0
	//		eq1 := (v/.t->0)==v0
	//		eq2 := (x/.t->t1)==0
	//		Solve[{eq1 && eq2}, {q,t1}]
	//
	if (v0 < -KINDA_SMALL_NUMBER)
	{
		t1 = FMath::Min(t1, -5.0f * x0 / v0);
	}

	if (t >= t1 - INERTIALIZATION_TIME_EPSILON)
	{
		return 0.0f;
	}

	const float t1_2 = t1 * t1;
	const float t1_3 = t1 * t1_2;
	const float t1_4 = t1 * t1_3;
	const float t1_5 = t1 * t1_4;

	// Compute the initial acceleration value (a0) for this curve.  Ideally we want to use an initial acceleration of zero, but
	// if there is a large negative initial velocity, then we will need to use a larger acceleration in order to ensure that
	// the curve does not overshoot below zero (ie: to ensure that x >= 0 for all t between 0 and t1).
	//
	// To compute a0, we first compute the a0 that we would get if we also specified that the third derivative (the "jerk" j)
	// is zero at t1.  If this value of a0 is positive (and therefore opposing the initial velocity), then we use that.  If it
	// is negative, then we simply use an initial a0 of zero.
	//
	// The following Mathematica expression solves for a0 that gives us zero jerk at t1:
	//
	//		x:= A*t^5 + B*t^4 + C*t^3 + D*t^2 + v0*t + x0
	//		v:=Dt[x, t, Constants->{A,B,C,D,v0,x0}]
	//		a:=Dt[v, t, Constants->{A,B,C,D,v0,x0}]
	//		j:=Dt[a, t, Constants->{A,B,C,D,v0,x0}]
	//		eq1:=(x/.t->t1)==0
	//		eq2:=(v/.t->t1)==0
	//		eq3:=(a/.t->t1)==0
	//		eq4:=(j/.t->t1)==0
	//		a0:=a/.t->0/.Solve[{eq1 && eq2 && eq3 && eq4}, {A,B,C,D}]
	//		ExpandNumerator[a0]
	//
	const float a0 = FMath::Max(0.0f, (-8.0f*t1*v0 - 20.0f*x0) / t1_2);

	// Compute the polynomial coefficients given the starting and ending conditions, solved from:
	//
	//		x:= A*t^5 + B*t^4 + C*t^3 + D*t^2 + v0*t + x0
	//		v:=Dt[x, t, Constants->{A,B,C,D,v0,x0}]
	//		a:=Dt[v, t, Constants->{A,B,C,D,v0,x0}]
	//		eq1:=(x/.t->t1)==0
	//		eq2:=(v/.t->t1)==0
	//		eq3:=(a/.t->t1)==0
	//		eq4:=(a/.t->0)==a0
	//		Simplify[Solve[{eq1 && eq2 && eq3 && eq4}, {A,B,C,D}]]
	//
	const float A = -0.5f * (     a0*t1_2 +  6.0f*t1*v0 + 12.0f*x0) / t1_5;
	const float B =  0.5f * (3.0f*a0*t1_2 + 16.0f*t1*v0 + 30.0f*x0) / t1_4;
	const float C = -0.5f * (3.0f*a0*t1_2 + 12.0f*t1*v0 + 20.0f*x0) / t1_3;
	const float D =  0.5f * a0;

	const float x = (((((A*t) + B)*t + C)*t + D)*t + v0)*t + x0;

	return x * sign;
}

#undef LOCTEXT_NAMESPACE