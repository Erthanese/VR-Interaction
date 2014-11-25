#include "Player.h"
#include <DirectXColors.h>
using namespace DirectX;

size_t PlayerCamera::ViewCount() const
{
	return IsStereoEnabled() ? 2U : 1U;
}

DirectX::XMMATRIX PlayerCamera::GetViewMatrix(size_t view) const
{
	assert(view < ViewCount());
	if (IsStereoEnabled())
	{
		auto eye = (DirectX::Scene::EyesEnum) view;
		auto pose = m_pRift->EyePoses(eye);
		XMVECTOR loc = (XMVECTOR) Position() + DirectX::XMVector3Rotate((XMVECTOR) pose.Position, pose.Orientation);
		XMVECTOR rot = Orientation() * pose.Orientation;
		XMVECTOR foward = XMVector3Rotate(Foward, rot);
		XMVECTOR up = XMVector3Rotate(Up, rot);
		return XMMatrixLookToRH(loc, foward, up);
	}
	else
	{
		XMVECTOR loc = (XMVECTOR) Position();
		XMVECTOR rot = Orientation();
		XMVECTOR foward = XMVector3Rotate(Foward, rot);
		XMVECTOR up = XMVector3Rotate(Up, rot);
		return XMMatrixLookToRH(loc, foward, up);
	}
}

DirectX::XMMATRIX PlayerCamera::GetProjectionMatrix(size_t view) const
{
	assert(view < ViewCount());
	if (IsStereoEnabled())
	{
		auto eye = (DirectX::Scene::EyesEnum) view;
		return m_pRift->EyeProjection(eye);
	}
	else
	{
		return XMMatrixPerspectiveFovRH(Fov, AspectRatio, 0.01f, 100.0f);
	}
}

// This ignore the effect of Oculus

void PlayerCamera::FocusAt(DirectX::FXMVECTOR focusPoint, DirectX::FXMVECTOR upDir)
{
	using namespace DirectX;
	Foward = XMVector3Normalize(focusPoint - (XMVECTOR) Position());
	Up = upDir;
	SetOrientation(XMQuaternionIdentity());
}

const Platform::Fundation::Vector3 & PlayerCamera::Position() const
{
	return m_BodyPose.Position;
}

void PlayerCamera::SetPosition(const Platform::Fundation::Vector3 &p)
{
	m_BodyPose.Position = p;
}

const Platform::Fundation::Quaternion & PlayerCamera::Orientation() const
{
	return m_BodyPose.Orientation;
}

void PlayerCamera::SetOrientation(const Platform::Fundation::Quaternion &q)
{
	m_BodyPose.Orientation = q;
}

void PlayerCamera::BeginFrame()
{
	if (m_pRift)
	{
		m_pRift->BeginFrame();
	} else
	{
		auto context = m_pDeviceResources->GetD3DDeviceContext();
		// Reset the viewport to target the whole screen.
		auto viewport = m_pDeviceResources->GetScreenViewport();
		context->RSSetViewports(1, &viewport);
		ID3D11RenderTargetView *const targets[1] = { m_pDeviceResources->GetBackBufferRenderTargetView() };
		context->OMSetRenderTargets(1, targets, m_pDeviceResources->GetDepthStencilView());
		context->ClearRenderTargetView(m_pDeviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::White);
		context->ClearDepthStencilView(m_pDeviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}

void PlayerCamera::EndFrame()
{
	if (m_pRift)
	{
		m_pRift->EndFrame();
	}
	else
	{
		m_pDeviceResources->Present();
	}
}

void PlayerCamera::SetView(size_t view)
{
	if (m_pRift)
	{
		m_pRift->EyeTexture((DirectX::Scene::EyesEnum) view).SetAsRenderTarget(m_pDeviceResources->GetD3DDeviceContext(), m_pRift->DepthStencilBuffer());
	}
}

//DirectX::XMMATRIX Player::GetViewMatrix() const
//{
//	using namespace DirectX;
//	XMVECTOR loc = (XMVECTOR) Position();
//	XMVECTOR rot = Orientation();
//	XMVECTOR foward = XMVector3Rotate(Foward, rot);
//	XMVECTOR up = XMVector3Rotate(Up, rot);
//	return XMMatrixLookToRH(loc, foward, up);
//}
//
//DirectX::XMMATRIX Player::GetProjectionMatrix() const
//{
//	using namespace DirectX;
//	return XMMatrixPerspectiveFovRH(Fov, AspectRatio, 0.01f,100.0f);
//}

void PlayerCamera::SetFov(float fovRadius, float aspectRatioHbyW)
{
	Fov = fovRadius; AspectRatio = aspectRatioHbyW;
}
