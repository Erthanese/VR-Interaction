#pragma once
#include "SceneObject.h"
#include "Armature.h"
#include "CharacterBehavier.h"

namespace Causality
{
	// Represent an Character that have intrinsic animation
	class CharacterObject : public VisualObject
	{
	public:
		typedef BehavierSpace::frame_type frame_type;
		typedef vector<BoneVelocity, DirectX::XMAllocator> velocity_frame_type;

		CharacterObject();
		~CharacterObject();

		void							EnabeAutoDisplacement(bool is_enable);

		const frame_type&				GetCurrentFrame() const;
		frame_type&						MapCurrentFrameForUpdate();
		void							ReleaseCurrentFrameFrorUpdate();

		IArmature&						Armature();
		const IArmature&				Armature() const;
		BehavierSpace&					Behavier();
		const BehavierSpace&			Behavier() const;
		void							SetBehavier(BehavierSpace& behaver);

		const ArmatureFrameAnimation*	CurrentAction() const;
		string							CurrentActionName() const;
		bool							StartAction(const string& key, time_seconds begin_time = time_seconds(0), bool loop = false, time_seconds transition_time = time_seconds(0));
		bool							StopAction(time_seconds transition_time = time_seconds(0));

		bool							IsFreezed() const;
		void							SetFreeze(bool freeze);

		virtual void					SetRenderModel(DirectX::Scene::IModelNode* pMesh, int LoD = 0) override;

		virtual void					Update(time_seconds const& time_delta) override;

		// Inherited via IRenderable
		virtual RenderFlags GetRenderFlags() const override;
		virtual bool IsVisible(const BoundingGeometry& viewFrustum) const override;
		virtual void Render(RenderContext & pContext, DirectX::IEffect* pEffect = nullptr) override;
		virtual void XM_CALLCONV UpdateViewMatrix(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection) override;

	protected:
		// Automatic displacement by analyze joints contact ground
		void							DisplaceByVelocityFrame();
		void							ComputeVelocityFrame(time_seconds time_delta);

	private:
		BehavierSpace*					        m_pBehavier;
		IArmature*								m_pArmature;
		BehavierSpace::animation_type*			m_pCurrentAction;
		BehavierSpace::animation_type*			m_pLastAction;
		time_seconds							m_CurrentActionTime;
		bool									m_LoopCurrentAction;

		int										m_FrameMapState;
		frame_type						        m_CurrentFrame;
		frame_type								m_LastFrame;
		velocity_frame_type						m_VelocityFrame;

		bool									m_UpdateLock;
		bool									m_IsAutoDisplacement;
	};

	void DrawArmature(const IArmature & armature, const AffineFrame & frame, const Color & color, const Matrix4x4& world = Matrix4x4::Identity, float thinkness = 0.015f);
}
