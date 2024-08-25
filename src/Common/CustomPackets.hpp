//
// Created by PinkySmile on 15/08/24.
//

#ifndef INC_4PSOKU_CUSTOMPACKETS_HPP
#define INC_4PSOKU_CUSTOMPACKETS_HPP


#include <Packet.hpp>

#pragma pack(push, 1)
enum CustomOpcodes : unsigned char {
	//Base game opcodes
	HELLO = 0x01,
	PUNCH,
	OLLEH,
	CHAIN,
	INIT_REQUEST,
	INIT_SUCCESS,
	INIT_ERROR,
	REDIRECT,
	PLAYER_JOIN,
	PLAYER_JOIN_ACK,
	QUIT,
	UNLOCK_CHAR_SELECT,
	HOST_GAME,
	CLIENT_GAME,
	LOADING_READY,
	_15
};

struct PacketPlayerJoin {
	CustomOpcodes type;
	char name[32] = {0};
	char slot;
};

struct PacketPlayerJoinAck {
	CustomOpcodes type;
	char slot;
};

struct PacketInitSucc {
	SokuLib::PacketType type;
	uint8_t unknown1[8];
	uint16_t dataSize;
	uint8_t unknown2[2];
	char p1Name[32];
	char p2Name[32];
	uint32_t swrDisabled;
	char p3Name[32];
	char p4Name[32];
};

struct PacketLoadingReady {
	CustomOpcodes type;
	SokuLib::CharacterPacked chr;
	unsigned char stage;
	unsigned char music;
	unsigned char skinId;
	unsigned char deckId;
	bool hasSimulButtons;
	uint8_t deckSize;
	unsigned short deck[0];
};

struct PacketGameMatchEvent {
	SokuLib::PacketType opcode;
	SokuLib::GameType type;
	uint8_t data[sizeof(SokuLib::PlayerMatchData) * 4 + 256 * 4 + 7];

	SokuLib::PlayerMatchData &operator[](int i)
	{
		if (i == 0)
			return *(SokuLib::PlayerMatchData *)this->data;
		return *(SokuLib::PlayerMatchData *)(*this)[i - 1].getEndPtr();
	}

	uint8_t &stageId()
	{
		return *(*this)[3].getEndPtr();
	}

	uint8_t &musicId()
	{
		return (*this)[3].getEndPtr()[1];
	}

	uint32_t &randomSeed()
	{
		return *reinterpret_cast<uint32_t *>(&(*this)[3].getEndPtr()[2]);
	}

	uint8_t &matchId()
	{
		return (*this)[3].getEndPtr()[6];
	}

	size_t getSize() const
	{
		return ((ptrdiff_t)&this->matchId() + 1) - (ptrdiff_t)this;
	}

	const SokuLib::PlayerMatchData &operator[](int i) const
	{
		if (i == 0)
			return *(SokuLib::PlayerMatchData *)this->data;
		return *(SokuLib::PlayerMatchData *)(*this)[i - 1].getEndPtr();
	}

	const uint8_t &stageId() const
	{
		return *(*this)[3].getEndPtr();
	}

	const uint8_t &musicId() const
	{
		return (*this)[3].getEndPtr()[1];
	}

	const uint32_t &randomSeed() const
	{
		return *reinterpret_cast<const uint32_t *>(&(*this)[3].getEndPtr()[2]);
	}

	const uint8_t &matchId() const
	{
		return (*this)[3].getEndPtr()[6];
	}
};

union CustomPacket {
	CustomOpcodes type;
	SokuLib::Packet base;
	PacketPlayerJoin playerJoin;
	PacketLoadingReady loadingReady;
	PacketPlayerJoinAck playerJoinAck;
	PacketGameMatchEvent gameMatchEvent;
};
#pragma pack(pop)

void displayPacketContent(std::ostream &stream, const CustomPacket &packet);


#endif //INC_4PSOKU_CUSTOMPACKETS_HPP
