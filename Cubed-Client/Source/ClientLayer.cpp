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
		// --- Input ---
		// Horizontal plane (XZ) from WASD
		glm::vec2 dirXZ{ 0.0f, 0.0f };
		if (Walnut::Input::IsKeyDown(Walnut::KeyCode::W)) dirXZ.y = -1.0f;
		else if (Walnut::Input::IsKeyDown(Walnut::KeyCode::S)) dirXZ.y = 1.0f;

		if (Walnut::Input::IsKeyDown(Walnut::KeyCode::A)) dirXZ.x = -1.0f;
		else if (Walnut::Input::IsKeyDown(Walnut::KeyCode::D)) dirXZ.x = 1.0f;

		// Vertical (Y) from Q/E — E up, Q down
		float dirY = 0.0f;
		if (Walnut::Input::IsKeyDown(Walnut::KeyCode::E)) dirY += 1.0f;
		if (Walnut::Input::IsKeyDown(Walnut::KeyCode::Q)) dirY -= 1.0f;

		// --- Build desired velocity from input ---
		const float speed = 5.0f;

		glm::vec3 desiredVel{ 0.0f };
		if (glm::length(dirXZ) > 0.0f)
		{
			dirXZ = glm::normalize(dirXZ);
			desiredVel += glm::vec3(dirXZ.x, 0.0f, dirXZ.y) * speed; // XZ
		}
		if (dirY != 0.0f)
			desiredVel.y += dirY * speed; // Y

		// If you want immediate response to keys, assign directly:
		m_PlayerVelocity = desiredVel;

		// --- Integrate ---
		m_PlayerPosition += m_PlayerVelocity * ts;

		// --- Damping (only when no input) ---
		if (desiredVel == glm::vec3(0.0f))
		{
			// decay towards zero; clamp mix factor [0..1]
			float k = glm::clamp(10.0f * ts, 0.0f, 1.0f);
			m_PlayerVelocity = glm::mix(m_PlayerVelocity, glm::vec3(0.0f), k);
		}

		m_PlayerRotation.y += 20.0f * ts;

		// --- Networking: keep protocol 2D (XZ) for now ---
		if (m_Client.GetConnectionStatus() == Walnut::Client::ConnectionStatus::Connected)
		{
			Walnut::BufferStreamWriter stream(s_ScratchBuffer);
			stream.WriteRaw(PacketType::ClientUpdate);

			// Send XZ only (match server/PlayerData)
			glm::vec2 pos2{ m_PlayerPosition.x, m_PlayerPosition.z };
			glm::vec2 vel2{ m_PlayerVelocity.x, m_PlayerVelocity.z };
			stream.WriteRaw<glm::vec2>(pos2);
			stream.WriteRaw<glm::vec2>(vel2);

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

		// Example anchor cube
		//m_Renderer.RenderCube(glm::vec3(0, 0, -5), m_PlayerRotation, 1);

		// Local player: full 3D
		//m_Renderer.RenderCube(m_PlayerPosition, m_PlayerRotation, 0);

		m_Renderer.RenderModels();

		// Remote players: map (x, y) -> (x, 0, z)
		m_PlayerDataMutex.lock();
		std::map<uint32_t, PlayerData> playerData = m_PlayerData;
		m_PlayerDataMutex.unlock();

		for (const auto& [id, data] : playerData)
		{
			if (id == m_PlayerID) continue;
			glm::vec3 pos3{ data.Position.x, 0.0f, data.Position.y };
			m_Renderer.RenderCube(pos3, m_PlayerRotation, 0);
		}

		m_Renderer.EndScene();
	}

	void ClientLayer::OnSwapchainRecreated() {
		m_Renderer.OnSwapchainRecreated();
	}
}


