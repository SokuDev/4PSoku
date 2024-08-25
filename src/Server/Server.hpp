//
// Created by PinkySmile on 14/08/24.
//

#ifndef INC_4PSOKU_SERVER_HPP
#define INC_4PSOKU_SERVER_HPP


#include <random>
#include <array>
#include <deque>
#include <SFML/Network.hpp>
#include <CustomPackets.hpp>
#include "Packet.hpp"

extern const uint8_t versionString2v2[16];

class Server {
private:
	enum ConnectionStatus {
		STATUS_CONNECTED,
		STATUS_DISCONNECTED,
		STATUS_POLITE,
		STATUS_PLAYING,
		STATUS_SPECTATING
	};

	struct PlayerState;

	struct Client {
		sf::IpAddress ip;
		unsigned short port;
		char slotId = -1;
		sf::Clock lastPacket;
		sf::Clock lastChain;
		sf::Clock lastChainReceived;
		unsigned gameId = 0;
		unsigned linkCount = UINT32_MAX;
		char name[32];
		PlayerState *state = nullptr;
		ConnectionStatus status = STATUS_CONNECTED;
		unsigned internalTimer = 0;

		Client(sf::IpAddress ip, unsigned short port): ip(ip), port(port) {}
	};

	enum GameStateStep {
		JOINING,
		SELECT_CHARACTER,
		READY_TO_LOAD,
		LOADING,
		READY_TO_FIGHT,
		FIGHT,
		END_OF_FIGHT
	};

	struct PlayerState {
		Client *client;
		unsigned lastFrameId = 0;
		unsigned frameIdOffset = 0;
		std::vector<SokuLib::Inputs> inputs;
		SokuLib::SceneId inputScene;
		SokuLib::CharacterPacked chr;
		unsigned char palette;
		unsigned char deckId;
		bool hasSimulButtons;
		std::vector<unsigned short> deck;
		GameStateStep state = JOINING;
	};

	struct GameState {
		std::vector<PlayerState> players;
		std::array<PlayerState *, 4> slots = {nullptr, nullptr, nullptr, nullptr};
		unsigned randomSeed;
		unsigned char stage;
		unsigned char music;
		unsigned char matchId = 0;
	};

	std::random_device _random;
	sf::UdpSocket _sock;
	std::map<std::pair<sf::IpAddress, unsigned short>, Client> _clients;
	GameState _state;

	void _disconnect(Client &client);

	void _handlePacket(Client &client, CustomPacket &packet, size_t packetSize);
	void _handlePacket(Client &client, SokuLib::PacketHello &packet, size_t packetSize);
	void _handlePacket(Client &client, SokuLib::PacketChain &packet, size_t packetSize);
	void _handlePacket(Client &client, SokuLib::PacketInitRequ &packet, size_t packetSize);
	void _handlePacket(Client &client, PacketPlayerJoinAck &packet, size_t packetSize);
	void _handlePacket(Client &client, SokuLib::PacketGame &packet, size_t packetSize);
	void _handlePacket(Client &client, PacketLoadingReady &packet, size_t packetSize);
	void _handlePacketQuit(Client &client);
	void _handlePacketGame(Client &client, SokuLib::GameLoadedEvent &packet, size_t packetSize);
	void _handlePacketGame(Client &client, SokuLib::GameInputEvent &packet, size_t packetSize);
	void _handlePacketGame(Client &client, SokuLib::GameReplayRequestEvent &packet, size_t packetSize);
	void _handlePacketGameMatchAck(Client &client);
	void _handlePacketGameMatchRequest(Client &client);
	void _handlePacketGameLoadAck(Client &client, SokuLib::GameLoadedEvent &packet, size_t packetSize);

	void _handleChrSelectInput(Client &client, SokuLib::GameInputEvent &packet);
	void _handleBattleInput(Client &client, SokuLib::GameInputEvent &packet);

	void _onPlayerDisconnect(size_t index);

	void _send(Client &client, void *data, size_t size);

	bool allSlotsFilled() const;
	bool readyToLoad() const;
	bool readyToFight() const;
	bool inChrSelect() const;
	bool inBattle() const;
	bool endOfFight() const;

	template<typename T, typename ...Args>
	void _send(Client &client, Args... args)
	{
		T packet{args...};

		this->_send(client, &packet, sizeof(packet));
	}

public:
	Server(unsigned short port);
	void update();
};


#endif //INC_4PSOKU_SERVER_HPP
