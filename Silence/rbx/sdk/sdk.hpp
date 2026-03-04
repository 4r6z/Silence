#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace silence
{
	namespace roblox
	{
		bool init();

		struct vector2_t final { float x, y; };
		struct vector3_t final { float x, y, z; };
		struct quaternion final { float x, y, z, w; };
		struct matrix4_t final { float data[16]; };

		class instance_t final
		{
		public:

			std::uint64_t self;

			std::string name();
			std::string class_name();
			std::vector<silence::roblox::instance_t> children();
			silence::roblox::instance_t find_first_child(std::string child);

			silence::roblox::instance_t get_local_player();
			silence::roblox::instance_t get_model_instance();
			silence::roblox::instance_t get_team();

			std::uintptr_t get_gameid();

			silence::roblox::vector2_t get_dimensions();
			silence::roblox::matrix4_t get_view_matrix();
			silence::roblox::vector3_t get_camera_pos();
			silence::roblox::vector3_t get_part_pos();
			silence::roblox::vector3_t get_part_velocity();

			float get_health();
			float get_max_health();

			void set_humanoid_walkspeed(float value);

			float get_ping();
		};

		silence::roblox::vector2_t world_to_screen(silence::roblox::vector3_t world, silence::roblox::vector2_t dimensions, silence::roblox::matrix4_t viewmatrix);
	}
}