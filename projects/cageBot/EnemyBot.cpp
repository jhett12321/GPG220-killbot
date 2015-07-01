#include "EnemyBot.h"

EnemyBot::EnemyBot(std::string id)
{
	m_botID = id;
}

kf::Vector2 EnemyBot::getPos()
{
	return m_pos;
}

void EnemyBot::update()
{
	m_invisible++;
}

void EnemyBot::setPos(kf::Vector2 pos)
{
	m_pos = pos;
	m_scannedPositions.push_back(pos);
	if (m_scannedPositions.size() > 5)
	{
		m_scannedPositions.erase(m_scannedPositions.begin());
	}
	m_invisible = 0;
}

kf::Vector2 EnemyBot::getExpectedPosition(int ticksAhead, float distance)
{
	if (m_scannedPositions.size() > 1)
	{
		float tickScalar = 1 + ticksAhead * m_tickScalar;
		float distScalar = 1 + distance * m_distScalar;

		return (m_scannedPositions[m_scannedPositions.size() - 1] - m_scannedPositions[m_scannedPositions.size() - 2]) * tickScalar * distScalar + m_scannedPositions[m_scannedPositions.size() - 1];
	}
	else
	{
		return m_pos;
	}
}

float EnemyBot::getDistanceToPostion(kf::Vector2 pos)
{
	float diffY = m_pos.y - pos.y;
	float diffX = m_pos.x - pos.x;
	return sqrt((diffY * diffY) + (diffX * diffX));
}