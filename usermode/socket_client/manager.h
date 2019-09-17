#include <windows.h>
#include <cstdint>
#include "vector.h"
#include "offsets.h"
#include "driver.h"
#include <vector>

namespace manager {
	static uint32_t m_pid;
	static uintptr_t m_base;
	static SOCKET m_connection;

	unsigned long get_player_count()
	{
		uint64_t game_manager = driver::read<uint64_t>(m_connection, m_pid, m_base + OFFSET_GAME_MANAGER);

		if (!game_manager)
			return NULL;

		return driver::read<unsigned long>(m_connection, m_pid, game_manager + OFFSET_GAME_MANAGER_ENTITY_COUNT);
	}

	uint64_t get_player_by_id(unsigned int i)
	{
		uint64_t game_manager = driver::read<uint64_t>(m_connection, m_pid, m_base + OFFSET_GAME_MANAGER);

		if (!game_manager)
			return NULL;

		uint64_t entity_list = driver::read<uint64_t>(m_connection, m_pid, game_manager + OFFSET_GAME_MANAGER_ENTITY_LIST);

		if (!entity_list)
			return NULL;

		if (i > get_player_count())
			return NULL;

		uint64_t entity = driver::read<uint64_t>(m_connection, m_pid, entity_list + (sizeof(PVOID) * i));

		if (!entity)
			return NULL;

		return driver::read<uint64_t>(m_connection, m_pid, entity + OFFSET_STATUS_MANAGER_LOCALENTITY);
	}

	unsigned short get_player_team(uint64_t player)
	{
		if (!player)
			return 0xFF;

		uint64_t replication = driver::read<uint64_t>(m_connection, m_pid, player + OFFSET_ENTITY_REPLICATION);

		if (!replication)
			return 0xFF;

		unsigned long online_team_id = driver::read<unsigned long>(m_connection, m_pid, replication + OFFSET_ENTITY_REPLICATION_TEAM);
		return LOWORD(online_team_id);
	}

	uint64_t get_local_player()
	{
		uint64_t status_manager = driver::read<uint64_t>(m_connection, m_pid, m_base + OFFSET_STATUS_MANAGER);

		if (!status_manager)
			return NULL;

		uint64_t entity_container = driver::read<uint64_t>(m_connection, m_pid, status_manager + OFFSET_STATUS_MANAGER_CONTAINER);

		if (!entity_container)
			return NULL;

		entity_container = driver::read<uint64_t>(m_connection, m_pid, entity_container);

		if (!entity_container)
			return NULL;

		return driver::read<uint64_t>(m_connection, m_pid, entity_container + OFFSET_STATUS_MANAGER_LOCALENTITY);
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

		uint64_t component_chain = driver::read<uint64_t>(m_connection, m_pid, player + OFFSET_ENTITY_COMPONENT);

		if (!_VALID(component_chain))
			return NULL;

		uint64_t component_list = driver::read<uint64_t>(m_connection, m_pid, component_chain + OFFSET_ENTITY_COMPONENT_LIST);

		if (!_VALID(component_list))
			return NULL;

		for (unsigned int i = 15; i < 28; i++)
		{
			uint64_t component = driver::read<uint64_t>(m_connection, m_pid, component_list + i * sizeof(uint64_t));

			if (!_VALID(component))
				continue;

			const uint64_t vt_marker = ENTITY_MARKER_VT_OFFSET;

			uint64_t vt_table = driver::read<uint64_t>(m_connection, m_pid, component);
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
			driver::write<unsigned int>(m_connection, m_pid, marker + ENTITY_MARKER_ENABLED_OFFSET, 65793);

		return true;
	}

	bool outline()
	{
		uint64_t outline = driver::read<uint64_t>(m_connection, m_pid, m_base + 0x50660B0);

		if (!outline)
			return false;

		uint64_t teamo = driver::read<uint64_t>(m_connection, m_pid, outline + 0x88);
		uint64_t teamou = driver::read<uint64_t>(m_connection, m_pid, teamo + 0x38);
		uint64_t teamout = driver::read<uint64_t>(m_connection, m_pid, teamou + 0x68);

		driver::write<unsigned int>(m_connection, m_pid, teamout + 0x20, 5);

		return true;
	}

	bool no_recoil(uintptr_t local_player)
	{
		uintptr_t lpVisualCompUnk = driver::read<uintptr_t>(m_connection, m_pid, local_player + 0x98);

		if (!lpVisualCompUnk)
			return false;

		uintptr_t lpWeapon = driver::read<uintptr_t>(m_connection, m_pid, lpVisualCompUnk + 0xC8);

		if (!lpWeapon)
			return false;

		uintptr_t lpCurrentDisplayWeapon = driver::read<uintptr_t>(m_connection, m_pid, lpWeapon + 0x208);

		if (!lpCurrentDisplayWeapon)
			return false;

		float almost_zero = 0.002f;
		driver::write<float>(m_connection, m_pid, lpCurrentDisplayWeapon + 0x50, almost_zero);
		driver::write<float>(m_connection, m_pid, lpCurrentDisplayWeapon + 0xA0, almost_zero);

		return true;
	}
}
