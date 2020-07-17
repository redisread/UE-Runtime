// @Leo TODO: Add a Video Texture Pass

#pragma once

#include "ScreenPass.h"
#include "OverridePassSequence.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "Renderer/Public/DepthResourceManager.h"

/* 获取深度缓存，在后处理阶段AddPostProcessingPasses()函数中调用（已完成）*/
void AddDepthInspectorPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, DepthResult* result);

