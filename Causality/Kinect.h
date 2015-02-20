#ifndef	KINECT_2_USER
#define KINECT_2_USER

#ifdef _MSC_VER
#pragma once
#endif  // _MSC_VER
#include "Common\BasicClass.h"
#include <wrl\client.h>
#include <memory>
#include "Common\DirectXMathExtend.h"
#include "Armature.h"



namespace Platform
{
	namespace Devices
	{
#ifndef _HandType_
#define _HandType_
		typedef enum _HandType HandType;


		enum _HandType
		{
			HandType_NONE = 0,
			HandType_LEFT = (HandType_NONE + 1),
			HandType_RIGHT = (HandType_LEFT + 1)
		};
#endif // _HandType_
#ifndef _HandState_
#define _HandState_
		typedef enum _HandState HandState;


		enum _HandState
		{
			HandState_Unknown = 0,
			HandState_NotTracked = 1,
			HandState_Open = 2,
			HandState_Closed = 3,
			HandState_Lasso = 4
		};
#endif // _HandState_

#ifndef _JointType_
#define _JointType_
		typedef enum _JointType JointType;


		enum _JointType
		{
			JointType_SpineBase = 0,
			JointType_SpineMid = 1,
			JointType_Neck = 2,
			JointType_Head = 3,
			JointType_ShoulderLeft = 4,
			JointType_ElbowLeft = 5,
			JointType_WristLeft = 6,
			JointType_HandLeft = 7,
			JointType_ShoulderRight = 8,
			JointType_ElbowRight = 9,
			JointType_WristRight = 10,
			JointType_HandRight = 11,
			JointType_HipLeft = 12,
			JointType_KneeLeft = 13,
			JointType_AnkleLeft = 14,
			JointType_FootLeft = 15,
			JointType_HipRight = 16,
			JointType_KneeRight = 17,
			JointType_AnkleRight = 18,
			JointType_FootRight = 19,
			JointType_SpineShoulder = 20,
			JointType_HandTipLeft = 21,
			JointType_ThumbLeft = 22,
			JointType_HandTipRight = 23,
			JointType_ThumbRight = 24,
			JointType_Count = (JointType_ThumbRight + 1)
		};
#endif // _JointType_

		struct TrackedPlayer
		{
		public:
			TrackedPlayer();
			// Skeleton Structure and basic body parameter
			// Shared through all players since they have same structure
			static std::unique_ptr<Armatures::ArmatureBase> PlayerArmature;

			// Default pose data, should we use this ?
			// Armatures::BoneAnimationFrame RestFrame;

			// Current pose data
			Armatures::BoneAnimationFrame CurrentFrame;

			// Hand States
			HandState HandStates[2];

			uint64_t PlayerID;

			bool IsCurrentTracked;

			Platform::Fundation::Event<const TrackedPlayer&, HandType, HandState> OnHandStateChanged;
			Platform::Fundation::Event<const TrackedPlayer&> OnPoseChanged;
		};

		// An aggregate of Kinect Resources
		class Kinect
		{
		public:
			~Kinect();

			HRESULT Initalize();

			bool IsConnected() const;

			// Player Event event interface!
			Platform::Fundation::Event<const TrackedPlayer&> OnPlayerTracked;
			Platform::Fundation::Event<const TrackedPlayer&> OnPlayerLost;
			//Platform::Fundation::Event<const TrackedPlayer&> OnPlayerPoseChanged;
			//Platform::Fundation::Event<const TrackedPlayer&, HandEnum, HandState> OnPlayerHandStateChanged;

			// Process frame and trigger the events
			void ProcessFrame();

			const std::map<uint64_t, TrackedPlayer*> &GetLatestPlayerFrame();

			// Static Constructors!!!
			static std::unique_ptr<Kinect> CreateDefault();
		protected:

			Kinect();

		public:
			static JointType JointsParent[JointType_Count];

		private:

			class Impl;
			std::unique_ptr<Impl> pImpl;

		};
		
	}
}


#endif


