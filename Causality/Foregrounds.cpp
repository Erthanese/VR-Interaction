#include "pch_bullet.h"
#include <iostream>
#include "Foregrounds.h"
#include "CausalityApplication.h"
#include "Common\PrimitiveVisualizer.h"

using namespace Causality;
using namespace DirectX;
using namespace DirectX::Scene;
using namespace std;
using namespace Platform;
using namespace Platform::Fundation;
using namespace Eigen;
using namespace DirectX::Visualizers;
extern wstring ResourcesDirectory;

std::unique_ptr<DirectX::GeometricPrimitive> HandPhysicalModel::s_pCylinder;
std::unique_ptr<DirectX::GeometricPrimitive> HandPhysicalModel::s_pSphere;

const static wstring SkyBoxTextures[6] = {
	ResourcesDirectory + wstring(L"Textures\\SkyBox\\GrimmNight\\Right.dds"),
	ResourcesDirectory + wstring(L"Textures\\SkyBox\\GrimmNight\\Left.dds"),
	ResourcesDirectory + wstring(L"Textures\\SkyBox\\GrimmNight\\Top.dds"),
	ResourcesDirectory + wstring(L"Textures\\SkyBox\\GrimmNight\\Bottom.dds"),
	ResourcesDirectory + wstring(L"Textures\\SkyBox\\GrimmNight\\Front.dds"),
	ResourcesDirectory + wstring(L"Textures\\SkyBox\\GrimmNight\\Back.dds"),
};

std::unique_ptr<btBroadphaseInterface>               pBroadphase = nullptr;
// Set up the collision configuration and dispatcher
std::unique_ptr<btDefaultCollisionConfiguration>     pCollisionConfiguration = nullptr;
std::unique_ptr<btCollisionDispatcher>               pDispatcher = nullptr;
// The actual physics solver
std::unique_ptr<btSequentialImpulseConstraintSolver> pSolver = nullptr;


float ShapeSimiliarity(const Eigen::VectorXf& v1, const Eigen::VectorXf& v2)
{
	auto v = v1 - v2;
	//dis = sqrt(v.dot(v);
	//1.414 - dis;
	auto theta = acosf(v1.dot(v2) * invsqrt(v1.dot(v1) * v2.dot(v2))); // Difference in angular [0,pi/4]
	auto rhlo = abs(sqrt(v1.dot(v1)) - sqrt(v2.dot(v2)));	// Difference in length [0,sqrt(2)]
	return 1.0f - 0.5f * (0.3f * rhlo / sqrt(2) + 0.7f * theta / (XM_PIDIV4));
}

Causality::WorldScene::WorldScene(const std::shared_ptr<DirectX::DeviceResources>& pResouce, const DirectX::ILocatable* pCamera)
	: States(pResouce->GetD3DDevice())
	, m_pCameraLocation(pCamera)
{
	m_HaveHands = false;
	m_showTrace = true;
	LoadAsync(pResouce->GetD3DDevice());
}

WorldScene::~WorldScene()
{
}

const float fingerRadius = 0.01f;
const float fingerLength = 0.01f;

class XmlModelLoader
{
};

void Causality::WorldScene::LoadAsync(ID3D11Device* pDevice)
{

	m_loadingComplete = false;
	pBackground = nullptr;

	//CD3D11_DEFAULT d;
	//CD3D11_RASTERIZER_DESC Desc(d);
	//Desc.MultisampleEnable = TRUE;
	//ThrowIfFailed(pDevice->CreateRasterizerState(&Desc, &pRSState));

	concurrency::task<void> load_models([this, pDevice]() {
		// Build the broad phase
		pBroadphase = make_unique<btDbvtBroadphase>();

		// Set up the collision configuration and dispatcher
		pCollisionConfiguration = make_unique<btDefaultCollisionConfiguration>();
		pDispatcher = make_unique<btCollisionDispatcher>(pCollisionConfiguration.get());

		// The actual physics solver
		pSolver = make_unique<btSequentialImpulseConstraintSolver>();

		// The world.
		pDynamicsWorld.reset(new btDiscreteDynamicsWorld(pDispatcher.get(), pBroadphase.get(), pSolver.get(), pCollisionConfiguration.get()));
		pDynamicsWorld->setGravity(btVector3(0, -9.8f, 0));

		//pGroundShape = make_shared<btStaticPlaneShape>(btVector3{ 0,1,0.5},-0.5);
		//pGroundShape.reset(new btBoxShape(btVector3{ 100,0.01f,100 }));
		//btTransform trans = btTransform::getIdentity();
		//trans.setOrigin(btVector3(0, -0.01f, 0));
		//trans.setRotation(vector_cast<btQuaternion>(XMQuaternionRotationRollPitchYaw(0, 0, 0)));
		//AlignedAllocator<btRigidBody> allocator;
		//pGroundRigid = allocate_shared<btRigidBody>(allocator, 0, new btDefaultMotionState(trans), pGroundShape.get());
		//pGroundRigid->setFriction(1.0f);
		//pDynamicsWorld->addRigidBody(pGroundRigid.get());
		////pGroundRigid->setAngularVelocity(btVector3(0.1f, 0, 0));
		////pGroundRigid->setLinearVelocity({ 0,-1.0f,0 });
		//pGroundRigid->setGravity({ 0,0,0 });
		auto pShape = make_shared<btSphereShape>(0.01f);
		//for (auto& pRigid : m_HandRigids)
		//{
		//	pRigid = make_shared<PhysicalRigid>();
		//	//auto pShape = make_shared<btCylinderShape>()
		//	pRigid->InitializePhysics(nullptr, pShape,1000);
		//	pRigid->GetBulletRigid()->setGravity({ 0,0,0 });
		//}

		auto Directory = App::Current()->GetResourcesDirectory();
		auto ModelDirectory = Directory / "Models";
		auto TextureDirectory = Directory / "Textures";
		auto texDir = TextureDirectory.wstring();
		pEffect = std::make_shared<BasicEffect>(pDevice);
		pEffect->SetVertexColorEnabled(false);
		pEffect->SetTextureEnabled(true);
		//pEffect->SetLightingEnabled(true);
		pEffect->EnableDefaultLighting();
		{
			void const* shaderByteCode;
			size_t byteCodeLength;
			pEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
			pInputLayout = CreateInputLayout<VertexPositionNormalTexture>(pDevice, shaderByteCode, byteCodeLength);
		}

		//pBackground = std::make_unique<SkyDome>(pDevice, SkyBoxTextures);

		auto sceneFile = Directory / "Foregrounds.xml";
		tinyxml2::XMLDocument sceneDoc;
		sceneDoc.LoadFile(sceneFile.string().c_str());
		auto scene = sceneDoc.FirstChildElement("scene");
		auto node = scene->FirstChildElement();
		while (node)
		{
			if (!strcmp(node->Name(), "obj"))
			{
				auto path = node->Attribute("src");
				if (path != nullptr && strlen(path) != 0)
				{
					auto model = std::make_shared<Causality::PhysicalGeometryModel>();
					IGeometryModel::CreateFromObjFile(model.get(), pDevice, (ModelDirectory / path).wstring(), texDir);
					XMFLOAT3 v = model->BoundOrientedBox.Extents;
					v.y /= v.x;
					v.z /= v.x;
					m_ModelFeatures[model->Name] = Eigen::Vector2f(v.y, v.z);
					std::cout << "[Model] f(" << model->Name << ") = " << m_ModelFeatures[model->Name] << std::endl;


					btTransform trans;
					shared_ptr<btCompoundShape> pShape(new btCompoundShape());
					//trans.setOrigin(vector_cast<btVector3>(model->BoundOrientedBox.Center));
					//trans.setRotation(vector_cast<btQuaternion>(model->BoundOrientedBox.Orientation));
					//pShape->addChildShape(trans, new btBoxShape(vector_cast<btVector3>(model->BoundOrientedBox.Extents)));
					for (const auto& part : model->Parts)
					{
						trans.setOrigin(vector_cast<btVector3>(part.BoundOrientedBox.Center));
						trans.setRotation(vector_cast<btQuaternion>(part.BoundOrientedBox.Orientation));
						pShape->addChildShape(trans, new btBoxShape(vector_cast<btVector3>(part.BoundOrientedBox.Extents)));
					}
					//shared_ptr<btBoxShape> pShape(new btBoxShape(vector_cast<btVector3>(model->BoundBox.Extents)));

					//shared_ptr<btSphereShape> pShape;
					//pShape.reset(new btSphereShape(model->BoundSphere.Radius));

					float scale = 1.0f;
					float mass = 1.0f;
					Vector3 pos;

					auto attr = node->Attribute("scale");
					if (attr != nullptr)
					{
						stringstream ss(attr);

						ss >> scale;
						//model->SetScale(XMVectorReplicate(scale));
					}
					attr = node->Attribute("position");
					if (attr != nullptr)
					{
						stringstream ss(attr);

						char ch;
						ss >> pos.x >> ch >> pos.y >> ch >> pos.z;
						//model->SetPosition(pos);
					}

					attr = node->Attribute("mass");
					if (attr)
						mass = atof(attr);

					pShape->setLocalScaling(btVector3(scale, scale, scale));
					btVector3 minb, maxb;
					trans.setIdentity();
					//pShape->getAabb(trans, minb, maxb);
					model->InitializePhysics(pDynamicsWorld, pShape, mass, pos, XMQuaternionIdentity());
					model->GetBulletRigid()->setFriction(1.0f);
					//model->GetBulletRigid()->applyTorqueImpulse(btVector3(1.0f, 0, 1.0f));
					//model->SetOrientation(DirectX::Quaternion::CreateFromYawPitchRoll(1.0f,0,1.0f));
					//model->GetBulletRigid()->setAngularVelocity(btVector3(1.0f, 0, 0));
					{
						std::lock_guard<mutex> guard(m_RenderLock);
						AddModel(model);
					}
				}
			}
			else if (!strcmp(node->Name(),"cube"))
			{
				Vector3 extent(1.0f);
				Vector3 pos;
				Color color(255,255,255,255);
				string name("cube");
				float mass = 1.0f;
				auto attr = node->Attribute("extent");
				if (attr != nullptr)
				{
					stringstream ss(attr);
					char ch;
					ss >> extent.x >> ch >> extent.y >> ch >> extent.z;
				}
				attr = node->Attribute("position");
				if (attr != nullptr)
				{
					stringstream ss(attr);

					char ch;
					ss >> pos.x >> ch >> pos.y >> ch >> pos.z;
				}
				attr = node->Attribute("color");
				if (attr != nullptr)
				{
					stringstream ss(attr);

					char ch;
					ss >> color.x >> ch >> color.y >> ch >> color.z;
					if (!ss.eof())
						ss >> ch >> color.w;
					color = color.ToVector4() / 255;
					color.Saturate();
				}
				attr = node->Attribute("name");
				if (attr)
					name = attr;

				attr = node->Attribute("mass");
				if (attr)
					mass = atof(attr);

				auto pModel = make_shared<CubeModel>(name, extent, (XMVECTOR)color);

				XMFLOAT3 v = pModel->BoundOrientedBox.Extents;
				v.y /= v.x;
				v.z /= v.x;
				m_ModelFeatures[pModel->Name] = Eigen::Vector2f(v.y, v.z);

				pModel->InitializePhysics(nullptr, nullptr, mass, pos);
				pModel->Enable(pDynamicsWorld);
				pModel->GetBulletRigid()->setFriction(1.0f);
				{
					std::lock_guard<mutex> guard(m_RenderLock);
					AddModel(pModel);
				}
			}
			node = node->NextSiblingElement();
		}

		//AddModel(GeometryModel::CreateFromObjFile(pDevice, (ModelDirectory / models[2]).wstring(), texDir));

		for (auto& obj : Children)
		{
			//auto& ext = obj->BoundBox.Extents;
			//auto s = max(max(ext.x, ext.y), ext.z);
			////s = 5.0f / s;
			//obj->SetScale({ s ,s ,s });
		}
		m_loadingComplete = true;
	});
}

void Causality::WorldScene::SetViewIdenpendntCameraPosition(const DirectX::ILocatable * pCamera)
{
	m_pCameraLocation = pCamera;
}

void Causality::WorldScene::Render(ID3D11DeviceContext * pContext)
{
	if (pBackground)
		pBackground->Render(pContext);

	{
		pContext->IASetInputLayout(pInputLayout.Get());
		auto pAWrap = States.AnisotropicWrap();
		pContext->PSSetSamplers(0, 1, &pAWrap);
		pContext->RSSetState(pRSState.Get());
		std::lock_guard<mutex> guard(m_RenderLock);
		ModelCollection::Render(pContext, pEffect.get());
	}

	for (const auto& item : m_HandModels)
	{
		if (item.second)
			item.second->Render(pContext, nullptr);
	}

	g_PrimitiveDrawer.Begin();

	Vector3 conners[8];
	BoundingOrientedBox obox;
	BoundingBox box;
	{
		//Draw axias
		g_PrimitiveDrawer.DrawSphere({ 0,0,0,0.02 }, Colors::Red);
		g_PrimitiveDrawer.DrawLine({ -5,0,0 }, { 5,0,0 }, Colors::Red);
		g_PrimitiveDrawer.DrawLine({ 0,-5,0 }, { 0,5,0 }, Colors::Green);
		g_PrimitiveDrawer.DrawLine({ 0,0,-5 }, { 0,0,5 }, Colors::Blue);
		g_PrimitiveDrawer.DrawTriangle({ 5.05f,0,0 }, { 4.95,0.05,0 }, { 4.95,-0.05,0 }, Colors::Red);
		g_PrimitiveDrawer.DrawTriangle({ 5.05f,0,0 }, { 4.95,-0.05,0 }, { 4.95,0.05,0 }, Colors::Red);
		g_PrimitiveDrawer.DrawTriangle({ 5.05f,0,0 }, { 4.95,0,0.05 }, { 4.95,0,-0.05 }, Colors::Red);
		g_PrimitiveDrawer.DrawTriangle({ 5.05f,0,0 }, { 4.95,0,-0.05 }, { 4.95,0,0.05 }, Colors::Red);
		g_PrimitiveDrawer.DrawTriangle({ 0,5.05f,0 }, { -0.05,4.95,0 }, { 0.05,4.95,0 }, Colors::Green);
		g_PrimitiveDrawer.DrawTriangle({ 0,5.05f,0 }, { 0.05,4.95,0 }, { -0.05,4.95,0 }, Colors::Green);
		g_PrimitiveDrawer.DrawTriangle({ 0,5.05f,0 }, { 0.0,4.95,-0.05 }, { 0,4.95,0.05 }, Colors::Green);
		g_PrimitiveDrawer.DrawTriangle({ 0,5.05f,0 }, { 0.0,4.95,0.05 }, { 0,4.95,-0.05 }, Colors::Green);
		g_PrimitiveDrawer.DrawTriangle({ 0,0,5.05f }, { 0.05,0,4.95 }, { -0.05,0,4.95 }, Colors::Blue);
		g_PrimitiveDrawer.DrawTriangle({ 0,0,5.05f }, { -0.05,0,4.95 }, { 0.05,0,4.95 }, Colors::Blue);
		g_PrimitiveDrawer.DrawTriangle({ 0,0,5.05f }, { 0,0.05,4.95 }, { 0,-0.05,4.95 }, Colors::Blue);
		g_PrimitiveDrawer.DrawTriangle({ 0,0,5.05f }, { 0,-0.05,4.95 }, { 0,0.05,4.95 }, Colors::Blue);

		//g_PrimitiveDrawer.DrawQuad({ 1.0f,0,1.0f }, { -1.0f,0,1.0f }, { -1.0f,0,-1.0f }, { 1.0f,0,-1.0f }, Colors::Pink);
		   

		auto& fh = m_HandDescriptionFeature;

		{
			std::lock_guard<mutex> guard(m_RenderLock);
			auto s = Children.size();
			if (m_HaveHands)
				std::cout << "Detail Similarity = {";
			for (size_t i = 0; i < s; i++)
			{
				const auto& model = Children[i];
				model->GetOrientedBoundingBox().GetCorners(conners);
				DrawBox(conners, DirectX::Colors::DarkGreen);
				if (m_HaveHands)
				{
					auto fm = m_ModelFeatures[model->Name];

					auto similarity = ShapeSimiliarity(fm, fh);
					m_ModelDetailSimilarity[model->Name] = similarity;
					std::cout << model->Name << ':' << similarity << " , ";
					Color c = Color::Lerp({ 1,0,0 }, { 0,1,0 }, similarity);
					for (size_t i = 0; i < 8; i++)
					{
						g_PrimitiveDrawer.DrawSphere(conners[i], 0.005 * similarity, c);
					}
				}
				auto pModel = dynamic_cast<IBasicModel*>(model.get());
				XMMATRIX transform = model->GetWorldMatrix();
				if (pModel)
				{
					for (const auto& part : pModel->Parts)
					{
						part.BoundOrientedBox.Transform(obox, transform);
						obox.GetCorners(conners);
						DrawBox(conners, DirectX::Colors::Orange);
					}
				}
			}
		}
		if (m_HaveHands)
			std::cout << '}' << std::endl;
	}


	if (m_HaveHands)
	{

		//for (auto& pRigid : m_HandRigids)
		//{
		//	g_PrimitiveDrawer.DrawSphere(pRigid->GetPosition(), 0.01f, Colors::Pink);
		//}

		auto pCamera = App::Current()->GetPrimaryCamera();

		XMMATRIX leap2world = m_FrameTransform;// XMMatrixScalingFromVector(XMVectorReplicate(0.001f)) * XMMatrixTranslation(0,0,-1.0) * XMMatrixRotationQuaternion(pCamera->GetOrientation()) * XMMatrixTranslationFromVector((XMVECTOR)pCamera->GetPosition());
		std::lock_guard<mutex> guard(m_HandFrameMutex);
		for (const auto& hand : m_Frame.hands())
		{
			auto palmPosition = XMVector3Transform(hand.palmPosition().toVector3<Vector3>(), leap2world);
			g_PrimitiveDrawer.DrawSphere(palmPosition, 0.02f, Colors::YellowGreen);
			//for (const auto& finger : hand.fingers())
			//{
			//	for (size_t i = 0; i < 4; i++)
			//	{
			//		const auto & bone = finger.bone((Leap::Bone::Type)i);
			//		XMVECTOR bJ = XMVector3Transform(bone.prevJoint().toVector3<Vector3>(), leap2world);
			//		XMVECTOR eJ = XMVector3Transform(bone.nextJoint().toVector3<Vector3>(), leap2world);
			//		//if (i == 0)
			//		//	g_PrimitiveDrawer.DrawSphere(bJ, 0.01f, DirectX::Colors::Lime);

			//		//// The unit of leap is millimeter
			//		//g_PrimitiveDrawer.DrawSphere(eJ, 0.01f, DirectX::Colors::Lime);
			//		g_PrimitiveDrawer.DrawLine(bJ, eJ, DirectX::Colors::White);
			//	}
			//}
		}

		// NOT VALIAD!~!!!!!
		if (m_HandTrace.size() > 0 && m_showTrace)
		{
			//auto pJoints = m_HandTrace.linearize();
			m_CurrentHandBoundingBox.GetCorners(conners);
			DrawBox(conners, Colors::LimeGreen);
			m_HandTraceBoundingBox.GetCorners(conners);
			DrawBox(conners, Colors::YellowGreen);
			m_HandTraceModel.Primitives.clear();
			for (int i = m_HandTrace.size() - 1; i >= std::max(0, (int) m_HandTrace.size() - TraceLength); i--)
			{
				const auto& h = m_HandTrace[i];
				float radius = (i + 1 - std::max(0U, m_HandTrace.size() - TraceLength)) / (std::min<float>(m_HandTrace.size(), TraceLength));
				for (size_t j = 0; j < h.size(); j++)
				{
					//m_HandTraceModel.Primitives.emplace_back(h[j], 0.02f);
					g_PrimitiveDrawer.DrawSphere(h[j], 0.005f * radius, Colors::LimeGreen);
				}
			}
		}
	}
	g_PrimitiveDrawer.End();

	//if (m_HandTrace.size() > 0)
	//{
	//	if (!pBatch)
	//	{
	//		pBatch = std::make_unique<PrimitiveBatch<VertexPositionNormal>>(pContext, 204800,40960);
	//	}
	//	m_HandTraceModel.SetISO(0.33333f);
	//	m_HandTraceModel.Update();
	//	m_HandTraceModel.Tessellate(m_HandTraceVertices, m_HandTraceIndices, 0.005f);
	//	pBatch->Begin();
	//	pEffect->SetDiffuseColor(Colors::LimeGreen);
	//	//pEffect->SetEmissiveColor(Colors::LimeGreen);
	//	pEffect->SetTextureEnabled(false);
	//	pEffect->SetWorld(XMMatrixIdentity());
	//	pEffect->Apply(pContext);
	//	pBatch->DrawIndexed(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, m_HandTraceIndices.data(), m_HandTraceIndices.size(), m_HandTraceVertices.data(), m_HandTraceVertices.size());
	//	pBatch->End();
	//}

}

void Causality::WorldScene::DrawBox(DirectX::SimpleMath::Vector3  conners [], DirectX::CXMVECTOR color)
{
	g_PrimitiveDrawer.DrawLine(conners[0], conners[1], color);
	g_PrimitiveDrawer.DrawLine(conners[1], conners[2], color);
	g_PrimitiveDrawer.DrawLine(conners[2], conners[3], color);
	g_PrimitiveDrawer.DrawLine(conners[3], conners[0], color);

	g_PrimitiveDrawer.DrawLine(conners[3], conners[7], color);
	g_PrimitiveDrawer.DrawLine(conners[2], conners[6], color);
	g_PrimitiveDrawer.DrawLine(conners[1], conners[5], color);
	g_PrimitiveDrawer.DrawLine(conners[0], conners[4], color);

	g_PrimitiveDrawer.DrawLine(conners[4], conners[5], color);
	g_PrimitiveDrawer.DrawLine(conners[5], conners[6], color);
	g_PrimitiveDrawer.DrawLine(conners[6], conners[7], color);
	g_PrimitiveDrawer.DrawLine(conners[7], conners[4], color);

}

void XM_CALLCONV Causality::WorldScene::UpdateViewMatrix(DirectX::FXMMATRIX view)
{
	if (pEffect)
		pEffect->SetView(view);
	if (pBackground)
		pBackground->UpdateViewMatrix(view);
	g_PrimitiveDrawer.SetView(view);
	//for (const auto& item : m_HandModels)
	//{
	//	if (item.second)
	//		item.second->UpdateViewMatrix(view);
	//}
}

void XM_CALLCONV Causality::WorldScene::UpdateProjectionMatrix(DirectX::FXMMATRIX projection)
{
	if (pEffect)
		pEffect->SetProjection(projection);
	if (pBackground)
		pBackground->UpdateProjectionMatrix(projection);
	g_PrimitiveDrawer.SetProjection(projection);
}

void Causality::WorldScene::UpdateAnimation(StepTimer const & timer)
{

	for (auto itr = m_HandModels.begin(); itr != m_HandModels.end(); )
	{
		bool result = itr->second->Update(m_Frame, m_FrameTransform);
		// Remove hands lost track for 60+ frames
		if (!result)
		{
			if (itr->second->LostFramesCount() > 60)
				itr = m_HandModels.erase(itr);
		}
		else
			++itr;
	}

	if (m_HandTrace.size() > 0)
	{
		{ // Critia section
			std::lock_guard<mutex> guard(m_HandFrameMutex);
			const int plotSize = 45;
			BoundingOrientedBox::CreateFromPoints(m_CurrentHandBoundingBox, m_HandTrace.back().size(), m_HandTrace.back().data(), sizeof(Vector3));
			m_TracePoints.clear();
			Color color = Colors::LimeGreen;
			for (int i = m_HandTrace.size() - 1; i >= std::max(0, (int) m_HandTrace.size() - plotSize); i--)
			{
				const auto& h = m_HandTrace[i];
				//float radius = (i + 1 - std::max(0U, m_HandTrace.size() - plotSize)) / (std::min<float>(m_HandTrace.size(), plotSize));
				for (size_t j = 0; j < h.size(); j++)
				{
					//g_PrimitiveDrawer.DrawSphere(h[j], 0.005 * radius, color);
					m_TracePoints.push_back(h[j]);
				}
			}
			//for (const auto& pModel : Children)
			//{
			//	//btCollisionWorld::RayResultCallback
			//	auto pRigid = dynamic_cast<PhysicalGeometryModel*>(pModel.get());
			//	pRigid->GetBulletRigid()->checkCollideWith()
			//}
			//auto pBat = dynamic_cast<PhysicalGeometryModel*>(Children[0].get());
			//pBat->SetPosition(vector_cast<Vector3>(m_Frame.hands().frontmost().palmPosition()));
		}
		CreateBoundingOrientedBoxFromPoints(m_HandTraceBoundingBox, m_TracePoints.size(), m_TracePoints.data(), sizeof(Vector3));
		XMFLOAT3 v = m_HandTraceBoundingBox.Extents;
		m_HandDescriptionFeature = Eigen::Vector2f(v.y / v.x, v.z / v.x);

		// ASSUMPTION: Extends is sorted from!


		//XMMATRIX invTrans = XMMatrixAffineTransformation(g_XMOne / XMVectorReplicate(m_HandTraceBoundingBox.Extents.x), XMVectorZero(), XMQuaternionInverse(XMLoadFloat4(&m_HandTraceBoundingBox.Orientation)), -XMLoadFloat3(&m_HandTraceBoundingBox.Center));
		//int sampleCount = std::min<int>(m_TraceSamples.size(), TraceLength)*m_TraceSamples[0].size();
		//for (const auto& model : Children)
		//{
		//	auto pModel = dynamic_cast<Model*>(model.get());
		//	auto inCount = 0;
		//	if (pModel)
		//	{
		//		auto obox = model->GetOrientedBoundingBox();
		//		XMMATRIX fowTrans = XMMatrixAffineTransformation(XMVectorReplicate(obox.Extents.x), XMVectorZero(), XMQuaternionIdentity(), XMVectorZero());
		//		fowTrans = invTrans * fowTrans;
		//		auto pSample = m_TraceSamples.back().data() + m_TraceSamples.back().size()-1;
		//		for (size_t i = 0; i < sampleCount; i++)
		//		{
		//			const auto& point = pSample[-i];
		//			XMVECTOR p = XMVector3Transform(point, fowTrans);
		//			int j;
		//			for ( j = 0; j < pModel->Parts.size(); j++)
		//			{
		//				if (pModel->Parts[j].BoundOrientedBox.Contains(p))
		//					break;
		//			}
		//			if (j >= pModel->Parts.size())
		//				inCount++;
		//		}
		//	}
		//	m_ModelDetailSimilarity[model->Name] = (float) inCount / (float)sampleCount;
		//}



	}

	//if (pGroundRigid)
	//	pGroundRigid->setLinearVelocity({ 0,-1.0f,0 });

	pDynamicsWorld->stepSimulation(timer.GetElapsedSeconds(), 10);

	//for (auto& obj : Children)
	//{
	//	auto s = (rand() % 1000) / 1000.0;
	//	obj->Rotate(XMQuaternionRotationRollPitchYaw(0, 0.5f * timer.GetElapsedSeconds(), 0));
	//}
}

void Causality::WorldScene::OnHandsTracked(const UserHandsEventArgs & e)
{
	m_HaveHands = true;
	//const auto& hand = e.sender.frame().hands().frontmost();
	//size_t i = 0;
	//
	//XMMATRIX leap2world = e.toWorldTransform;
	//for (const auto& finger : hand.fingers())
	//{
	//	XMVECTOR bJ = XMVector3Transform(finger.bone((Leap::Bone::Type)0).prevJoint().toVector3<Vector3>(), leap2world);
	//	auto pState = m_HandRigids[i]->GetBulletRigid()->getMotionState();
	//	auto transform = btTransform::getIdentity();
	//	transform.setOrigin(vector_cast<btVector3>(bJ));
	//	if (!pState)
	//	{
	//		pState = new btDefaultMotionState(transform);
	//		m_HandRigids[i]->GetBulletRigid()->setMotionState(pState);
	//	}
	//	else
	//		pState->setWorldTransform(transform);
	//	

	//	i++;
	//	for (size_t boneIdx = 0; boneIdx < 4; boneIdx++) // bone idx
	//	{
	//		const auto & bone = finger.bone((Leap::Bone::Type)boneIdx);
	//		XMVECTOR eJ = XMVector3Transform(bone.nextJoint().toVector3<Vector3>(), leap2world);

	//		auto pState = m_HandRigids[i]->GetBulletRigid()->getMotionState();
	//		auto transform = btTransform::getIdentity();
	//		transform.setOrigin(vector_cast<btVector3>(eJ));
	//		if (!pState)
	//		{
	//			pState = new btDefaultMotionState(transform);
	//			m_HandRigids[i]->GetBulletRigid()->setMotionState(pState);
	//		}
	//		else
	//			pState->setWorldTransform(transform);

	//		i++;
	//	}
	//}

	m_Frame = e.sender.frame();
	for (const auto& hand : m_Frame.hands())
	{
		if (!m_HandModels[hand.id()])
			m_HandModels[hand.id()].reset(new HandPhysicalModel(pDynamicsWorld, m_Frame, hand, e.toWorldTransform));
	}

	//for (size_t j = 0; j < i; j++)
	//{
	//	pDynamicsWorld->addRigidBody(m_HandRigids[j]->GetBulletRigid());
	//	m_HandRigids[j]->GetBulletRigid()->setGravity({ 0,0,0 });
	//}
}

void Causality::WorldScene::OnHandsTrackLost(const UserHandsEventArgs & e)
{
	m_Frame = e.sender.frame();
	m_FrameTransform = e.toWorldTransform;
	if (m_Frame.hands().count() == 0)
	{
		m_HaveHands = false;
		std::lock_guard<mutex> guard(m_HandFrameMutex);
		m_HandTrace.clear();
		m_TraceSamples.clear();
		//for (const auto &pRigid : m_HandRigids)
		//{
		//	pDynamicsWorld->removeRigidBody(pRigid->GetBulletRigid());
		//}
	}
}

void Causality::WorldScene::OnHandsMove(const UserHandsEventArgs & e)
{
	std::lock_guard<mutex> guard(m_HandFrameMutex);
	m_Frame = e.sender.frame();
	m_FrameTransform = e.toWorldTransform;
	XMMATRIX leap2world = m_FrameTransform;
	//std::array<DirectX::Vector3, 25> joints;
	std::vector<DirectX::BoundingOrientedBox> handBoxes;
	float fingerStdev = 0.02;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::normal_distribution<float> normalDist(0, fingerStdev);
	std::uniform_real<float> uniformDist;

	int handIdx = 0;
	for (const auto& hand : m_Frame.hands())
	{
		int fingerIdx = 0; // hand idx
		m_HandTrace.emplace_back();
		m_TraceSamples.emplace_back();
		auto& samples = m_TraceSamples.back();
		auto& joints = m_HandTrace.back();
		for (const auto& finger : hand.fingers())
		{
			XMVECTOR bJ = XMVector3Transform(finger.bone((Leap::Bone::Type)0).prevJoint().toVector3<Vector3>(), leap2world);
			joints[fingerIdx * 5] = bJ;
			for (size_t boneIdx = 0; boneIdx < 4; boneIdx++) // bone idx
			{
				const auto & bone = finger.bone((Leap::Bone::Type)boneIdx);
				XMVECTOR eJ = XMVector3Transform(bone.nextJoint().toVector3<Vector3>(), leap2world);
				joints[fingerIdx * 5 + boneIdx + 1] = eJ;


				//auto dir = eJ - bJ;
				//float dis = XMVectorGetX(XMVector3Length(dir));
				//if (abs(dis) < 0.001)
				//	continue;
				//XMVECTOR rot = XMQuaternionRotationVectorToVector(g_XMIdentityR1, dir);
				//bJ = eJ;
				//for (size_t k = 0; k < 100; k++) // bone idx
				//{
				//	float x = normalDist(gen);
				//	float h = uniformDist(gen);
				//	float z = normalDist(gen);
				//	XMVECTOR disp = XMVectorSet(x, h*dis, z, 1);
				//	disp = XMVector3Rotate(disp, rot);
				//	disp += bJ;
				//	samples[(fingerIdx * 4 + boneIdx) * 100 + k] = disp;
				//}
			}
			fingerIdx++;
		}
		handIdx++;
		while (m_HandTrace.size() > 60)
		{
			m_HandTrace.pop_front();
			//m_TraceSamples.pop_front();
		}

		Vector3 rayEnd = XMVector3Transform(hand.palmPosition().toVector3<Vector3>(), leap2world);
		Vector3 rayBegin = m_pCameraLocation->GetPosition();
		auto pConeShape = new btConeShape(100, XM_PI / 16 * 100);
		auto pCollisionCone = new btCollisionObject();
		pCollisionCone->setCollisionShape(pConeShape);
		btTransform trans(
			vector_cast<btQuaternion>(XMQuaternionRotationVectorToVector(g_XMIdentityR1, rayEnd - rayBegin)),
			vector_cast<btVector3>(rayBegin));
		pCollisionCone->setWorldTransform(trans);
		class Callback : public btDynamicsWorld::ContactResultCallback
		{
		public:
			const IModelNode* pModel;
			Callback() {}

			void SetModel(const IModelNode* pModel)
			{
				this->pModel = pModel;
			}
			Callback(const IModelNode* pModel)
				: pModel(pModel)
			{
			}
			virtual	btScalar	addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
			{
				cout << "point frustrum contact with "<< pModel->Name << endl;
				return 0;
			}
		};
		static map<string, Callback> callbackTable;
		//for (const auto& model : Children)
		//{
		//	auto pRigid = dynamic_cast<PhysicalRigid*>(model.get());
		//	callbackTable[model->Name].SetModel(model.get());
		//	pDynamicsWorld->contactPairTest(pRigid->GetBulletRigid(), pCollisionCone, callbackTable[model->Name]);
		//}
	}

	//int i = 0;
	//const auto& hand = m_Frame.hands().frontmost();
	//for (const auto& finger : hand.fingers())
	//{
	//	XMVECTOR bJ = XMVector3Transform(finger.bone((Leap::Bone::Type)0).prevJoint().toVector3<Vector3>(), leap2world);
	//	auto pState = m_HandRigids[i]->GetBulletRigid()->getMotionState();
	//	auto transform = btTransform::getIdentity();
	//	transform.setOrigin(vector_cast<btVector3>(bJ));
	//	m_HandRigids[i]->GetBulletRigid()->proceedToTransform(transform);


	//	i++;
	//	for (size_t boneIdx = 0; boneIdx < 4; boneIdx++) // bone idx
	//	{
	//		const auto & bone = finger.bone((Leap::Bone::Type)boneIdx);
	//		XMVECTOR eJ = XMVector3Transform(bone.nextJoint().toVector3<Vector3>(), leap2world);

	//		auto pState = m_HandRigids[i]->GetBulletRigid()->getMotionState();
	//		auto transform = btTransform::getIdentity();
	//		transform.setOrigin(vector_cast<btVector3>(eJ));
	//		m_HandRigids[i]->GetBulletRigid()->proceedToTransform(transform);

	//		i++;
	//	}
	//}


	std::cout << "[Leap] Hands Move." << std::endl;
}

void Causality::WorldScene::OnKeyDown(const KeyboardEventArgs & e)
{
}

void Causality::WorldScene::OnKeyUp(const KeyboardEventArgs & e)
{
	if (e.Key == 'T')
		m_showTrace = !m_showTrace;
}

void XM_CALLCONV Causality::WorldScene::SetModelMatrix(DirectX::FXMMATRIX model)
{
	return;
}

XMMATRIX Causality::WorldScene::GetModelMatrix() const
{
	//return XMMatrixScalingFromVector(XMVectorReplicate(0.1f));//
	return XMMatrixIdentity();
}

std::pair<DirectX::Vector3, DirectX::Quaternion> XM_CALLCONV CaculateCylinderTransform(FXMVECTOR P1, FXMVECTOR P2)
{
	std::pair<DirectX::Vector3, DirectX::Quaternion> trans;
	auto center = XMVectorAdd(P1, P2);
	center = XMVectorMultiply(center, g_XMOneHalf);
	auto dir = XMVectorSubtract(P1, P2);
	auto scale = XMVector3Length(dir);
	XMVECTOR rot;
	if (XMVector4Equal(dir, g_XMZero))
		rot = XMQuaternionIdentity();
	else
		rot = XMQuaternionRotationVectorToVector(g_XMIdentityR1, dir);
	trans.first = center;
	trans.second = rot;
	return trans;
}

Causality::HandPhysicalModel::HandPhysicalModel(const std::shared_ptr<btDynamicsWorld> &pWorld, const Leap::Frame & frame, const Leap::Hand & hand, const DirectX::Matrix4x4 & transform)
	:m_Hand(hand)
{
	Id = hand.id();
	LocalMatrix = transform;
	XMMATRIX leap2world = LocalMatrix;
	int j = 0;
	for (const auto& finger : m_Hand.fingers())
	{
		for (size_t i = 0; i < 4; i++)
		{
			const auto & bone = finger.bone((Leap::Bone::Type)i);
			XMVECTOR bJ = XMVector3Transform(bone.prevJoint().toVector3<Vector3>(), leap2world);
			XMVECTOR eJ = XMVector3Transform(bone.nextJoint().toVector3<Vector3>(), leap2world);
			auto center = 0.5f * XMVectorAdd(bJ, eJ);
			auto dir = XMVectorSubtract(eJ, bJ);
			auto halfext = std::max(XMVectorGetX(XMVector3Length(dir))*0.5f, fingerLength);
			XMVECTOR rot;
			if (XMVector4Equal(dir, g_XMZero))
				rot = XMQuaternionIdentity();
			else
				rot = XMQuaternionRotationVectorToVector(g_XMIdentityR1, dir);
			shared_ptr<btCylinderShape> pShape(new btCylinderShape(btVector3(fingerRadius, halfext, fingerRadius)));
			m_HandRigids.emplace_back(new PhysicalRigid());
			const auto & pRigid = m_HandRigids.back();
			//pRigid->GetBulletRigid()->setGravity({ 0,0,0 });
			pRigid->InitializePhysics(nullptr, pShape, 10, center, rot);
			pRigid->GetBulletRigid()->setGravity({ 0,0,0 });
			pRigid->Enable(pWorld);
			pRigid->GetBulletRigid()->setGravity({ 0,0,0 });
			pRigid->GetBulletRigid()->setFriction(5.0f);
		}
	}
}

bool Causality::HandPhysicalModel::Update(const Leap::Frame & frame, const DirectX::Matrix4x4 & transform)
{
	LocalMatrix = transform;
	XMMATRIX leap2world = LocalMatrix;
	m_Hand = frame.hand(Id);

	if (m_Hand.isValid())
	{
		LostFrames = 0;
		int j = 0;
		for (const auto& finger : m_Hand.fingers())
		{
			for (size_t i = 0; i < 4; i++)
			{
				auto & pRigid = m_HandRigids[j++];
				if (!pRigid->IsEnabled())
					pRigid->Enable();
				const auto & bone = finger.bone((Leap::Bone::Type)i);
				XMVECTOR bJ = XMVector3Transform(bone.prevJoint().toVector3<Vector3>(), leap2world);
				XMVECTOR eJ = XMVector3Transform(bone.nextJoint().toVector3<Vector3>(), leap2world);
				auto center = 0.5f * XMVectorAdd(bJ, eJ);
				auto dir = XMVectorSubtract(eJ, bJ);
				XMVECTOR rot;
				if (XMVector4Equal(dir, g_XMZero))
					rot = XMQuaternionIdentity();
				else
					rot = XMQuaternionRotationVectorToVector(g_XMIdentityR1, dir);
				auto trans = btTransform(vector_cast<btQuaternion>(rot), vector_cast<btVector3>(center));
				pRigid->GetBulletRigid()->proceedToTransform(trans);
			}
		}
		return true;
	}
	else
	{
		for (auto& pRigid : m_HandRigids)
		{
			pRigid->Disable();
		}
		LostFrames++;
		return false;
	}
}

// Inherited via IModelNode

void Causality::HandPhysicalModel::Render(ID3D11DeviceContext * pContext, DirectX::IEffect * pEffect)
{
	XMMATRIX leap2world = LocalMatrix;
	auto palmPosition = XMVector3Transform(m_Hand.palmPosition().toVector3<Vector3>(), leap2world);
	//g_PrimitiveDrawer.DrawSphere(palmPosition, 0.02f, Colors::YellowGreen);
	for (const auto& pRigid : m_HandRigids)
	{
		g_PrimitiveDrawer.DrawCylinder(
			pRigid->GetPosition(),
			XMVector3Rotate(g_XMIdentityR1, pRigid->GetOrientation()),
			dynamic_cast<btCylinderShape*>(pRigid->GetBulletShape())->getHalfExtentsWithoutMargin().y(),
			fingerRadius,
			Colors::LimeGreen);
	}
	//for (const auto& finger : m_Hand.fingers())
	//{
	//	for (size_t i = 0; i < 4; i++)
	//	{
	//		const auto & bone = finger.bone((Leap::Bone::Type)i);
	//		XMVECTOR bJ = XMVector3Transform(bone.prevJoint().toVector3<Vector3>(), leap2world);
	//		XMVECTOR eJ = XMVector3Transform(bone.nextJoint().toVector3<Vector3>(), leap2world);
	//		//g_PrimitiveDrawer.DrawLine(bJ, eJ, Colors::LimeGreen);
	//		//g_PrimitiveDrawer.DrawCube(bJ, g_XMOne * 0.03, g_XMIdentityR3, Colors::Red);
	//		g_PrimitiveDrawer.DrawCylinder(bJ, eJ,0.015f,Colors::LimeGreen);

	//		//auto center = 0.5f * XMVectorAdd(bJ, eJ);
	//		//auto dir = XMVectorSubtract(eJ, bJ);
	//		//auto scale = XMVector3Length(dir);
	//		//XMVECTOR rot;
	//		//if (XMVector4LessOrEqual(XMVector3LengthSq(dir), XMVectorReplicate(0.01f)))
	//		//	rot = XMQuaternionIdentity();
	//		//else
	//		//	rot = XMQuaternionRotationVectorToVector(g_XMIdentityR1, dir);
	//		//XMMATRIX world = XMMatrixAffineTransformation(scale, g_XMZero, rot, center);
	//		//s_pCylinder->Draw(world, ViewMatrix, ProjectionMatrix,Colors::LimeGreen);
	//	}
	//}
}

inline void Causality::CubeModel::Render(ID3D11DeviceContext * pContext, DirectX::IEffect * pEffect)
{
	XMVECTOR extent = XMLoadFloat3(&BoundBox.Extents);
	g_PrimitiveDrawer.DrawCube(this->GetPosition(),
		XMVectorMultiply(extent, (XMVECTOR)this->GetScale()),
		this->GetOrientation(),
		m_Color);
}