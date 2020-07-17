// Renderer/Public/VideoResourceManager.h
#pragma once

#include "Math/Vector2D.h"
#include "RHIResources.h"
#include "Math/Color.h"
#include "Containers/Queue.h"
#include "RendererInterface.h"

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "SceneInterface.h"
#include "MaterialShared.h"


DEFINE_LOG_CATEGORY_STATIC(VideoResourceLog, Log, All);

class VideoResourceManager
{
private:
	// 视频贴图的信息
	FPooledRenderTargetDesc* Desc;
	// 用于保存视频贴图
	FSceneRenderTargetItem* VideoRenderTargetItem;
	// 视频贴图
	FTextureRHIRef TextureRHI;

	// 逆投影矩阵和投影矩阵
	FMatrix DeProjectionMatrixThisView;
	FMatrix ProjectionMatrixCameraView;
	
	VideoResourceManager()
	{
		UE_LOG(VideoResourceLog, Log, TEXT("Ini"));
	}

public:
	bool bUseVideoTexture = false;

public:
	// 初始化RenderTargetItem，在RenderTargetItem为空时会调用
	void InitRenderTargetItem(FSceneRenderTargetItem InItem, FPooledRenderTargetDesc InDesc)
	{
		VideoRenderTargetItem = new FSceneRenderTargetItem;
		// 创建一个大小为1920*1088的Texture
		FVector2D RHITextureRect = FVector2D(1920, 1088);
		uint8 Format = InItem.TargetableTexture.GetReference()->GetFormat();
		uint32 NumMips = InItem.TargetableTexture.GetReference()->GetNumMips();
		uint32 NumSamples = InItem.TargetableTexture.GetReference()->GetNumSamples();
		uint32 Flags = InItem.TargetableTexture.GetReference()->GetFlags();
		FRHIResourceCreateInfo CreateInfo;

		VideoRenderTargetItem->TargetableTexture = RHICreateTexture2D(RHITextureRect.X, RHITextureRect.Y, Format, NumMips, NumSamples, Flags, CreateInfo);
		VideoRenderTargetItem->ShaderResourceTexture = TextureRHI;
		VideoRenderTargetItem->UAV = InItem.UAV;

		Desc = new FPooledRenderTargetDesc(InDesc);
		Desc->DebugName = TEXT("VideoTexture");
	}

	FPooledRenderTargetDesc* GetDesc()
	{
		return Desc;
	}

	FMatrix GetDeProjectionMatrix()
	{
		return DeProjectionMatrixThisView;
	}

	FMatrix GetProjectionMatrix()
	{
		return ProjectionMatrixCameraView;
	}

	FSceneRenderTargetItem* GetVideoRenderTargetItem()
	{
		return VideoRenderTargetItem;
	}

	// 设置当前视角的逆投影矩阵
	void SetDeProjectionMatrix(FMatrix InMatrix)
	{
		DeProjectionMatrixThisView = InMatrix;
	}
	
	// 设置投影相机的投影矩阵
	void SetProjectionMatrix(FMatrix InMatrix)
	{
		ProjectionMatrixCameraView = InMatrix;
	}

	// 设置视频纹理
	void SetVideoTexture(FTextureRHIRef InTextureRHI)
	{
		TextureRHI = InTextureRHI;
	}

	RENDERER_API static VideoResourceManager& Get()
	{
		static VideoResourceManager singleton;
		return singleton;
	}

};