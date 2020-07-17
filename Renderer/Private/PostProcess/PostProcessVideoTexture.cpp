// @Leo TODO: Add a Video Texture Pass

#include "PostProcess/PostProcessVideoTexture.h"
#include "Renderer/Public/VideoResourceManager.h"
#include "PostProcess/PostProcessCompositeEditorPrimitives.h"
#include "SceneTextureParameters.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "ClearQuad.h"

class FVideoTextureShader : public FGlobalShader
{
public:
	FVideoTextureShader() = default;
	FVideoTextureShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);  
		return true;
	}
};

class FVideoTextureShaderPS : public FVideoTextureShader
{
	// 将我们的shader加入到全局的shadermap中，这也是虚幻能识别到我们的shader然后编译它的关键
	DECLARE_GLOBAL_SHADER(FVideoTextureShaderPS);
	SHADER_USE_PARAMETER_STRUCT(FVideoTextureShaderPS, FVideoTextureShader);

	/* 定义传入Shader的参数，使用FVideoTextureShaderPS::FParameters可以生成一个参数实例
		第一行绑定Shader中的SimpleColor变量，第二行是给这个Shader绑定一个RenderTarget
		因为这个Shader属于Raster（光栅）功能，如果是用于计算功能则不用绑定RenderTarget */
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, DepTexScale)
		SHADER_PARAMETER(FMatrix, DeProjectionMatrixThisView)
		SHADER_PARAMETER(FMatrix, ProjectionMatrixCameraView)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VideoTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

// 将Shader类和具体Shader文件绑定，指定Shader类型和HLSL入口代码，使用相对路径是因为RenderModule里定义了相对路径的写法
IMPLEMENT_GLOBAL_SHADER(FVideoTextureShaderPS, "/Engine/Private/PostProcessVideoTexture.usf", "MainPS", SF_Pixel);

FScreenPassTexture AddVideoTexturePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FVideoTextureInputs& Inputs)
{
	check(Inputs.SceneColor.IsValid());

	RDG_EVENT_SCOPE(GraphBuilder, "VideoTexture");

	FSceneRenderTargets& renderTargets = FSceneRenderTargets::Get(GRHICommandList.GetImmediateCommandList());

	if (VideoResourceManager::Get().GetVideoRenderTargetItem() == NULL)
	{
		VideoResourceManager::Get().InitRenderTargetItem(renderTargets.GBufferB->GetRenderTargetItem(), renderTargets.GBufferB->GetDesc());
	}

	// 创建一个用于输出RenderTarget
	FScreenPassRenderTarget Output = Inputs.OverrideOutput;

	// 从Manager中获取RenderTargetItem和Desc
	FSceneRenderTargetItem* VideoRenderTargetItem = VideoResourceManager::Get().GetVideoRenderTargetItem();	
	FPooledRenderTargetDesc* Desc = VideoResourceManager::Get().GetDesc();
	
	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;	// 定义一个空值
	GRenderTargetPool.CreateUntrackedElement(/*输入*/*Desc, /*输出*/PooledRenderTarget, /*输入*/*VideoRenderTargetItem);
	FRDGTextureRef RDGVideoColorTexture = GraphBuilder.RegisterExternalTexture(/*输入*/PooledRenderTarget, TEXT("VideoTexture"));

	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, Inputs.SceneColor, View.GetOverwriteLoadAction(), TEXT("VideoTextureColor"));
	}

	const FScreenPassTextureViewport OutputViewport(Output);			// 定义一个输出的Viewport
	const FScreenPassTextureViewport ColorViewport(Inputs.SceneColor);	// 定义一个输入的Viewport

	FVideoTextureShaderPS::FParameters* PassParameters = GraphBuilder.AllocParameters< FVideoTextureShaderPS::FParameters>();
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	PassParameters->DeProjectionMatrixThisView = VideoResourceManager::Get().GetDeProjectionMatrix();
	PassParameters->ProjectionMatrixCameraView = VideoResourceManager::Get().GetProjectionMatrix();
	PassParameters->VideoTexture = RDGVideoColorTexture;
	PassParameters->ColorTexture = Inputs.SceneColor.Texture;
	PassParameters->DepthTexture = Inputs.SceneDepth.Texture;
	PassParameters->Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->DepTexScale = (float)renderTargets.GetSceneDepthTexture()->GetSizeX() / (View.ViewRect.Max.X - View.ViewRect.Min.X);

	TShaderMapRef<FVideoTextureShaderPS> PixelShader(View.ShaderMap);

	// 向GraphBuilder添加Pass
	AddDrawScreenPass(
		GraphBuilder,
		RDG_EVENT_NAME("VideoTexture"),
		View,
		OutputViewport,
		ColorViewport,
		PixelShader,
		PassParameters);

	return MoveTemp(Output);
}

bool HasVideoTexture()
{
	return VideoResourceManager::Get().bUseVideoTexture;
}
