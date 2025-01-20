//
// Created by PinkySmile on 14/08/24.
//

#include <iostream>
#include <cassert>
#include <cstring>
#include "Server.hpp"

#define BUFFER_SIZE 1024
#define CHARACTER_INPUT_DELAY 30
#define BATTLE_INPUT_DELAY 4

const uint8_t versionString2v2[] = {
	0x41, 0xD0, 0x3B, 0x30, 0x64, 0x41, 0x74, 0xC8,
	0xC6, 0x24, 0x8C, 0xA4, 0x15, 0x44, 0x32, 0x96
};

const char emptyMagicString[32] = "\0\x1F\x54\xF2\xA2\x67\x90\x78\xC2";

Server::Server(unsigned short port)
{
	this->_state.players.reserve(4);
	this->_sock.setBlocking(false);
	if (this->_sock.bind(port, sf::IpAddress::Any) != sf::Socket::Done)
		throw std::runtime_error("Bind to port" + std::to_string(port) + " failed.");
}

void Server::update()
{
	unsigned char buffer[BUFFER_SIZE];
	auto packet = reinterpret_cast<CustomPacket *>(buffer);
	size_t recvSize;
	sf::IpAddress ip;
	unsigned short port;

	while (true) {
		for (auto &c : this->_clients) {
			if (c.second.lastPacket.getElapsedTime().asSeconds() >= 10 || c.second.lastChainReceived.getElapsedTime().asSeconds() >= 5)
				this->_disconnect(c.second);
			else if (c.second.lastChain.getElapsedTime().asSeconds() >= 1 && c.second.status > STATUS_POLITE) {
				this->_send<SokuLib::PacketChain, SokuLib::PacketType, uint32_t>(c.second, SokuLib::CHAIN, this->_state.players.size());
				c.second.lastChain.restart();
			}
		}

		while (true) {
			auto it = std::find_if(this->_clients.begin(), this->_clients.end(), [](auto &p){
				return p.second.status == STATUS_DISCONNECTED;
			});

			if (it == this->_clients.end())
				break;
			this->_clients.erase(it);
		}

		auto status = this->_sock.receive(buffer, sizeof(buffer), recvSize, ip, port);

		switch (status) {
		case sf::Socket::Partial:
			continue;
		case sf::Socket::NotReady:
			return;
		case sf::Socket::Error:
			throw std::runtime_error("Unknown socket error");
		default:
			break;
		}

		auto it = this->_clients.find({ip, port});

		if (it == this->_clients.end()) {
			std::cout << "New connection from " << ip.toString() << ":" << port << std::endl;
			this->_clients.emplace(std::pair{ip, port}, Client{ip, port});
			it = this->_clients.find({ip, port});
		}
		this->_handlePacket(it->second, *packet, recvSize);
	}
}

void Server::_handlePacket(Client &client, CustomPacket &packet, size_t packetSize)
{
	client.lastPacket.restart();
	if (packetSize < sizeof(SokuLib::PacketType))
		return;
	if (
		(packet.type != CLIENT_GAME || packet.base.game.event.type != SokuLib::GAME_INPUT) &&
		packet.type != CHAIN
	) {
		std::cout << "[" << client.ip.toString() << ":" << client.port << "<]: ";
		displayPacketContent(std::cout, packet);
		std::cout << std::endl;
	}
	switch (packet.type) {
	case HELLO:
		return this->_handlePacket(client, packet.base.hello, packetSize);
	case CHAIN:
		return this->_handlePacket(client, packet.base.chain, packetSize);
	case INIT_REQUEST:
		return this->_handlePacket(client, packet.base.initRequest, packetSize);
	case LOADING_READY:
		return this->_handlePacket(client, packet.loadingReady, packetSize);
	case QUIT:
		return this->_handlePacketQuit(client);
	case CLIENT_GAME:
		return this->_handlePacket(client, packet.base.game, packetSize);
	case PLAYER_JOIN_ACK:
		return this->_handlePacket(client, packet.playerJoinAck, packetSize);
	default:
		return;
	}
}

void Server::_handlePacket(Client &client, SokuLib::PacketHello &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	if (client.status == STATUS_CONNECTED)
		client.status = STATUS_POLITE;
	this->_send<SokuLib::PacketType>(client, SokuLib::OLLEH);
}

void Server::_handlePacket(Client &client, SokuLib::PacketChain &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	client.linkCount = packet.spectatorCount;
	client.lastChainReceived.restart();
}

void Server::_handlePacket(Client &client, SokuLib::PacketInitRequ &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	if (client.status < STATUS_POLITE)
		return;
	if (memcmp(packet.gameId, versionString2v2, 16) != 0) {
		std::cout << "Invalid game string!" << std::endl;
		return;
	}
	if (
		(packet.reqType == SokuLib::PLAY_REQU     && this->_state.players.size() == 4) ||
		(packet.reqType == SokuLib::SPECTATE_REQU && this->_state.players.size() != 4)
	)
		return this->_send<SokuLib::PacketInitError>(client, SokuLib::INIT_ERROR, SokuLib::ERROR_GAME_STATE_INVALID);
	if (packet.reqType == SokuLib::SPECTATE_REQU) {
		client.status = STATUS_SPECTATING;
		client.lastChainReceived.restart();
		return;
	}

	if (client.status != STATUS_PLAYING) {
		PacketPlayerJoin playerJoin{PLAYER_JOIN, {0}, -1};

		memcpy(client.name, packet.name, sizeof(client.name));
		memcpy(playerJoin.name, packet.name, sizeof(playerJoin.name));
		for (auto &p: this->_state.players) {
			this->_send(*p.client, &playerJoin, sizeof(playerJoin));
			this->_send(*p.client, &playerJoin, sizeof(playerJoin));
			this->_send(*p.client, &playerJoin, sizeof(playerJoin));
			this->_send(*p.client, &playerJoin, sizeof(playerJoin));
			this->_send(*p.client, &playerJoin, sizeof(playerJoin));
		}

		this->_state.players.push_back({&client});
		client.state = &this->_state.players.back();
	}

	PacketInitSucc response{
		SokuLib::INIT_SUCCESS,
		{0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00},
		132,
		{0, 0}
	};
	char *nameAddrs[4] = {
		response.p1Name,
		response.p2Name,
		response.p3Name,
		response.p4Name
	};

	for (int i = 0; i < 4; i++)
		if (this->_state.slots[i] != nullptr)
			memcpy(nameAddrs[i], this->_state.slots[i]->client->name, 32);
		else
			memcpy(nameAddrs[i], emptyMagicString, 32);

	response.swrDisabled = 0;
	this->_send(client, &response, sizeof(response));
	client.status = STATUS_PLAYING;
	client.lastChainReceived.restart();
}

void Server::_handlePacket(Server::Client &client, PacketPlayerJoinAck &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	if (client.status != STATUS_PLAYING)
		return;
	if (this->allSlotsFilled())
		return;

	PacketPlayerJoinAck response{PLAYER_JOIN_ACK, client.slotId};

	if (packet.slot == -2) {
		response.slot = client.slotId;
		this->_send(client, &response, sizeof(response));
		return;
	}
	if (packet.slot == -1) {
		response.slot = -1;
		this->_send(client, &response, sizeof(response));
		if (client.slotId != -1) {
			PacketPlayerJoin p{PLAYER_JOIN, {0}, client.slotId};

			memcpy(p.name, emptyMagicString, sizeof(p.name));
			this->_state.slots[client.slotId] = nullptr;
			for (auto &player : this->_state.players) {
				this->_send(*player.client, &p, sizeof(p));
				this->_send(*player.client, &p, sizeof(p));
				this->_send(*player.client, &p, sizeof(p));
				this->_send(*player.client, &p, sizeof(p));
				this->_send(*player.client, &p, sizeof(p));
			}
		}
		client.slotId = -1;
		return;
	}
	if (client.slotId != -1) {
		this->_send(client, &response, sizeof(response));
		if (this->allSlotsFilled()) {
			this->_send<CustomOpcodes>(client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(client, UNLOCK_CHAR_SELECT);
		}
		return;
	}
	if (packet.slot > 3 || packet.slot < 0)
		return;
	if (this->_state.slots[packet.slot] != nullptr) {
		PacketPlayerJoin p{PLAYER_JOIN, {0}, packet.slot};

		memcpy(p.name, this->_state.slots[packet.slot]->client->name, sizeof(p.name));
		return this->_send(client, &p, sizeof(p));
	}

	PacketPlayerJoin p{PLAYER_JOIN, {0}, packet.slot};

	client.slotId = packet.slot;
	response.slot = packet.slot;
	memcpy(p.name, client.name, sizeof(p.name));
	this->_send(client, &response, sizeof(response));
	for (auto &player : this->_state.players) {
		this->_send(*player.client, &p, sizeof(p));
		this->_send(*player.client, &p, sizeof(p));
		this->_send(*player.client, &p, sizeof(p));
		this->_send(*player.client, &p, sizeof(p));
		this->_send(*player.client, &p, sizeof(p));
	}
	this->_state.slots[packet.slot] = client.state;
	if (this->allSlotsFilled()) {
		for (auto &player : this->_state.players) {
			this->_send<CustomOpcodes>(*player.client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(*player.client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(*player.client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(*player.client, UNLOCK_CHAR_SELECT);
			this->_send<CustomOpcodes>(*player.client, UNLOCK_CHAR_SELECT);
		}
	}
}

void Server::_handlePacket(Client &client, SokuLib::PacketGame &packet, size_t packetSize)
{
	if (client.status != STATUS_PLAYING)
		return;
	packetSize -= sizeof(SokuLib::PacketType);
	if (packetSize < sizeof(SokuLib::GameType))
		return;
	switch (packet.event.type) {
	case SokuLib::GAME_LOADED:
		return this->_handlePacketGame(client, packet.event.loaded, packetSize);
	case SokuLib::GAME_LOADED_ACK:
		return this->_handlePacketGameLoadAck(client, packet.event.loaded, packetSize);
	case SokuLib::GAME_INPUT:
		return this->_handlePacketGame(client, packet.event.input, packetSize);
	case SokuLib::GAME_MATCH_ACK:
		return this->_handlePacketGameMatchAck(client);
	case SokuLib::GAME_MATCH_REQUEST:
		return this->_handlePacketGameMatchRequest(client);
	case SokuLib::GAME_REPLAY_REQUEST:
		return this->_handlePacketGame(client, packet.event.replayRequest, packetSize);
	default:
		return;
	}
}

void Server::_handlePacket(Server::Client &client, PacketLoadingReady &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	if (packetSize < sizeof(packet) + packet.deckSize * sizeof(unsigned short))
		return;
	if (client.status != STATUS_PLAYING)
		return;
	if (client.state->state != READY_TO_LOAD) {
		client.state->chr = packet.chr;
		client.state->deckId = packet.deckId;
		client.state->palette = packet.skinId;
		client.state->deck.resize(packet.deckSize);
		client.state->hasSimulButtons = packet.hasSimulButtons;
		if (std::all_of(this->_state.players.begin(), this->_state.players.end(), [](const PlayerState &p){ return p.state < READY_TO_LOAD; })) {
			this->_state.randomSeed = this->_random();
			this->_state.music = packet.music;
			this->_state.stage = packet.stage;
			this->_state.matchId++;
		}
		memcpy(client.state->deck.data(), packet.deck, packet.deckSize * sizeof(*client.state->deck.data()));
		client.state->state = READY_TO_LOAD;
	}

	if (this->readyToLoad()) {
		char buffer[sizeof(SokuLib::PacketType) + sizeof(SokuLib::GameType)];
		auto game = (SokuLib::PacketGame *) buffer;

		game->type = SokuLib::HOST_GAME;
		game->event.type = SokuLib::GAME_MATCH_REQUEST;
		this->_send(client, game, sizeof(SokuLib::PacketType) + sizeof(SokuLib::GameType));
	}
}

void Server::_handlePacketQuit(Server::Client &client)
{
	this->_send<SokuLib::PacketType>(client, SokuLib::QUIT);
	this->_disconnect(client);
}

void Server::_handlePacketGame(Client &client, SokuLib::GameLoadedEvent &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	if (packet.sceneId == SokuLib::SCENEID_BATTLE) {
		if (client.state->state == LOADING) {
			SokuLib::PacketGame game{SokuLib::HOST_GAME};

			game.event.loaded = packet;
			client.state->state = READY_TO_FIGHT;
			this->_send(client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
			game.event.type = SokuLib::GAME_LOADED_ACK;
			this->_send(client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
		}
	} else if (packet.sceneId == SokuLib::SCENEID_CHARACTER_SELECT) {
		if (client.state->state == FIGHT) {
			SokuLib::PacketGame game{SokuLib::HOST_GAME};

			game.event.loaded = packet;
			if (!this->inBattle())
				this->_send(client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
			else for (auto &p : this->_state.players) {
				this->_send(*p.client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
				this->_send(*p.client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
				this->_send(*p.client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
				this->_send(*p.client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
				this->_send(*p.client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
				p.state = END_OF_FIGHT;
			}
			game.event.type = SokuLib::GAME_LOADED_ACK;
			this->_send(client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
			client.state->state = END_OF_FIGHT;
			client.state->frameIdOffset = 0;
			client.state->lastFrameId = CHARACTER_INPUT_DELAY;
			client.state->inputs.clear();
			client.state->inputs.resize(CHARACTER_INPUT_DELAY, {.raw = 0});
		} else if (client.state->state == JOINING) {
			SokuLib::PacketGame game{SokuLib::HOST_GAME};

			game.event.loaded = packet;
			this->_send(client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
			game.event.type = SokuLib::GAME_LOADED_ACK;
			this->_send(client, &game, sizeof(packet) + sizeof(SokuLib::PacketType));
		}
	}
}

void Server::_handlePacketGameLoadAck(Server::Client &client, SokuLib::GameLoadedEvent &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	if (packet.sceneId == SokuLib::SCENEID_BATTLE) {
		if (client.state->state == READY_TO_FIGHT) {
			client.state->state = FIGHT;
			client.state->frameIdOffset = 0;
			client.state->lastFrameId = BATTLE_INPUT_DELAY;
			client.state->inputs.clear();
			client.state->inputs.resize(BATTLE_INPUT_DELAY, {.raw = 0});
		}
	} else if (packet.sceneId == SokuLib::SCENEID_CHARACTER_SELECT) {
		if (client.state->state == JOINING) {
			client.state->state = SELECT_CHARACTER;
			client.state->frameIdOffset = 0;
			client.state->lastFrameId = 0;
			client.state->inputs.clear();
		}
	}
}

void Server::_handlePacketGame(Client &client, SokuLib::GameInputEvent &packet, size_t packetSize)
{
	if (packetSize < sizeof(packet))
		return;
	if (packetSize < sizeof(packet) + packet.inputCount * 2)
		return;
	if (client.state->state == END_OF_FIGHT && packet.sceneId == SokuLib::SCENEID_CHARACTER_SELECT)
		client.state->state = SELECT_CHARACTER;
	if (client.state->inputScene != packet.sceneId) {
		if (packet.sceneId == SokuLib::SCENEID_CHARACTER_SELECT) {
			client.state->frameIdOffset = 0;
			client.state->lastFrameId = CHARACTER_INPUT_DELAY;
			client.state->inputs.clear();
			client.state->inputs.resize(CHARACTER_INPUT_DELAY, {.raw = 0});
		} else {
			client.state->frameIdOffset = 0;
			client.state->lastFrameId = BATTLE_INPUT_DELAY;
			client.state->inputs.clear();
			client.state->inputs.resize(BATTLE_INPUT_DELAY, {.raw = 0});
		}
		client.state->inputScene = packet.sceneId;
	}
	if (client.state->state == SELECT_CHARACTER)
		this->_handleChrSelectInput(client, packet);
	else if (client.state->state == FIGHT)
		this->_handleBattleInput(client, packet);
}

void Server::_handleChrSelectInput(Server::Client &client, SokuLib::GameInputEvent &packet)
{
	char buffer[BUFFER_SIZE];
	auto game = (SokuLib::PacketGame *)buffer;

	if (packet.sceneId != SokuLib::SCENEID_CHARACTER_SELECT)
		return;
	if (this->endOfFight() && !this->inChrSelect() && this->allSlotsFilled())
		return;
	game->type = SokuLib::HOST_GAME;
	game->event.type = SokuLib::GAME_INPUT;
	game->event.input.sceneId = packet.sceneId;
	if (this->allSlotsFilled()) {
		for (int i = 0; i < packet.inputCount; i++)
			if (client.state->lastFrameId == packet.frameId + i + CHARACTER_INPUT_DELAY) {
				client.state->inputs.push_back(packet.inputs[i]);
				client.state->lastFrameId++;
			}

		unsigned lastFrame = client.state->lastFrameId;

		for (auto &player : this->_state.players)
			if (player.state == SELECT_CHARACTER)
				lastFrame = std::min(lastFrame, player.lastFrameId - player.frameIdOffset + client.state->frameIdOffset);
		if (lastFrame <= packet.frameId)
			return;
		client.internalTimer++;
		game->event.input.frameId = packet.frameId + 1;
		game->event.input.inputCount = (lastFrame - packet.frameId) * 4;
		for (int i = 0; i < game->event.input.inputCount; i += 4) {
			for (int j = 0; j < 4; j++) {
				auto &p = *this->_state.slots[j];

				if (p.state != SELECT_CHARACTER) {
					game->event.input.inputs[game->event.input.inputCount - 4 - i + j].raw = 0;
					game->event.input.inputs[game->event.input.inputCount - 4 - i + j].charSelect.Z = (client.internalTimer >> 2) & 1;
				} else
					game->event.input.inputs[game->event.input.inputCount - 4 - i + j] = p.getInput(lastFrame - client.state->frameIdOffset + p.frameIdOffset);
			}
		}
	} else {
		client.state->inputs.clear();
		client.state->inputs.resize(CHARACTER_INPUT_DELAY, {.raw = 0});
		client.state->frameIdOffset = packet.frameId + 1;
		client.state->lastFrameId = client.state->frameIdOffset + CHARACTER_INPUT_DELAY;
		game->event.input.frameId = packet.frameId + 1;
		game->event.input.inputCount = packet.inputCount * 4;
		for (int i = 0; i < game->event.input.inputCount; i += 4) {
			game->event.input.inputs[i + 0].raw = 0;
			game->event.input.inputs[i + 1].raw = 0;
			game->event.input.inputs[i + 2].raw = 0;
			game->event.input.inputs[i + 3].raw = 0;
		}
	}
	this->_send(client, game, sizeof(packet) + sizeof(SokuLib::PacketType) + sizeof(*packet.inputs) * game->event.input.inputCount);
}

void Server::_handleBattleInput(Server::Client &client, SokuLib::GameInputEvent &packet)
{
	char buffer[BUFFER_SIZE];
	auto game = (SokuLib::PacketGame *)buffer;

	if (packet.sceneId != SokuLib::SCENEID_BATTLE)
		return;
	if (!readyToFight())
		return;
	game->type = SokuLib::HOST_GAME;
	game->event.type = SokuLib::GAME_INPUT;
	game->event.input.sceneId = packet.sceneId;
	for (int i = 0; i < packet.inputCount; i++)
		if (client.state->lastFrameId == packet.frameId + i + BATTLE_INPUT_DELAY) {
			client.state->inputs.push_back(packet.inputs[i]);
			client.state->lastFrameId++;
		}

	unsigned lastFrame = client.state->lastFrameId;

	for (auto &player : this->_state.players)
		if (player.state == FIGHT)
			lastFrame = std::min(lastFrame, player.lastFrameId - player.frameIdOffset + client.state->frameIdOffset);
		else if (player.state == READY_TO_FIGHT)
			lastFrame = std::min(lastFrame, client.state->frameIdOffset + BATTLE_INPUT_DELAY);
	if (lastFrame <= packet.frameId)
		return;
	client.internalTimer++;
	game->event.input.frameId = packet.frameId + 1;
	game->event.input.inputCount = (lastFrame - packet.frameId) * 4;
	for (int i = 0; i < game->event.input.inputCount; i += 4) {
		for (int j = 0; j < 4; j++) {
			auto &p = *this->_state.slots[j];

			if (p.state == FIGHT || p.state == READY_TO_FIGHT) {
				auto index = p.frameIdOffset - client.state->frameIdOffset + ((lastFrame - i / 4) - (p.lastFrameId - (int)p.inputs.size()) - 1);

				game->event.input.inputs[game->event.input.inputCount - 4 - i + j] = p.inputs.at(index);
			} else
				game->event.input.inputs[game->event.input.inputCount - 4 - i + j].raw = 0;
		}
	}
	this->_send(client, game, sizeof(packet) + sizeof(SokuLib::PacketType) + sizeof(*packet.inputs) * game->event.input.inputCount);
}

void Server::_handlePacketGameMatchRequest(Client &client)
{
	if (this->readyToLoad()) {
		char buffer[sizeof(SokuLib::PacketType) + sizeof(SokuLib::GameType)];
		auto game = (SokuLib::PacketGame *) buffer;

		game->type = SokuLib::HOST_GAME;
		game->event.type = SokuLib::GAME_MATCH_ACK;
		this->_send(client, game, sizeof(SokuLib::PacketType) + sizeof(SokuLib::GameType));
	}
}

void Server::_handlePacketGameMatchAck(Client &client)
{
	if (this->readyToLoad()) {
		PacketGameMatchEvent game;

		game.opcode = SokuLib::HOST_GAME;
		game.type = SokuLib::GAME_MATCH;
		for (int i = 0; i < 4; i++) {
			auto &chr = game[i];
			auto &state = *this->_state.slots[i];

			chr.deckId = state.deckId;
			chr.skinId = state.palette;
			chr.character = state.chr;
			chr.deckSize = state.deck.size();
			memcpy(chr.cards, state.deck.data(), chr.deckSize * sizeof(*chr.cards));
			chr.disabledSimultaneousButton() = state.hasSimulButtons;
		}
		game.stageId() = this->_state.stage;
		game.musicId() = this->_state.music;
		game.randomSeed() = this->_state.randomSeed;
		game.matchId() = this->_state.matchId;
		this->_send(client, &game, game.getSize());
		client.state->state = LOADING;
	}
}

void Server::_handlePacketGame(Client &client, SokuLib::GameReplayRequestEvent &packet, size_t packetSize)
{
}

void Server::_onPlayerDisconnect(size_t index)
{
	assert(index < this->_state.players.size());
	std::cout << "Removing player " << index << " because of disconnect." << std::endl;

	PacketPlayerJoin response{PLAYER_JOIN, {0}, this->_state.players[index].client->slotId};

	memcpy(response.name, emptyMagicString, sizeof(response.name));
	this->_state.players.erase(this->_state.players.begin() + index);
	if (response.slot >= 0) {
		for (auto &player: this->_state.players) {
			this->_send(*player.client, &response, sizeof(response));
			this->_send(*player.client, &response, sizeof(response));
			this->_send(*player.client, &response, sizeof(response));
			this->_send(*player.client, &response, sizeof(response));
			this->_send(*player.client, &response, sizeof(response));
		}
		this->_state.slots[response.slot] = nullptr;
	}
}

void Server::_send(Server::Client &client, void *data, size_t size)
{
	auto &packet = *(CustomPacket *)data;

	if (
		(packet.type != HOST_GAME || packet.base.game.event.type != SokuLib::GAME_INPUT || packet.base.game.event.input.frameId == 1) &&
		packet.type != CHAIN
	) {
		std::cout << "[" << client.ip.toString() << ":" << client.port << ">]: ";
		displayPacketContent(std::cout, packet);
		std::cout << std::endl;
	}
	this->_sock.send(data, size, client.ip, client.port);
}

void Server::_disconnect(Server::Client &client)
{
	std::cout << client.ip.toString() << ":" << client.port << " disconnected." << std::endl;
	client.status = STATUS_DISCONNECTED;
	for (size_t i = 0; i < this->_state.players.size(); i++)
		if (&this->_state.players[i] == client.state) {
			client.state = nullptr;
			this->_onPlayerDisconnect(i);
			return;
		}
}

bool Server::allSlotsFilled() const
{
	return std::all_of(this->_state.slots.begin(), this->_state.slots.end(), [](void *p) { return p != nullptr; });
}

bool Server::readyToLoad() const
{
	return std::all_of(this->_state.players.begin(), this->_state.players.end(), [](const PlayerState &p){ return p.state >= READY_TO_LOAD && p.state <= READY_TO_FIGHT; });
}

bool Server::readyToFight() const
{
	return std::all_of(this->_state.players.begin(), this->_state.players.end(), [](const PlayerState &p){ return p.state >= READY_TO_FIGHT; });
}

bool Server::inBattle() const
{
	return std::all_of(this->_state.players.begin(), this->_state.players.end(), [](const PlayerState &p){ return p.state == FIGHT; });
}

bool Server::inChrSelect() const
{
	return std::all_of(this->_state.players.begin(), this->_state.players.end(), [](const PlayerState &p){ return p.state == SELECT_CHARACTER; });
}

bool Server::endOfFight() const
{
	return std::all_of(this->_state.players.begin(), this->_state.players.end(), [](const PlayerState &p){ return p.state == END_OF_FIGHT || p.state == SELECT_CHARACTER; });
}

SokuLib::Inputs Server::PlayerState::getInput(unsigned int frame)
{
#ifdef _DEBUG
	return this->inputs.at(frame - this->frameIdOffset - 1);
#else
	return this->inputs[frame - this->frameIdOffset - 1];
#endif
}
