#include "sdk.hpp"
#include "../offsets.hpp"

#include "../datamodel/datamodel.hpp"
#include "../../overlay/overlay.hpp"
#include "../globals/globals.hpp"
#include "../../features/aimbot/aimbot.hpp"
#include "../../features/esp/esp.hpp"

#include "../driver/driver.h"

#include "../../utils/json/json.hpp"

#pragma comment(lib, "libcurl.lib")
#include <fstream>
#include <thread>
#include <cstdlib> 

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
	size_t totalSize = size * nmemb;
	output->append(static_cast<char*>(contents), totalSize);
	return totalSize;
}

std::string drv_readstring(std::uint64_t address)
{
	std::string string;
	char character = 0;
	int char_size = sizeof(character);
	int offset = 0;

	string.reserve(204);

	while (offset < 200)
	{
		character = drv::read<char>(address + offset);
		if (character == 0) break;
		offset += char_size;
		string.push_back(character);
	}
	return string;
}

uint64_t get_datamodel() {
	uintptr_t scheduler = drv::read<uintptr_t>(drv::GetBase() + Offsets::TaskScheduler::Pointer);
	if (!scheduler) return 0;

	uintptr_t jobStart = drv::read<uintptr_t>(scheduler + Offsets::TaskScheduler::JobStart);
	uintptr_t jobEnd = drv::read<uintptr_t>(scheduler + Offsets::TaskScheduler::JobEnd);

	if (!jobStart || !jobEnd || jobStart >= jobEnd) return 0;

	for (uintptr_t job = jobStart; job < jobEnd; job += 0x10) {
		uintptr_t jobAddress = drv::read<uintptr_t>(job);
		if (!jobAddress) continue;

		std::string jobName = drv_readstring(jobAddress + Offsets::TaskScheduler::JobName);

		if (jobName == "RenderJob") {
			auto RenderView = drv::read<uintptr_t>(jobAddress + Offsets::RenderJob::RenderView);
			if (!RenderView) continue;

			globals::visualengine.self = drv::read<uintptr_t>(drv::GetBase() + Offsets::VisualEngine::Pointer);

			uintptr_t FakeDataModel = drv::read<uintptr_t>(drv::GetBase() + Offsets::FakeDataModel::Pointer);
			if (!FakeDataModel) continue;

			auto realDataModel = drv::read<uintptr_t>(FakeDataModel + Offsets::FakeDataModel::RealDataModel);
			if (!realDataModel) continue;

			return realDataModel;
		}
	}
	return 0;
}

bool silence::roblox::init()
{
	printf(("silence > Waiting for RBX::GAME...\n"));

	while (true) {
		drv::procid = drv::FindProcess("RobloxPlayerBeta.exe");
		if (drv::procid) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	if (!drv::Init()) {
		std::perror(("silence > driver: failed to initialize usermode driver\n"));
		return false;
	}

	printf(("silence > Finding RBX::DataModel...\n"));

	uint64_t game_ptr = 0;
	while (!game_ptr) {
		game_ptr = get_datamodel();
		if (!game_ptr) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	auto game = static_cast<silence::roblox::instance_t>(game_ptr);

	std::cout << std::hex << "silence > RBX::DataModel -> 0x" << game.self << std::endl;

	globals::datamodel = game;

	silence::roblox::instance_t players;
	players.self = 0;
	while (!players.self) {
		players = game.find_first_child(("Players"));
		if (!players.self) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	globals::players = players;

	std::string teamname;

	silence::roblox::instance_t team = players.get_local_player().get_team();

	if (team.self)
	{
		teamname = team.name();
	}
	else
	{
		teamname = ("none");
	}

	auto placeid =
		static_cast<silence::roblox::instance_t>(silence::utils::datamodel::get_place_id());

	if (!placeid.self)

		globals::placeid = placeid;

	std::thread(silence::roblox::hook_aimbot).detach();

	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(10));

		if (FindWindowA("WINDOWSCLIENT", 0) == NULL) {
			exit(0);
		}
	}

	return true;
}

silence::roblox::vector2_t silence::roblox::world_to_screen(silence::roblox::vector3_t world, silence::roblox::vector2_t dimensions, silence::roblox::matrix4_t viewmatrix)
{
	silence::roblox::quaternion quaternion;

	quaternion.x = (world.x * viewmatrix.data[0]) + (world.y * viewmatrix.data[1]) + (world.z * viewmatrix.data[2]) + viewmatrix.data[3];
	quaternion.y = (world.x * viewmatrix.data[4]) + (world.y * viewmatrix.data[5]) + (world.z * viewmatrix.data[6]) + viewmatrix.data[7];
	quaternion.z = (world.x * viewmatrix.data[8]) + (world.y * viewmatrix.data[9]) + (world.z * viewmatrix.data[10]) + viewmatrix.data[11];
	quaternion.w = (world.x * viewmatrix.data[12]) + (world.y * viewmatrix.data[13]) + (world.z * viewmatrix.data[14]) + viewmatrix.data[15];

	if (quaternion.w < 0.1f)
		return{ -1, -1 };

	float inv_w = 1.0f / quaternion.w;
	vector3_t ndc;
	ndc.x = quaternion.x * inv_w;
	ndc.y = quaternion.y * inv_w;
	ndc.z = quaternion.z * inv_w;

	return
	{
		(dimensions.x / 2 * ndc.x) + (ndc.x + dimensions.x / 2),
		-(dimensions.y / 2 * ndc.y) + (ndc.y + dimensions.y / 2)
	};
}

std::string readstring(std::uint64_t address)
{
	std::string string;
	char character = 0;
	int char_size = sizeof(character);
	int offset = 0;

	string.reserve(204);

	while (offset < 200)
	{
		character = drv::read<char>(address + offset);

		if (character == 0)
			break;

		offset += char_size;
		string.push_back(character);
	}

	return string;
}

std::string readstring2(std::uint64_t string)
{
	const auto length = drv::read<int>(string + 0x18);

	if (length >= 16u)
	{
		const auto New = drv::read<std::uint64_t>(string);
		return readstring(New);
	}
	else
	{
		const auto Name = readstring(string);
		return Name;
	}
}

std::string silence::roblox::instance_t::name()
{
	const auto ptr = drv::read<std::uint64_t>(this->self + Offsets::Instance::Name);

	if (ptr)
		return readstring2(ptr);

	return ("???");
}

std::string silence::roblox::instance_t::class_name()
{
	const auto ptr = drv::read<std::uint64_t>(this->self + Offsets::Instance::ClassName);

	if (ptr)
		return readstring2(ptr + Offsets::Instance::This);

	return ("???_classname");
}

std::vector<silence::roblox::instance_t> silence::roblox::instance_t::children()
{
	std::vector<silence::roblox::instance_t> container;

	if (!this->self)
		return container;

	auto start = drv::read<std::uint64_t>(this->self + Offsets::Instance::ChildrenStart);

	if (!start)
		return container;

	auto end = drv::read<std::uint64_t>(start + Offsets::Instance::ChildrenEnd);

	for (auto instances = drv::read<std::uint64_t>(start); instances != end; instances += 16)
		container.emplace_back(drv::read<silence::roblox::instance_t>(instances));

	return container;
}

silence::roblox::instance_t silence::roblox::instance_t::find_first_child(std::string child)
{
	silence::roblox::instance_t ret;

	for (auto& object : this->children())
	{
		if (object.name() == child)
		{
			ret = static_cast<silence::roblox::instance_t>(object);
			break;
		}
	}

	return ret;
}

void silence::roblox::instance_t::set_humanoid_walkspeed(float value)
{
	auto humanoid_instance = this->get_model_instance().find_first_child("Humanoid");

	if (humanoid_instance.self) {
		drv::write<float>(humanoid_instance.self + Offsets::Humanoid::Walkspeed, value);
	}
}

float silence::roblox::instance_t::get_ping()
{
	float ping = drv::read<float>(this->self + 0xCC);
	return ping * 2000;
}

silence::roblox::instance_t silence::roblox::instance_t::get_local_player()
{
	auto local_player = drv::read<silence::roblox::instance_t>(this->self + Offsets::Player::LocalPlayer);
	return local_player;
}

silence::roblox::instance_t silence::roblox::instance_t::get_model_instance()
{
	auto character = drv::read<silence::roblox::instance_t>(this->self + Offsets::Player::ModelInstance);
	return character;
}

silence::roblox::vector2_t silence::roblox::instance_t::get_dimensions()
{
	auto dimensions = drv::read<silence::roblox::vector2_t>(this->self + Offsets::VisualEngine::Dimensions);
	return dimensions;
}

silence::roblox::matrix4_t silence::roblox::instance_t::get_view_matrix()
{
	auto dimensions = drv::read<silence::roblox::matrix4_t>(this->self + Offsets::VisualEngine::ViewMatrix);
	return dimensions;
}

silence::roblox::vector3_t silence::roblox::instance_t::get_camera_pos()
{
	auto camera_pos = drv::read<silence::roblox::vector3_t>(this->self + Offsets::Camera::Position);
	return camera_pos;
}

silence::roblox::vector3_t silence::roblox::instance_t::get_part_pos()
{
	vector3_t res{};

	auto primitive = drv::read<std::uint64_t>(this->self + Offsets::BasePart::Primitive);

	if (!primitive)
		return res;

	res = drv::read<silence::roblox::vector3_t>(primitive + Offsets::Primitive::Position);
	return res;
}

silence::roblox::vector3_t silence::roblox::instance_t::get_part_velocity()
{
	vector3_t res{};

	auto primitive = drv::read<std::uint64_t>(this->self + Offsets::BasePart::Primitive);

	if (!primitive)
		return res;

	res = drv::read<silence::roblox::vector3_t>(primitive + Offsets::Primitive::AssemblyLinearVelocity);
	return res;
}

union convertion
{
	std::uint64_t hex;
	float f;
} conv;

float silence::roblox::instance_t::get_health()
{
	auto one = drv::read<std::uint64_t>(this->self + Offsets::Humanoid::Health);
	auto two = drv::read<std::uint64_t>(drv::read<std::uint64_t>(this->self + Offsets::Humanoid::Health));

	conv.hex = one ^ two;
	return conv.f;
}

float silence::roblox::instance_t::get_max_health()
{
	auto one = drv::read<std::uint64_t>(this->self + Offsets::Humanoid::MaxHealth);
	auto two = drv::read<std::uint64_t>(drv::read<std::uint64_t>(this->self + Offsets::Humanoid::MaxHealth));

	conv.hex = one ^ two;
	return conv.f;
}

silence::roblox::instance_t silence::roblox::instance_t::get_team()
{
	auto getteam = drv::read<silence::roblox::instance_t>(this->self + Offsets::Player::Team);
	return getteam;
}

std::uintptr_t silence::roblox::instance_t::get_gameid()
{
	auto gameid = drv::read<std::uintptr_t>(this->self + Offsets::DataModel::GameId);
	return gameid;
}