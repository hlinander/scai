#ifndef __AI_STRUCTS_H_DEF__
#define __AI_STRUCTS_H_DEF__

#include <vector>
#include <cereal/types/vector.hpp>

enum Param {
	TEAM_HP = 0,
	TEAM_COUNT,
	TEAM_DISTANCE,
	TEAM_MINERALS,
	ENEMY_DISTANCE,
	ENEMY_COUNT,
	ENEMY_HP,
	ME_HP,
	ME_ATTACKED,
	ME_REPAIRED,
	MAX_PARAM
};

enum Action {
	ATTACK = 0,
	REPAIR,
	FLEE,
	MAX_ACTION
};

struct State {
	float data[Param::MAX_PARAM];
};

struct Model {
	Model()
	{
		params.resize(Action::MAX_ACTION);
		for (auto &ref : params) {
			ref.resize(Param::MAX_PARAM);
		}
	}

	template <class Archive>
	void serialize(Archive &a)
	{
		a(cereal::make_nvp("params", params));
	}

	std::vector< std::vector<float> > params;
};

#endif // __AI_STRUCTS_H_DEF__

