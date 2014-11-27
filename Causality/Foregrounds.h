#pragma once
#include "Common\Model.h"
#include "Common\DeviceResources.h"
#include <Effects.h>
#include "Interactive.h"
#include <wrl\client.h>

namespace Causality
{
	class Foregrounds : public DirectX::Scene::IRenderable, public DirectX::Scene::ModelCollection, public DirectX::Scene::IViewable , public DirectX::Scene::ITimeAnimatable
	{
	public:
		Foregrounds(const std::shared_ptr<DirectX::DeviceResources> &pResouce);
		~Foregrounds();

		void LoadAsync(ID3D11Device* pDevice);

		std::shared_ptr<DirectX::BasicEffect> pEffect;

		//virtual DirectX::XMMATRIX GetModelMatrix() const override;
		// Inherited via IRenderable
		virtual void Render(ID3D11DeviceContext * pContext) override;

		// Inherited via IViewable
		virtual void XM_CALLCONV UpdateViewMatrix(DirectX::FXMMATRIX view) override;
		virtual void XM_CALLCONV UpdateProjectionMatrix(DirectX::FXMMATRIX projection) override;

		// Inherited via ITimeAnimatable
		virtual void UpdateAnimation(DirectX::StepTimer const & timer) override;

	private:
		Microsoft::WRL::ComPtr<ID3D11InputLayout>	pInputLayout;
		bool m_loadingComplete;
	};
}
