 
#include "PostProcess/PostProcessDepthCapture.h"
#include "DepthResourceManager.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "ClearQuad.h"

/*获取深度缓存，在后处理阶段AddPostProcessingPasses()函数中调用（已完成）*/
void AddDepthInspectorPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, DepthResult* result)
{

	RDG_EVENT_SCOPE(GraphBuilder, "DepthInspector");

	// 获取渲染对象
	FSceneRenderTargets& renderTargets = FSceneRenderTargets::Get(GRHICommandList.GetImmediateCommandList());

	// 定义拷贝参数
	uint32 striped = 0;
	FIntPoint size = renderTargets.GetBufferSizeXY();
	result->bufferSizeX = size.X;
	result->bufferSizeY = size.Y;
	result->data.AddUninitialized(size.X * size.Y);

	// 获取视窗某一帧的深度缓存对象
	FRHITexture2D* depthTexture = (FRHITexture2D *)renderTargets.SceneDepthZ->GetRenderTargetItem().TargetableTexture.GetReference();

	// 执行拷贝深度缓存操作，将GPU显存中的缓存信息拷贝到CPU内存中，返回指向这块CPU内存的首地址
	void* buffer = RHILockTexture2D(depthTexture, 0, EResourceLockMode::RLM_ReadOnly, striped, true);

	// 将缓存结果拷贝到result，用于输出
	memcpy(result->data.GetData(), buffer, size.X * size.Y * 8);

	// 必须执行解锁语句，否则被锁住的GPU缓存信息将不能释放
	RHIUnlockTexture2D(depthTexture, 0, true);

	// 拷贝结果入队
	DepthCapture::Get().FinishedCapture(result);

	return;
}
