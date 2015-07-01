#ifndef ENEMYBOT_H
#define ENEMYBOT_H

#include <vector>
#include "kf/kf_vector2.h"

class EnemyBot
{
public:
	EnemyBot(std::string id);
	kf::Vector2 getPos();
	void setPos(kf::Vector2 pos);
	void update();

	kf::Vector2 getExpectedPosition(int ticksAhead, float distance);
	float getDistanceToPostion(kf::Vector2 pos);

	std::string m_botID;
	int m_invisible; //Number of ticks since seeing this enemy. This is reset when the target becomes visible. (setPos)
	std::vector<kf::Vector2> m_scannedPositions;
private:
	kf::Vector2 m_pos;
	
	//Tweakables
	float m_distScalar = 0.12f; //Mulitiplied by distance to adjust shot location.
	float m_tickScalar = 0.0f; //Multiplied by ticks to adjust shot location.
};

#endif