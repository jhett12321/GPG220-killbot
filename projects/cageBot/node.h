#ifndef NODE_H
#define NODE_H

#include <vector>
#include "kf/kf_vector2.h"

class Node
{
public:
	Node(int posx, int posy, bool isWall)
	{
		m_posx = posx;
		m_posy = posy;
		m_isWall = isWall;
	}
	int getX()
	{
		return m_posx;
	}
	int getY()
	{
		return m_posy;
	}
	bool isWall()
	{
		return m_isWall;
	}

	void setX(int x);
	void setY(int x);

	Node* m_parent;

	int m_posx;
	int m_posy;

	int m_fCost;
	int m_gCost;
	int m_hCost;

	bool m_open;
	bool m_closed;

	bool m_isWall;

private:

};

#endif