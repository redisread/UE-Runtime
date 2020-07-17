// @Leo TODO: Add a Video Texture Pass

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"
#include "PostProcess/RenderingCompositionGraph.h"

// 用于Processing过程中，向AddVideoTexturePass()传入当前的场景颜色引用
struct FVideoTextureInputs
{
	FScreenPassRenderTarget OverrideOutput;
	FScreenPassTexture SceneColor;
	FScreenPassTexture SceneDepth;
};

FScreenPassTexture AddVideoTexturePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVideoTextureInputs& Inputs);

bool HasVideoTexture();

