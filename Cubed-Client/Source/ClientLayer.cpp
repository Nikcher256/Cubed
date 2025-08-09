#include "Walnut/Input/Input.h"

#include "ClientLayer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "Walnut/ImGui/ImGuiTheme.h"
#include "Walnut/Serialization/BufferStream.h"

#include "glm/gtc/type_ptr.hpp"

#include "ServerPacket.h"

namespace Cubed {

	Walnut::Buffer s_ScratchBuffer;

	static void DrawRect(glm::vec2 position, glm::vec2 size, uint32_t color) {
		ImDrawList* drawList = ImGui::GetBackgroundDrawList();
		ImVec2 min = ImGui::GetWindowPos() + ImVec2(position.x, position.y);
		ImVec2 max = min + ImVec2(size.x, size.y);

		drawList->AddRectFilled(min, max, color);
	}

	void ClientLayer::OnAttach()
	{
		s_ScratchBuffer.Allocate(10 * 1024 * 1024);//10MB

		m_Client.SetDataReceivedCallback([this](const Walnut::Buffer buffer) { OnDataReceived(buffer); });

		m_Renderer.Init();
	}

	void ClientLayer::OnDetach()
	{
		m_Client.Disconnect();
		m_Renderer.Shutdown();
	}

	void ClientLayer::OnUpdate(float ts)
	{
		glm::vec2 dir{ 0.0f, 0.0f };
		if (Walnut::Input::IsKeyDown(Walnut::KeyCode::W))
			dir.y = -1;
		else if (Walnut::Input::IsKeyDown(Walnut::KeyCode::S))
			dir.y = 1;

		if (Walnut::Input::IsKeyDown(Walnut::KeyCode::A))
			dir.x = -1;
		else if (Walnut::Input::IsKeyDown(Walnut::KeyCode::D))
			dir.x = 1;
		

		if (glm::length(dir) > 0.0f) 
		{
			const float speed = 5.0f;
			// Optional (for game play reasons?)
			dir = glm::normalize(dir);
			m_PlayerVelocity = dir * speed;
		}

		m_PlayerPosition += m_PlayerVelocity * ts;
		m_PlayerVelocity = glm::mix(m_PlayerVelocity, glm::vec2(0.0f), 10.0f * ts);
		m_PlayerRotation.y += 20.0f * ts;

		if (m_Client.GetConnectionStatus() == Walnut::Client::ConnectionStatus::Connected)
		{
			Walnut::BufferStreamWriter stream(s_ScratchBuffer);
			stream.WriteRaw(PacketType::ClientUpdate);
			stream.WriteRaw<glm::vec2>(m_PlayerPosition);
			stream.WriteRaw<glm::vec2>(m_PlayerVelocity);
			m_Client.SendBuffer(stream.GetBuffer());
		}
	}

	void ClientLayer::OnUIRender()
	{
		Walnut::Client::ConnectionStatus connectionStatus = m_Client.GetConnectionStatus();
		if (connectionStatus != Walnut::Client::ConnectionStatus::Connected)
		{

			ImGui::Begin("Connect to Server");

			ImGui::InputText("Server Address: ", &m_ServerAddress);
			if (connectionStatus == Walnut::Client::ConnectionStatus::FailedToConnect)
				ImGui::TextColored(ImColor(Walnut::UI::Colors::Theme::error), "Failed to connect.");
			else if(connectionStatus == Walnut::Client::ConnectionStatus::Connecting)
				ImGui::TextColored(ImColor(Walnut::UI::Colors::Theme::textDarker), "Connecting...");
			if (ImGui::Button("Connect"))
			{
				m_Client.ConnectToServer(m_ServerAddress);
			}

			ImGui::End();
		}
		m_Renderer.RenderUI();
		ImGui::Begin("Controls");

		ImGui::DragFloat3("Player Position", glm::value_ptr(m_PlayerPosition), 0.05f);
		ImGui::DragFloat3("Player Rotation", glm::value_ptr(m_PlayerRotation), 0.05f);

		ImGui::DragFloat3("Camera Position", glm::value_ptr(m_Camera.Position), 0.05f);
		ImGui::DragFloat3("Camera Rotation", glm::value_ptr(m_Camera.Rotation), 0.05f);

		ImGui::End();

	}

	void ClientLayer::OnDataReceived(const Walnut::Buffer buffer)
	{
		Walnut::BufferStreamReader stream(buffer);

		PacketType type;
		stream.ReadRaw(type);
		switch (type)
		{
		case PacketType::ClientUpdate:
			m_PlayerDataMutex.lock();
			{
				stream.ReadMap(m_PlayerData);
			}
			m_PlayerDataMutex.unlock();
			break;
		case PacketType::ClientConnect:

			uint32_t idFromServer;
			stream.ReadRaw<uint32_t>(idFromServer);
			WL_INFO("We have connected! Server says our ID is {}", idFromServer);
			WL_INFO("We say our ID is {}", m_Client.GetID());
			m_PlayerID = idFromServer;
			break;
		case PacketType::ClientDisconnect:
			uint32_t DisconnectedPlayerID;
			stream.ReadRaw<uint32_t>(DisconnectedPlayerID);
			m_PlayerDataMutex.lock();
			{
				m_PlayerData.erase(DisconnectedPlayerID);
			}
			m_PlayerDataMutex.unlock();
			break;
		}
		
	}

	void ClientLayer::OnRender()
	{

		m_Renderer.BeginScene(m_Camera);

		Walnut::Client::ConnectionStatus connectionStatus = m_Client.GetConnectionStatus();
		//if (connectionStatus == Walnut::Client::ConnectionStatus::Connected)
		{
			uint32_t color = (m_PlayerID << 8) | 0xFF;
			m_Renderer.RenderCube(glm::vec3(m_PlayerPosition.x, 0, m_PlayerPosition.y), m_PlayerRotation);
			m_PlayerDataMutex.lock();
			std::map<uint32_t, PlayerData> playerData = m_PlayerData;
			m_PlayerDataMutex.unlock();

			for (const auto& [id, data] : playerData)
			{
				if (id == m_PlayerID)
					continue;
				color = (id << 8) | 0xFF;
				m_Renderer.RenderCube(glm::vec3(data.Position.x, 0, data.Position.y), m_PlayerRotation);
			}
		}

		m_Renderer.EndScene();

	}
}
