#include "pch_bcl.h"
#include "CameraObject.h"
#include <DirectXColors.h>
#include <ShadowMapGenerationEffect.h>
#include <ShadowMapEffect.h>
#include <PostProcessingEffect.h>
#include "OculusRift.h"

using namespace DirectX;
using namespace Causality;

const XMVECTORF32 CameraViewControl::Foward = g_XMNegIdentityR2, CameraViewControl::Up = g_XMIdentityR1;

IEffect * IRenderControl::GetRenderEffect() { return nullptr; }

XMMATRIX CameraViewControl::GetProjectionMatrix() const
{
	return UpdateProjectionCache();
}

XMMATRIX CameraViewControl::UpdateProjectionCache() const
{
	if (m_ProjectDirty)
	{
		using namespace DirectX;
		m_ProjectDirty = false;
		XMMATRIX proj;
		if (m_IsPerspective)
		{
			proj = XMMatrixPerspectiveFovRH(m_Fov, m_AspectRatio, m_Near, m_Far);
			m_ViewFrutum.Type = BoundingGeometryType::Geometry_Frustum;
			BoundingFrustumExtension::CreateFromMatrixRH(m_ViewFrutum.Frustum, proj);
		}
		else
		{
			proj = XMMatrixOrthographicRH(m_Fov * m_AspectRatio, m_Fov, m_Near, m_Far);
			m_ViewFrutum.Type = BoundingGeometryType::Geometry_OrientedBox;
			m_ViewFrutum.OrientedBox.Center = { 0, 0, -0.5f * (m_Near + m_Far) };
			m_ViewFrutum.OrientedBox.Extents = { 0.5f * m_Fov * m_AspectRatio, 0.5f* m_Fov, 0.5f * abs(m_Near - m_Far) };
			m_ViewFrutum.OrientedBox.Orientation = Quaternion::Identity;
		}

		XMStoreFloat4x4(&m_ProjectionCache, proj);
		return proj;
	}
	else
		return m_ProjectionCache;
}

const BoundingGeometry & CameraViewControl::GetViewFrustum() const {
	if (m_ProjectDirty)
	{
		UpdateProjectionCache();
	}
	XMMATRIX transform = m_Parent->GetRigidTransformMatrix();

	XMVECTOR cF = XMVector3TransformCoord(Foward.v, transform);

	//if (m_ViewFrutum.Type == Geometry_OrientedBox || m_ViewFrutum.Type == Geometry_AxisAlignedBox)
	//{
	//	// make sure the transform rotation origin is 0,0,0 instead of bounding geometry's center
	//	XMVECTOR vT = XMLoadFloat3(&m_ViewFrutum.OrientedBox.Center);
	//	//transform = XMMatrixTranslationFromVector(-vT) * transform;
	//}
	//else
	//{
	//}

	m_ViewFrutum.Transform(m_ExtrinsicViewFrutum, transform);
	return m_ExtrinsicViewFrutum;
}

// This ignore the effect of Oculus

CameraViewControl::CameraViewControl()
{
	m_ProjectDirty = true;
	m_ViewDirty = true;
	m_Parent = nullptr;
	m_IsRightHand = true;
	m_Forward = Foward.v;
	m_Displacement = g_XMZero.v;
	m_UpDir = Up.v;
}

XMMATRIX CameraViewControl::GetViewMatrix() const
{
	using namespace DirectX;
	XMVECTOR refLoc = (XMVECTOR)m_Parent->GetPosition();
	XMVECTOR rot = m_Parent->GetOrientation();
	XMVECTOR up = XMVector3Rotate(m_UpDir.Load(), rot);
	XMVECTOR forward = XMVector3Rotate(m_Forward.Load(), rot);

	return XMMatrixLookToRH(refLoc, forward, up);
}

void CameraViewControl::FocusAt(FXMVECTOR focusPoint, FXMVECTOR upDir)
{
	using namespace DirectX;
	// Right-Hand Coordinate
	auto pos = m_Parent->GetPosition(); // assume this is a Rigid transform without scale
	auto rot = m_Parent->GetOrientation();

	m_Forward = XMVector3Rotate(focusPoint - pos.Load(), XMQuaternionConjugate(rot.Load()));
	m_Forward -= m_Displacement;
}

void CameraViewControl::SetAttachedRigid(DirectX::IRigid * pRigid)
{
	m_Parent = pRigid;
}

void CameraViewControl::SetPerspective(float fovRadius, float aspectRatioHbyW, float _near, float _far)
{
	m_IsPerspective = true;
	m_ProjectDirty = true;
	m_Fov = fovRadius; 
	m_AspectRatio = aspectRatioHbyW; 
	m_Near = _near; 
	m_Far = _far;
}

void CameraViewControl::SetOrthographic(float viewWidth, float viewHeight, float _near, float _far)
{
	m_IsPerspective = false;
	m_ProjectDirty = true;
	m_Fov = viewHeight; 
	m_AspectRatio = viewWidth / viewHeight;
	m_Near = _near; 
	m_Far = _far;
}

bool CameraViewControl::IsPerspective() const { return m_IsPerspective; }

float CameraViewControl::GetFov() const
{
	return m_Fov;
}

float CameraViewControl::GetAspectRatio() const { return m_AspectRatio; }

float CameraViewControl::GetNear() const { return m_Near; }

float CameraViewControl::GetFar() const { return m_Far; }

void Causality::CameraViewControl::SetFov(float fov) { m_Fov = fov; m_ProjectDirty = true; }

void Causality::CameraViewControl::SetAspectRatio(float aspectHbyW) { 
	m_AspectRatio = aspectHbyW; 
	m_ProjectDirty = true;
}

void Causality::CameraViewControl::SetNear(float _near) { m_Near = _near; m_ProjectDirty = true; }

void Causality::CameraViewControl::SetFar(float _far) { m_Far = _far; m_ProjectDirty = true; }

// Only need for Stereo or more camera


// Inherited via IRenderControl

EffectRenderControl::EffectRenderControl()
	: m_RequstFlags(RenderFlags::Visible), m_IfClearRenderTarget(true)
{
}

 EffectRenderControl::~EffectRenderControl()
{
}

 void EffectRenderControl::Begin(IRenderContext * context)
{
	m_pRenderContext = context;
	m_HaveItemRendered = false;
	auto pContext = m_pRenderContext.Get();
	if (m_IfClearRenderTarget)
		m_RenderTarget.Clear(pContext, m_Background);
	m_RenderTarget.SetAsRenderTarget(pContext);
}

 void EffectRenderControl::End()
{
	// if no item is rendered, no need to call post processing effect
	if (m_pPostEffect && m_HaveItemRendered)
		if (m_PostEffectOutput == nullptr)
			m_pPostEffect->Apply(m_pRenderContext.Get(), m_RenderTarget.ColorBuffer());
		else
			m_pPostEffect->Apply(m_pRenderContext.Get(), m_PostEffectOutput, m_RenderTarget.ColorBuffer(), m_RenderTarget.ViewPort(), m_RenderTarget.ViewPort());
	m_pRenderContext = nullptr;
}

 bool EffectRenderControl::AcceptRenderFlags(RenderFlags flags)
{
	bool result = flags.Contains(m_RequstFlags);
	m_HaveItemRendered |= result;
	return result;
}

 void EffectRenderControl::SetView(IViewControl * pViewControl) {
	auto pEffectMatrices = dynamic_cast<DirectX::IEffectMatrices*>(m_pEffect.get());
	if (pEffectMatrices && pViewControl)
	{
		pEffectMatrices->SetView(pViewControl->GetViewMatrix());
		pEffectMatrices->SetProjection(pViewControl->GetProjectionMatrix());
	}
}

 DirectX::IEffect * EffectRenderControl::GetRenderEffect() { return m_pEffect.get(); }

 DirectX::IPostEffect * Causality::EffectRenderControl::GetPostEffect()
 {
	 return m_pPostEffect.get();
 }

 void EffectRenderControl::SetRequestRenderFlags(RenderFlags flags) {
	m_RequstFlags = flags;
}

 void EffectRenderControl::SetRenderEffect(const std::shared_ptr<DirectX::IEffect>& pEffect) { m_pEffect = pEffect; }

 void EffectRenderControl::SetPostEffect(const std::shared_ptr<DirectX::IPostEffect>& pPostEffect) { m_pPostEffect = pPostEffect; }

 void EffectRenderControl::SetRenderTargetClearence(bool ifClear)
 {
	 m_IfClearRenderTarget = ifClear;
 }

 Color EffectRenderControl::GetBackground() const { return m_Background; }

 void EffectRenderControl::SetBackground(const Color & color) { m_Background = color; }

 DirectX::RenderTarget & EffectRenderControl::GetRenderTarget() {
	return m_RenderTarget;
}

 DirectX::Texture2D & Causality::EffectRenderControl::GetOutput()
 {
	 if (m_PostEffectOutput == nullptr)
		 return m_RenderTarget.ColorBuffer();
	 else
		 return m_PostEffectOutput;
 }

 void EffectRenderControl::SetRenderTarget(DirectX::RenderTarget & renderTarget) {
	 m_RenderTarget = renderTarget; 
 }

 void Causality::EffectRenderControl::SetPostEffectOutput(DirectX::RenderableTexture2D & output)
 {
	 m_PostEffectOutput = output;
 }

 XMVECTOR XM_CALLCONV GetOrientationFromFocus(FXMVECTOR origin, FXMVECTOR focus, FXMVECTOR up)
 {
	 XMVECTOR foward = focus - origin; // -Z
	 XMMATRIX M;
	 M.r[2] = -XMVector3Normalize(foward);
	 M.r[1] = XMVector3Normalize(up);
	 M.r[0] = XMVector3Cross(M.r[1], M.r[2]); // X = YxZ, for rh coords

	 // for the case up is not orthogal with foward
	 M.r[1] = XMVector3Cross(M.r[2], M.r[0]); // Y = ZxX
	 M.r[3] = g_XMNegIdentityR3.v;

	 // M' is the rotation of StandardView -> CurrentView Orientation
	 XMVECTOR q = XMQuaternionRotationMatrix(M);
	 XMVECTOR tF = XMVector3Transform(-g_XMIdentityR2.v, M);
	 //q = XMQuaternionConjugate(q);
	 return q;
 }

 Camera::Camera()
 {
 }

 size_t Causality::Camera::ViewRendererCount(int view) const { return 1; }

 IRenderControl * Causality::Camera::GetViewRenderer(int view, int renderer) { return this; }
 
 void Camera::SetRenderTarget(DirectX::RenderTarget & renderTarget)
 {
	 float rtvAspect = renderTarget.ViewPort().Width / renderTarget.ViewPort().Height;
	 if (abs(GetAspectRatio() - rtvAspect) > 0.01f)
		 SetAspectRatio(rtvAspect);
	 EffectRenderControl::SetRenderTarget(renderTarget);
 }

 size_t Causality::SingleViewCamera::ViewCount() const { return 1; }

 IViewControl * Causality::SingleViewCamera::GetView(int view) { return this; }

 void SingleViewCamera::FocusAt(DirectX::FXMVECTOR focusPoint, DirectX::FXMVECTOR upDir)
{
	XMVECTOR origin = GetPosition();
	XMVECTOR q = GetOrientationFromFocus(origin, focusPoint, upDir);
	SetOrientation(q);
}

 void SingleViewCamera::BeginFrame()
 {
 }

 void SingleViewCamera::EndFrame()
 {
 }

SoftShadowCamera::SoftShadowCamera(IRenderDevice * pDevice, DirectX::RenderTarget& canvas)
{
	using namespace DirectX;
	auto resolution = canvas.Bounds();
	// pass 0 : render binary shadow to screen space (from light-view-proj space)
	auto pEffectRender = std::make_shared<EffectRenderControl>();
	pEffectRender->SetRenderTarget(RenderTarget(pDevice, resolution.x, resolution.y));
	pEffectRender->SetBackground(g_XMOne.v);

	auto& shadowBuffer = pEffectRender->GetRenderTarget().ColorBuffer();
	auto pShadowEffect = std::make_shared<ShadowMapEffect>(pDevice);
	pShadowEffect->SetEffectMode(ShadowMapEffect::ScreenSpaceShadowGeneration);
	pEffectRender->SetRenderEffect(pShadowEffect);
	pEffectRender->SetRequestRenderFlags(RenderFlags::OpaqueObjects);
	pEffectRender->SetPostEffectOutput(RenderableTexture2D(pDevice, resolution.x, resolution.y));
	auto& bluredShadowBuffer = pEffectRender->GetOutput();
	// pass 0.5 : blur screen space shadow map
	auto pBlurEffect = std::make_shared<DirectX::GuassianBlurEffect>(pDevice);
	pEffectRender->SetPostEffect(pBlurEffect);
	pBlurEffect->ResizeBufferRespectTo(pDevice, canvas.ColorBuffer());
	pBlurEffect->SetBlurRadius(0.6f);
	m_pRenderers.push_back(pEffectRender);

	// pass 1 : render objects with screen space shadow map
	pEffectRender = std::make_shared<EffectRenderControl>();
	pEffectRender->SetRenderTargetClearence(true);
	pEffectRender->SetRenderTarget(canvas);
	pEffectRender->SetBackground(Colors::Gray.v);
	pShadowEffect = std::make_shared<ShadowMapEffect>(pDevice);
	pShadowEffect->SetEffectMode(ShadowMapEffect::ScreenSpaceShadowRender); // Second shadow effect for render
	pShadowEffect->SetScreenSpaceLightsShadowMap(shadowBuffer.ShaderResourceView(), bluredShadowBuffer.ShaderResourceView());
	pEffectRender->SetRenderEffect(pShadowEffect); // Use default effect
	pEffectRender->SetRequestRenderFlags(RenderFlags::Visible); // minium requirement
	m_pRenderers.push_back(pEffectRender);

	// pass 2 : render highlights (blooming) effect
	pEffectRender = std::make_shared<EffectRenderControl>();
	auto pShadowGenEffect = std::make_shared<ShadowMapGenerationEffect>(pDevice);
	pShadowGenEffect->SetShadowFillMode(ShadowMapGenerationEffect::BoneColorFill);
	pShadowGenEffect->SetShadowColor(Colors::LimeGreen);
	pEffectRender->SetRenderEffect(pShadowGenEffect);
	pEffectRender->SetRenderTarget(RenderTarget(pDevice, resolution.x, resolution.y));

	pBlurEffect = std::make_shared<DirectX::GuassianBlurEffect>(pDevice);
	pEffectRender->SetBackground(Colors::Transparent.v);
	pEffectRender->SetPostEffect(pBlurEffect);
	pBlurEffect->ResizeBufferRespectTo(pDevice, canvas.ColorBuffer());
	pBlurEffect->SetBlurRadius(2.5f);
	pBlurEffect->SetMultiplier(0.5f);
	pBlurEffect->SetOutputMode(AlphaBlendNoDepth);
	pBlurEffect->SetOutputDepthStencil(canvas.DepthBuffer().DepthStencilView());
	pEffectRender->SetPostEffect(pBlurEffect);
	pEffectRender->SetRequestRenderFlags(RenderFlags::BloomEffectSource | RenderFlags::Skinable); // minium requirement
	pEffectRender->SetPostEffectOutput(canvas.ColorBuffer());
	m_pRenderers.push_back(pEffectRender);

}

PercentCloserShadowCamera::PercentCloserShadowCamera(IRenderDevice * pDevice, RenderTarget & canvas)
{
	using namespace DirectX;
	auto resolution = canvas.Bounds();
	// pass 0 : render binary shadow to screen space (from light-view-proj space)
	auto pEffectRender = std::make_shared<EffectRenderControl>();
	pEffectRender->SetRenderTarget(RenderTarget(pDevice, resolution.x, resolution.y));
	pEffectRender->SetBackground(g_XMOne.v);

	auto& shadowBuffer = pEffectRender->GetRenderTarget().ColorBuffer();
	auto pShadowEffect = std::make_shared<ShadowMapEffect>(pDevice);
	pShadowEffect->SetEffectMode(ShadowMapEffect::LightSpaceShadowRender);
	pEffectRender->SetRenderTargetClearence(true);
	pEffectRender->SetRenderTarget(canvas);
	pEffectRender->SetRenderEffect(pShadowEffect);
	pEffectRender->SetRequestRenderFlags(RenderFlags::Visible);
	m_pRenderers.push_back(pEffectRender);

	// pass 2 : render highlights (blooming) effect
	pEffectRender = std::make_shared<EffectRenderControl>();
	auto pShadowGenEffect = std::make_shared<ShadowMapGenerationEffect>(pDevice);
	pShadowGenEffect->SetShadowFillMode(ShadowMapGenerationEffect::BoneColorFill);
	pShadowGenEffect->SetShadowColor(Colors::LimeGreen);
	pEffectRender->SetRenderEffect(pShadowGenEffect);
	pEffectRender->SetRenderTarget(RenderTarget(pDevice, resolution.x, resolution.y));

	auto pBlurEffect = std::make_shared<DirectX::GuassianBlurEffect>(pDevice);
	pEffectRender->SetBackground(Colors::Transparent.v);
	pEffectRender->SetPostEffect(pBlurEffect);
	pBlurEffect->ResizeBufferRespectTo(pDevice, canvas.ColorBuffer());
	pBlurEffect->SetBlurRadius(2.5f);
	pBlurEffect->SetMultiplier(0.5f);
	pBlurEffect->SetOutputMode(AlphaBlendNoDepth);
	pBlurEffect->SetOutputDepthStencil(canvas.DepthBuffer().DepthStencilView());
	pEffectRender->SetPostEffect(pBlurEffect);
	pEffectRender->SetRequestRenderFlags(RenderFlags::BloomEffectSource | RenderFlags::Skinable); // minium requirement
	pEffectRender->SetPostEffectOutput(canvas.ColorBuffer());
	m_pRenderers.push_back(pEffectRender);
}

size_t MultipassCamera::ViewRendererCount(int view) const { return m_pRenderers.size(); }

IRenderControl * MultipassCamera::GetViewRenderer(int view, int renderer) { return m_pRenderers[renderer].get(); }

void MultipassCamera::AddEffectRenderControl(const shared_ptr<EffectRenderControl>& pRenderer) { m_pRenderers.push_back(pRenderer); }

size_t MuiltiviewCamera::ViewCount() const { return m_Views.size(); }

IViewControl * MuiltiviewCamera::GetView(int view) { return m_Views[view].get(); }

size_t MuiltiviewCamera::ViewRendererCount(int view) const { return m_ViewportRenderers[view].size(); }

IRenderControl * MuiltiviewCamera::GetViewRenderer(int view, int renderer) {
	auto pRenderer = m_pRenderers[m_ViewportRenderers[view][renderer]].get();
	pRenderer->SetRenderTarget(m_RenderTargets[view]);
	return pRenderer;
}

void Causality::MuiltiviewCamera::BeginFrame()
{
}

void Causality::MuiltiviewCamera::EndFrame()
{
}

HMDCamera::HMDCamera(IRenderDevice *pDevice, RenderTarget & canvas)
{
	using namespace DirectX;
	auto resolution = canvas.Bounds();
	// pass 0 : render binary shadow to screen space (from light-view-proj space)
	auto pEffectRender = std::make_shared<EffectRenderControl>();
	pEffectRender->SetRenderTarget(RenderTarget(pDevice, resolution.x, resolution.y));
	pEffectRender->SetBackground(g_XMOne.v);

	auto& shadowBuffer = pEffectRender->GetRenderTarget().ColorBuffer();
	auto pShadowEffect = std::make_shared<ShadowMapEffect>(pDevice);
	pShadowEffect->SetEffectMode(ShadowMapEffect::LightSpaceShadowRender);
	pEffectRender->SetRenderTargetClearence(true);
	pEffectRender->SetRenderTarget(canvas);
	pEffectRender->SetRenderEffect(pShadowEffect);
	pEffectRender->SetRenderTargetClearence(false);
	pEffectRender->SetRequestRenderFlags(RenderFlags::Visible);
	m_pRenderers.push_back(pEffectRender);

	m_ViewportRenderers.resize(2);
	m_ViewportRenderers[0] = { 0 };
	m_ViewportRenderers[1] = { 0 };

	m_Views.emplace_back(new CameraViewControl());
	m_Views.emplace_back(new CameraViewControl());

	m_Views[0]->SetAttachedRigid(this);
	m_Views[1]->SetAttachedRigid(this);

	auto viewport = canvas.ViewPort();
	viewport.Width /= 2;
	m_RenderTargets.resize(2);
	m_RenderTargets[0] = canvas.Subview(viewport);
	viewport.TopLeftX += viewport.Width;
	m_RenderTargets[1] = canvas.Subview(viewport);

	pDevice->GetImmediateContext(&m_pContext);
}

void HMDCamera::BeginFrame()
{
	if (!m_pRift)
		m_RenderTargets[0].Clear(m_pContext.Get(), g_XMZero.v);
	//else

}

void HMDCamera::EndFrame()
{
	//if (m_pRift)
		//m_pRift->EndFrame();
}
