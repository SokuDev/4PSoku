//
// Created by PinkySmile on 14/08/24.
//

#include <iostream>
#include <cstdint>
#include <thread>
#include "Server.hpp"

namespace SokuLib {
	std::vector<std::string> charactersName{
		"Reimu Hakurei",
		"Marisa Kirisame",
		"Sakuya Izayoi",
		"Alice Margatroid",
		"Patchouli Knowledge",
		"Youmu Konpaku",
		"Remilia Scarlet",
		"Yuyuko Saigyouji",
		"Yukari Yakumo",
		"Suika Ibuki",
		"Reisen Udongein Inaba",
		"Aya Shameimaru",
		"Komachi Onozuka",
		"Iku Nagae",
		"Tenshi Hinanawi",
		"Sanae Kochiya",
		"Cirno",
		"Hong Meiling",
		"Utsuho Reiuji",
		"Suwako Moriya",
		"Random Select",
		"Namazu",
		//Soku2 Characters
		"Momiji Inubashiri",
		"Clownpiece",
		"Flandre Scarlet",
		"Rin Kaenbyou",
		"Yuuka Kazami",
		"Kaguya Houraisan",
		"Fujiwara no Mokou",
		"Mima",
		"Shou Tormaru",
		"Minamitsu Murasa",
		"Sekibanki",
		"Satori Komeiji",
		"Ran Yakumo",
		"Tewi Inaba"
	};
}

int main(int argc, char **argv)
{
	if (argc != 2 && argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
		return EXIT_FAILURE;
	}

	uint64_t sleepTime = 5000;

#ifndef _DEBUG
	try {
#endif
		if (argc == 3)
			sleepTime = std::stoull(argv[2]);
		if (sleepTime > 1000000 / 60)
			std::cerr << "Warning: Sleep time value is higher than 1/60s. This may induce some lag during games." << std::endl;

		unsigned port = std::stoul(argv[1]);

		if (port > UINT16_MAX)
			throw std::invalid_argument("Invalid port");

		Server server{static_cast<unsigned short>(port)};

		while (true) {
			server.update();
			std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
		}
#ifndef _DEBUG
	} catch (std::exception &e) {
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
#endif
	return EXIT_SUCCESS;
}