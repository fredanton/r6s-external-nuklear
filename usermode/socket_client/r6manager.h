#include <windows.h>
#include <cstdint>
#include "vector.h"
#include "offsets.h"
#include "driver.h"
#include <vector>

class r6manager
{
public:
	static uint32_t m_pid;
	static uintptr_t m_base;
	static SOCKET m_connection;

	r6manager() { }

	template<typename T>
	T read(uint64_t address)
	{
		T buffer = NULL;

		if (!this->m_pid)
			return buffer;

		buffer = driver::read<T>(m_connection, m_base, address);

		if (result != 0)
			return NULL;

		return buffer;
	}

	template<typename T>
	bool write(uint64_t address, T buffer)
	{
		const auto result = driver::write<T>(m_connection, m_pid, address, uint64_t(&buffer), sizeof(T));

		if (result != 0)
			return false;

		return true;
	}

	unsigned long get_player_count()
	{
		uint64_t game_manager = read<uint64_t>(m_base + OFFSET_GAME_MANAGER);

		if (!game_manager)
			return NULL;

		return read<unsigned long>(game_manager + OFFSET_GAME_MANAGER_ENTITY_COUNT);
	}

	uint64_t get_player_by_id(unsigned int i)
	{
		uint64_t game_manager = read<uint64_t>(m_base + OFFSET_GAME_MANAGER);

		if (!game_manager)
			return NULL;

		uint64_t entity_list = read<uint64_t>(game_manager + OFFSET_GAME_MANAGER_ENTITY_LIST);

		if (!entity_list)
			return NULL;

		if (i > get_player_count())
			return NULL;

		uint64_t entity = read<uint64_t>(entity_list + (sizeof(PVOID) * i));

		if (!entity)
			return NULL;

		return read<uint64_t>(entity + OFFSET_STATUS_MANAGER_LOCALENTITY);
	}

	unsigned short get_player_team(uint64_t player)
	{
		if (!player)
			return 0xFF;

		uint64_t replication = read<uint64_t>(player + OFFSET_ENTITY_REPLICATION);

		if (!replication)
			return 0xFF;

		unsigned long online_team_id = read<unsigned long>(replication + OFFSET_ENTITY_REPLICATION_TEAM);
		return LOWORD(online_team_id);
	}

	uint64_t get_local_player()
	{
		uint64_t status_manager = read<uint64_t>(m_base + OFFSET_STATUS_MANAGER);

		if (!status_manager)
			return NULL;

		uint64_t entity_container = read<uint64_t>(status_manager + OFFSET_STATUS_MANAGER_CONTAINER);

		if (!entity_container)
			return NULL;

		entity_container = read<uint64_t>(entity_container);

		if (!entity_container)
			return NULL;

		return read<uint64_t>(entity_container + OFFSET_STATUS_MANAGER_LOCALENTITY);
	}

	bool get_enemy_players(std::vector<uint64_t>& players)
	{
		uint64_t local_player = get_local_player();

		if (!_VALID(local_player))
			return FALSE;

		unsigned short local_team = get_player_team(local_player);

		unsigned int count = get_player_count();

		if (count > 255)
			return false;

		for (unsigned int i = 0; i < count; i++)
		{
			uint64_t target_player = get_player_by_id(i);

			if (!_VALID(target_player))
				continue;

			if (target_player == local_player)
				continue;

			if (get_player_team(target_player) == local_team)
				continue;

			players.push_back(target_player);
		}

		return true;
	}

	bool get_team_players(std::vector<uint64_t>& players)
	{
		uint64_t local_player = get_local_player();

		if (!_VALID(local_player))
			return FALSE;

		unsigned short local_team = get_player_team(local_player);

		unsigned int count = get_player_count();

		if (count > 255)
			return false;

		for (unsigned int i = 0; i < count; i++)
		{
			uint64_t target_player = get_player_by_id(i);

			if (!_VALID(target_player))
				continue;

			if (target_player == local_player)
				continue;

			if (get_player_team(target_player) != local_team)
				continue;

			players.push_back(target_player);
		}

		return true;
	}

	uint64_t get_spotted_marker(uint64_t player)
	{
		if (!_VALID(player))
			return FALSE;

		uint64_t component_chain = read<uint64_t>(player + OFFSET_ENTITY_COMPONENT);

		if (!_VALID(component_chain))
			return NULL;

		uint64_t component_list = read<uint64_t>(component_chain + OFFSET_ENTITY_COMPONENT_LIST);

		if (!_VALID(component_list))
			return NULL;

		for (unsigned int i = 15; i < 28; i++)
		{
			uint64_t component = read<uint64_t>(component_list + i * sizeof(uint64_t));

			if (!_VALID(component))
				continue;

			const uint64_t vt_marker = ENTITY_MARKER_VT_OFFSET;

			uint64_t vt_table = read<uint64_t>(component);
			uint64_t vt_offset = vt_table - m_base;

			if (vt_offset == vt_marker)
				return component;
		}

		return NULL;
	}

	bool esp()
	{
		std::vector<uint64_t> enemy_players;

		if (!get_enemy_players(enemy_players))
			return false;

		std::vector<uint64_t> enemy_marker_components;

		for (uint64_t player : enemy_players)
		{
			uint64_t marker_component = get_spotted_marker(player);

			if (_VALID(marker_component))
				enemy_marker_components.push_back(marker_component);
		}

		for (uint64_t marker : enemy_marker_components)
			write<unsigned int>(marker + ENTITY_MARKER_ENABLED_OFFSET, 65793);

		return true;
	}

	bool outline()
	{
		uint64_t outline = read<uint64_t>(m_base + 0x50660B0);

		if (!outline)
			return false;

		uint64_t teamo = read<uint64_t>(outline + 0x88);
		uint64_t teamou = read<uint64_t>(teamo + 0x38);
		uint64_t teamout = read<uint64_t>(teamou + 0x68);

		write<unsigned int>(teamout + 0x20, 5);

		return true;
	}

	bool no_recoil(uintptr_t local_player)
	{
		uintptr_t lpVisualCompUnk = read<uintptr_t>(local_player + 0x98);

		if (!lpVisualCompUnk)
			return false;

		uintptr_t lpWeapon = read<uintptr_t>(lpVisualCompUnk + 0xC8);

		if (!lpWeapon)
			return false;

		uintptr_t lpCurrentDisplayWeapon = read<uintptr_t>(lpWeapon + 0x208);

		if (!lpCurrentDisplayWeapon)
			return false;

		float almost_zero = 0.002f;
		write<float>(lpCurrentDisplayWeapon + 0x50, almost_zero);
		write<float>(lpCurrentDisplayWeapon + 0xA0, almost_zero);

		return true;
	}
};