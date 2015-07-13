#include "..\Inc\ShadowMapGenerationEffect.h"
#include "pch_directX.h"

#include <ShadowMapGenerationEffect.h>
#include "EffectCommon.h"
#include <CommonStates.h>

using namespace DirectX;

struct ShadowMapGenerationEffectConstants
{
	XMMATRIX	WorldLightProj;
	XMVECTOR	ShadowColor;
	XMFLOAT3X4	Bones[ShadowMapGenerationEffect::MaxBones];
};

struct ShadowMapGenerationEffectTraits
{
	typedef ShadowMapGenerationEffectConstants ConstantBufferType;

	static const int VertexShaderCount = 8;
	static const int PixelShaderCount = 4;
	static const int ShaderPermutationCount = 16;
};

typedef ShadowMapGenerationEffectTraits			EffectTraitsType;
typedef EffectBase<EffectTraitsType>			EffectBaseType;

SharedResourcePool<ID3D11Device*, EffectBaseType::DeviceResources> EffectBaseType::deviceResourcesPool;

using Microsoft::WRL::ComPtr;

class ShadowMapGenerationEffect::Impl : public EffectBaseType
{
public:
	int weightsPerVertex;
	ShadowFillMode fillMode;
	Matrix4x4 World, LightView, LightProj;
	ID3D11DepthStencilView* pDepthMap;
	ID3D11RenderTargetView* pRenderTargetView;
	CommonStates			commonStates;

	typedef EffectTraitsType	Traits;
	typedef EffectBaseType		Base;

	Impl(ID3D11Device* device)
			: EffectBase(device),
			commonStates(device),
			weightsPerVertex(0),
			fillMode(DepthFill),
			pRenderTargetView(NULL),
			pDepthMap(NULL)
		{
			static_assert(_countof(Base::VertexShaderIndices) == Traits::ShaderPermutationCount, "array/max mismatch");
			static_assert(_countof(Base::VertexShaderBytecode) == Traits::VertexShaderCount, "array/max mismatch");
			static_assert(_countof(Base::PixelShaderBytecode) == Traits::PixelShaderCount, "array/max mismatch");
			static_assert(_countof(Base::PixelShaderIndices) == Traits::ShaderPermutationCount, "array/max mismatch");

			XMMATRIX id = XMMatrixIdentity();

			for (size_t i = 0; i < MaxBones; ++i)
			{
				XMStoreFloat3x4(&constants.Bones[i], id);
			}
		}

	int GetCurrentShaderPermutation() const
	{
		int perm = 0;
		// 0 1 2 4
		if (weightsPerVertex <= 2) perm = weightsPerVertex;
		if (weightsPerVertex == 4) perm = 3;

		if (texture != nullptr)
			perm += 4;

		if (fillMode == SolidColorFill)
			perm += 8;
		return perm;
	}

	void Apply(ID3D11DeviceContext* deviceContext)
	{
		// Compute derived parameter values.
		matrices.SetConstants(dirtyFlags, constants.WorldLightProj);

		// Set the render targets if applicatable
		if (pDepthMap != nullptr || pRenderTargetView != nullptr)
		{
			ID3D11ShaderResourceView* pSrvs[] = {NULL, NULL, NULL, NULL, NULL, NULL};
			deviceContext->PSSetShaderResources(0, sizeof(pSrvs), pSrvs);
			assert(fillMode != SolidColorFill && pRenderTargetView != nullptr);
			ID3D11RenderTargetView* rtvs[] = { pRenderTargetView };
			deviceContext->OMSetRenderTargets(1, rtvs, pDepthMap);
		}

		ApplyShaders(deviceContext, GetCurrentShaderPermutation());
		auto pSampler = commonStates.PointWrap();
		deviceContext->OMSetBlendState(commonStates.Opaque(), g_XMOne.f, -1);
		deviceContext->PSSetSamplers(0, 1, &pSampler);
	}
};


namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "Shaders/Xbox/ShadowMapGen_VS_NoBone.inc"
#include "Shaders/Xbox/ShadowMapGen_VS_OneBone.inc"
#include "Shaders/Xbox/ShadowMapGen_VS_TwoBone.inc"
#include "Shaders/Xbox/ShadowMapGen_VS_FourBone.inc"

#include "Shaders/Windows/ShadowMapGen_PS.inc"
#else
#include "Shaders/Windows/ShadowMapGen_VS_NoBone.inc"
#include "Shaders/Windows/ShadowMapGen_VS_OneBone.inc"
#include "Shaders/Windows/ShadowMapGen_VS_TwoBone.inc"
#include "Shaders/Windows/ShadowMapGen_VS_FourBone.inc"
#include "Shaders/Windows/ShadowMapGen_VS_NoBoneTex.inc"
#include "Shaders/Windows/ShadowMapGen_VS_OneBoneTex.inc"
#include "Shaders/Windows/ShadowMapGen_VS_TwoBoneTex.inc"
#include "Shaders/Windows/ShadowMapGen_VS_FourBoneTex.inc"

#include "Shaders/Windows/ShadowMapGen_PS_DepthNoTex.inc"
#include "Shaders/Windows/ShadowMapGen_PS_DepthTex.inc"
#include "Shaders/Windows/ShadowMapGen_PS_ColorNoTex.inc"
#include "Shaders/Windows/ShadowMapGen_PS_ColorTex.inc"
#endif
}

template <size_t Size>
inline ShaderBytecode MakeShaderByteCode(const BYTE(&bytecode)[Size])
{
	return ShaderBytecode{ bytecode ,sizeof(bytecode) };
}

const ShaderBytecode EffectBase<ShadowMapGenerationEffectTraits>::VertexShaderBytecode[] =
{
	MakeShaderByteCode(ShadowMapGen_VS_NoBone),
	MakeShaderByteCode(ShadowMapGen_VS_OneBone),
	MakeShaderByteCode(ShadowMapGen_VS_TwoBone),
	MakeShaderByteCode(ShadowMapGen_VS_FourBone),
	MakeShaderByteCode(ShadowMapGen_VS_NoBoneTex),
	MakeShaderByteCode(ShadowMapGen_VS_OneBoneTex),
	MakeShaderByteCode(ShadowMapGen_VS_TwoBoneTex),
	MakeShaderByteCode(ShadowMapGen_VS_FourBoneTex),
};


const int EffectBase<ShadowMapGenerationEffectTraits>::VertexShaderIndices[] =
{
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
};


const ShaderBytecode EffectBase<ShadowMapGenerationEffectTraits>::PixelShaderBytecode[] =
{
	MakeShaderByteCode(ShadowMapGen_PS_DepthNoTex),
	MakeShaderByteCode(ShadowMapGen_PS_DepthTex),
	MakeShaderByteCode(ShadowMapGen_PS_ColorNoTex),
	MakeShaderByteCode(ShadowMapGen_PS_ColorTex),
};


const int EffectBase<ShadowMapGenerationEffectTraits>::PixelShaderIndices[] =
{
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3
};

ShadowMapGenerationEffect::ShadowMapGenerationEffect(ID3D11Device * device)
	: pImpl(new Impl(device))
{
}

ShadowMapGenerationEffect::~ShadowMapGenerationEffect()
{
}


void ShadowMapGenerationEffect::SetShadowMap(ID3D11DepthStencilView * pShaodwMap, ID3D11RenderTargetView* pRTV)
{
	pImpl->pRenderTargetView = pRTV;
	pImpl->pDepthMap = pShaodwMap;
}

void XM_CALLCONV DirectX::ShadowMapGenerationEffect::SetShadowFillMode(ShadowFillMode mode)
{
	pImpl->fillMode = mode;
}

void XM_CALLCONV DirectX::ShadowMapGenerationEffect::SetShadowColor(FXMVECTOR color)
{
	pImpl->constants.ShadowColor = color;
	pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

void ShadowMapGenerationEffect::SetAlphaDiscardThreshold(float clipThreshold)
{
}

void ShadowMapGenerationEffect::SetAlphaDiscardTexture(ID3D11ShaderResourceView * pTexture)
{
	pImpl->texture = pTexture;
}

void ShadowMapGenerationEffect::DisableAlphaDiscard()
{
	pImpl->texture = nullptr;
}

void XM_CALLCONV ShadowMapGenerationEffect::SetWorld(FXMMATRIX value)
{
	pImpl->matrices.world = value;

	pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV ShadowMapGenerationEffect::SetView(FXMMATRIX value)
{
	pImpl->matrices.view = value;

	pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV ShadowMapGenerationEffect::SetProjection(FXMMATRIX value)
{
	pImpl->matrices.projection = value;

	pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}

void ShadowMapGenerationEffect::SetWeightsPerVertex(int value)
{
	if ((value != 0) &&
		(value != 1) &&
		(value != 2) &&
		(value != 4))
	{
		throw std::out_of_range("WeightsPerVertex must be 0, 1, 2, or 4");
	}

	pImpl->weightsPerVertex = value;
}


void ShadowMapGenerationEffect::SetBoneTransforms(_In_reads_(count) XMMATRIX const* value, size_t count)
{
	if (count > MaxBones)
		throw std::out_of_range("count parameter out of range");

	auto& boneConstant = pImpl->constants.Bones;

	for (size_t i = 0; i < count; i++)
	{
		XMMATRIX boneMatrix = XMMatrixTranspose(value[i]);
		XMStoreFloat3x4(&boneConstant[i], boneMatrix);
	}

	pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void ShadowMapGenerationEffect::ResetBoneTransforms()
{
	auto boneConstant = pImpl->constants.Bones;

	XMMATRIX id = XMMatrixIdentity();

	for (size_t i = 0; i < MaxBones; ++i)
	{
		XMStoreFloat3x4(&boneConstant[i], id);
	}

	pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}



void ShadowMapGenerationEffect::Apply(ID3D11DeviceContext * deviceContext)
{
	pImpl->Apply(deviceContext);
	//ApplyShaders(deviceContext, m_pImpl->GetCurrentShaderPermutation());
}

void ShadowMapGenerationEffect::GetVertexShaderBytecode(void const ** pShaderByteCode, size_t * pByteCodeLength)
{
	auto perm = pImpl->GetCurrentShaderPermutation();
	pImpl->GetVertexShaderBytecode(perm, pShaderByteCode, pByteCodeLength);
}
