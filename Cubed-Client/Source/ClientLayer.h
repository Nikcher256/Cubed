#pragma once

#include "Walnut/Application.h"	
#include "Walnut/Layer.h"

#include "Walnut/Networking/Client.h"

#include <glm\glm.hpp>
#include <map>
#include <mutex>

#include "Renderer/Renderer.h"

namespace Cubed {
	class ClientLayer : public Walnut::Layer
	{
	public:

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void OnUpdate(float ts) override;
		virtual void OnRender() override;
		virtual void OnUIRender() override;
		virtual void OnSwapchainRecreated() override;
	private:
		void OnDataReceived(const Walnut::Buffer buffer);
	private:
		Renderer m_Renderer;

		glm::vec3 m_PlayerPosition{ 0, 0, 0};
		glm::vec3 m_PlayerRotation{ 30.0f, 45.0f, 0 };
		glm::mat4 m_PlayerTransform{ 1.0f };
		glm::vec3 m_PlayerVelocity{ 0, 0, 0};

		Camera m_Camera;

		std::string m_ServerAddress;

		Walnut::Client m_Client;

		std::unordered_map<uint32_t, std::shared_ptr<Cubed::Model>> m_PlayerModels;

		uint32_t m_PlayerID = 0;

		struct PlayerData
		{
			glm::vec2 Position;
			glm::vec2 Velocity;
		};

		std::mutex m_PlayerDataMutex;
		std::map<uint32_t, PlayerData> m_PlayerData;
	};
}
