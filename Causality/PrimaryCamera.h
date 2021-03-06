#pragma once
#include "OculusRift.h"

// A Camera setup which works both in the case with OculusRift and Normal Monolith Camera
__declspec(align(16))
class PlayerAttachedCamera : public DirectX::AlignedNew<PlayerAttachedCamera>, public DirectX::Scene::ICameraBase , public DirectX::Scene::ICameraRenderControl , public DirectX::Scene::ICameraParameters
{
public:
	PlayerAttachedCamera(const std::shared_ptr<DirectX::DeviceResources> &resources = nullptr)
		: m_pDeviceResources(resources)
	{
	}

	~PlayerAttachedCamera()
	{
	}

	inline void EnableStereo(const std::shared_ptr<Platform::Devices::OculusRift>& pRift)
	{
		m_pRift = pRift;
	}

	inline void DisableStoreo()
	{
		m_pRift = nullptr;
	}

	inline bool IsStereoEnabled() const
	{
		return m_pRift != nullptr;
	}

	// Inherited via ICameraRenderControl
	virtual void BeginFrame() override;

	virtual void EndFrame() override;

	// Only need for Stereo or more camera
	virtual void SetView(size_t view) override;

	// This ignore the effect of Oculus
	virtual void FocusAt(DirectX::FXMVECTOR focusPoint, DirectX::FXMVECTOR upDir);
	// Inherited via ICamera
	virtual size_t ViewCount() const override;
	virtual DirectX::XMMATRIX GetViewMatrix(size_t view) const override;
	virtual DirectX::XMMATRIX GetProjectionMatrix(size_t view) const override;
	// Inherited via ICameraParameters
	virtual void SetFov(float fovRadius, float aspectRatioHbyW) override;
	virtual float GetFov() const override;
	virtual const Platform::Fundation::Vector3&    GetPosition() const override;
	virtual void  SetPosition(const Platform::Fundation::Vector3& p) override;
	virtual const Platform::Fundation::Quaternion& GetOrientation() const override;
	virtual void  SetOrientation(const Platform::Fundation::Quaternion &q) override;
	//virtual void XM_CALLCONV Move(DirectX::FXMVECTOR p) override;
	//virtual void XM_CALLCONV Rotate(DirectX::FXMVECTOR q) override;
private:
	DirectX::XMVECTORF32 Foward = DirectX::g_XMNegIdentityR2, Up = DirectX::g_XMIdentityR1;
	Platform::Fundation::StaticPose m_BodyPose;
	float Fov, AspectRatio;
	std::shared_ptr<Platform::Devices::OculusRift>	m_pRift;
	std::shared_ptr<DirectX::DeviceResources>		m_pDeviceResources;




};

